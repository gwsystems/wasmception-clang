//===--- SemaInternal.h - Internal Sema Interfaces --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides common API and #includes for the internal
// implementation of Sema.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_SEMAINTERNAL_H
#define LLVM_CLANG_SEMA_SEMAINTERNAL_H

#include "clang/AST/ASTContext.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"

namespace clang {

inline PartialDiagnostic Sema::PDiag(unsigned DiagID) {
  return PartialDiagnostic(DiagID, Context.getDiagAllocator());
}

inline bool
FTIHasSingleVoidParameter(const DeclaratorChunk::FunctionTypeInfo &FTI) {
  return FTI.NumParams == 1 && !FTI.isVariadic &&
         FTI.Params[0].Ident == nullptr && FTI.Params[0].Param &&
         cast<ParmVarDecl>(FTI.Params[0].Param)->getType()->isVoidType();
}

inline bool
FTIHasNonVoidParameters(const DeclaratorChunk::FunctionTypeInfo &FTI) {
  // Assume FTI is well-formed.
  return FTI.NumParams && !FTIHasSingleVoidParameter(FTI);
}

// This requires the variable to be non-dependent and the initializer
// to not be value dependent.
inline bool IsVariableAConstantExpression(VarDecl *Var, ASTContext &Context) {
  const VarDecl *DefVD = nullptr;
  return !isa<ParmVarDecl>(Var) &&
    Var->isUsableInConstantExpressions(Context) &&
    Var->getAnyInitializer(DefVD) && DefVD->checkInitIsICE(); 
}

// Directly mark a variable odr-used. Given a choice, prefer to use 
// MarkVariableReferenced since it does additional checks and then 
// calls MarkVarDeclODRUsed.
// If the variable must be captured:
//  - if FunctionScopeIndexToStopAt is null, capture it in the CurContext
//  - else capture it in the DeclContext that maps to the 
//    *FunctionScopeIndexToStopAt on the FunctionScopeInfo stack.  
inline void MarkVarDeclODRUsed(VarDecl *Var,
    SourceLocation Loc, Sema &SemaRef,
    const unsigned *const FunctionScopeIndexToStopAt) {
  // Keep track of used but undefined variables.
  // FIXME: We shouldn't suppress this warning for static data members.
  if (Var->hasDefinition(SemaRef.Context) == VarDecl::DeclarationOnly &&
    !Var->isExternallyVisible() &&
    !(Var->isStaticDataMember() && Var->hasInit())) {
      SourceLocation &old = SemaRef.UndefinedButUsed[Var->getCanonicalDecl()];
      if (old.isInvalid()) old = Loc;
  }
  QualType CaptureType, DeclRefType;
  SemaRef.tryCaptureVariable(Var, Loc, Sema::TryCapture_Implicit, 
    /*EllipsisLoc*/ SourceLocation(),
    /*BuildAndDiagnose*/ true, 
    CaptureType, DeclRefType, 
    FunctionScopeIndexToStopAt);

  Var->markUsed(SemaRef.Context);
}

/// Return a DLL attribute from the declaration.
inline InheritableAttr *getDLLAttr(Decl *D) {
  assert(!(D->hasAttr<DLLImportAttr>() && D->hasAttr<DLLExportAttr>()) &&
         "A declaration cannot be both dllimport and dllexport.");
  if (auto *Import = D->getAttr<DLLImportAttr>())
    return Import;
  if (auto *Export = D->getAttr<DLLExportAttr>())
    return Export;
  return nullptr;
}

class TypoCorrectionConsumer : public VisibleDeclConsumer {
  typedef SmallVector<TypoCorrection, 1> TypoResultList;
  typedef llvm::StringMap<TypoResultList> TypoResultsMap;
  typedef std::map<unsigned, TypoResultsMap> TypoEditDistanceMap;

public:
  TypoCorrectionConsumer(Sema &SemaRef,
                         const DeclarationNameInfo &TypoName,
                         Sema::LookupNameKind LookupKind,
                         Scope *S, CXXScopeSpec *SS,
                         std::unique_ptr<CorrectionCandidateCallback> CCC,
                         DeclContext *MemberContext,
                         bool EnteringContext)
      : Typo(TypoName.getName().getAsIdentifierInfo()), CurrentTCIndex(0),
        SemaRef(SemaRef), S(S), SS(SS), CorrectionValidator(std::move(CCC)),
        MemberContext(MemberContext), Result(SemaRef, TypoName, LookupKind),
        Namespaces(SemaRef.Context, SemaRef.CurContext, SS),
        EnteringContext(EnteringContext), SearchNamespaces(false) {
    Result.suppressDiagnostics();
    // Arrange for ValidatedCorrections[0] to always be an empty correction.
    ValidatedCorrections.push_back(TypoCorrection());
  }

  bool includeHiddenDecls() const override { return true; }

  // Methods for adding potential corrections to the consumer.
  void FoundDecl(NamedDecl *ND, NamedDecl *Hiding, DeclContext *Ctx,
                 bool InBaseClass) override;
  void FoundName(StringRef Name);
  void addKeywordResult(StringRef Keyword);
  void addCorrection(TypoCorrection Correction);

  bool empty() const {
    return CorrectionResults.empty() && ValidatedCorrections.size() == 1;
  }

  /// \brief Return the list of TypoCorrections for the given identifier from
  /// the set of corrections that have the closest edit distance, if any.
  TypoResultList &operator[](StringRef Name) {
    return CorrectionResults.begin()->second[Name];
  }

  /// \brief Return the edit distance of the corrections that have the
  /// closest/best edit distance from the original typop.
  unsigned getBestEditDistance(bool Normalized) {
    if (CorrectionResults.empty())
      return (std::numeric_limits<unsigned>::max)();

    unsigned BestED = CorrectionResults.begin()->first;
    return Normalized ? TypoCorrection::NormalizeEditDistance(BestED) : BestED;
  }

  /// \brief Set-up method to add to the consumer the set of namespaces to use
  /// in performing corrections to nested name specifiers. This method also
  /// implicitly adds all of the known classes in the current AST context to the
  /// to the consumer for correcting nested name specifiers.
  void
  addNamespaces(const llvm::MapVector<NamespaceDecl *, bool> &KnownNamespaces);

  /// \brief Return the next typo correction that passes all internal filters
  /// and is deemed valid by the consumer's CorrectionCandidateCallback,
  /// starting with the corrections that have the closest edit distance. An
  /// empty TypoCorrection is returned once no more viable corrections remain
  /// in the consumer.
  const TypoCorrection &getNextCorrection();

  /// \brief Get the last correction returned by getNextCorrection().
  const TypoCorrection &getCurrentCorrection() {
    return CurrentTCIndex < ValidatedCorrections.size()
               ? ValidatedCorrections[CurrentTCIndex]
               : ValidatedCorrections[0];  // The empty correction.
  }

  /// \brief Reset the consumer's position in the stream of viable corrections
  /// (i.e. getNextCorrection() will return each of the previously returned
  /// corrections in order before returning any new corrections).
  void resetCorrectionStream() {
    CurrentTCIndex = 0;
  }

  ASTContext &getContext() const { return SemaRef.Context; }
  const LookupResult &getLookupResult() const { return Result; }

private:
  class NamespaceSpecifierSet {
    struct SpecifierInfo {
      DeclContext* DeclCtx;
      NestedNameSpecifier* NameSpecifier;
      unsigned EditDistance;
    };

    typedef SmallVector<DeclContext*, 4> DeclContextList;
    typedef SmallVector<SpecifierInfo, 16> SpecifierInfoList;

    ASTContext &Context;
    DeclContextList CurContextChain;
    std::string CurNameSpecifier;
    SmallVector<const IdentifierInfo*, 4> CurContextIdentifiers;
    SmallVector<const IdentifierInfo*, 4> CurNameSpecifierIdentifiers;
    bool isSorted;

    SpecifierInfoList Specifiers;
    llvm::SmallSetVector<unsigned, 4> Distances;
    llvm::DenseMap<unsigned, SpecifierInfoList> DistanceMap;

    /// \brief Helper for building the list of DeclContexts between the current
    /// context and the top of the translation unit
    static DeclContextList buildContextChain(DeclContext *Start);

    void sortNamespaces();

    unsigned buildNestedNameSpecifier(DeclContextList &DeclChain,
                                      NestedNameSpecifier *&NNS);

   public:
    NamespaceSpecifierSet(ASTContext &Context, DeclContext *CurContext,
                          CXXScopeSpec *CurScopeSpec);

    /// \brief Add the DeclContext (a namespace or record) to the set, computing
    /// the corresponding NestedNameSpecifier and its distance in the process.
    void addNameSpecifier(DeclContext *Ctx);

    typedef SpecifierInfoList::iterator iterator;
    iterator begin() {
      if (!isSorted) sortNamespaces();
      return Specifiers.begin();
    }
    iterator end() { return Specifiers.end(); }
  };

  void addName(StringRef Name, NamedDecl *ND,
               NestedNameSpecifier *NNS = nullptr, bool isKeyword = false);

  /// \brief Find any visible decls for the given typo correction candidate.
  /// If none are found, it to the set of candidates for which qualified lookups
  /// will be performed to find possible nested name specifier changes.
  bool resolveCorrection(TypoCorrection &Candidate);

  /// \brief Perform qualified lookups on the queued set of typo correction
  /// candidates and add the nested name specifier changes to each candidate if
  /// a lookup succeeds (at which point the candidate will be returned to the
  /// main pool of potential corrections).
  void performQualifiedLookups();

  /// \brief The name written that is a typo in the source.
  IdentifierInfo *Typo;

  /// \brief The results found that have the smallest edit distance
  /// found (so far) with the typo name.
  ///
  /// The pointer value being set to the current DeclContext indicates
  /// whether there is a keyword with this name.
  TypoEditDistanceMap CorrectionResults;

  SmallVector<TypoCorrection, 4> ValidatedCorrections;
  size_t CurrentTCIndex;

  Sema &SemaRef;
  Scope *S;
  CXXScopeSpec *SS;
  std::unique_ptr<CorrectionCandidateCallback> CorrectionValidator;
  DeclContext *MemberContext;
  LookupResult Result;
  NamespaceSpecifierSet Namespaces;
  SmallVector<TypoCorrection, 2> QualifiedResults;
  bool EnteringContext;
  bool SearchNamespaces;
};

} // end namespace clang

#endif
