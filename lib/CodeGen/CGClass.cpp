//===--- CGClass.cpp - Emit LLVM Code for C++ classes -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code dealing with C++ code generation of classes
//
//===----------------------------------------------------------------------===//

#include "CGBlocks.h"
#include "CGCXXABI.h"
#include "CGDebugInfo.h"
#include "CGRecordLayout.h"
#include "CodeGenFunction.h"
#include "clang/AST/CXXInheritance.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/StmtCXX.h"
#include "clang/Basic/TargetBuiltins.h"
#include "clang/CodeGen/CGFunctionInfo.h"
#include "clang/Frontend/CodeGenOptions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Utils/SanitizerStats.h"

using namespace clang;
using namespace CodeGen;

/// Return the best known alignment for an unknown pointer to a
/// particular class.
CharUnits CodeGenModule::getClassPointerAlignment(const CXXRecordDecl *RD) {
  if (!RD->isCompleteDefinition())
    return CharUnits::One(); // Hopefully won't be used anywhere.

  auto &layout = getContext().getASTRecordLayout(RD);

  // If the class is final, then we know that the pointer points to an
  // object of that type and can use the full alignment.
  if (RD->hasAttr<FinalAttr>()) {
    return layout.getAlignment();

  // Otherwise, we have to assume it could be a subclass.
  } else {
    return layout.getNonVirtualAlignment();
  }
}

/// Return the best known alignment for a pointer to a virtual base,
/// given the alignment of a pointer to the derived class.
CharUnits CodeGenModule::getVBaseAlignment(CharUnits actualDerivedAlign,
                                           const CXXRecordDecl *derivedClass,
                                           const CXXRecordDecl *vbaseClass) {
  // The basic idea here is that an underaligned derived pointer might
  // indicate an underaligned base pointer.

  assert(vbaseClass->isCompleteDefinition());
  auto &baseLayout = getContext().getASTRecordLayout(vbaseClass);
  CharUnits expectedVBaseAlign = baseLayout.getNonVirtualAlignment();

  return getDynamicOffsetAlignment(actualDerivedAlign, derivedClass,
                                   expectedVBaseAlign);
}

CharUnits
CodeGenModule::getDynamicOffsetAlignment(CharUnits actualBaseAlign,
                                         const CXXRecordDecl *baseDecl,
                                         CharUnits expectedTargetAlign) {
  // If the base is an incomplete type (which is, alas, possible with
  // member pointers), be pessimistic.
  if (!baseDecl->isCompleteDefinition())
    return std::min(actualBaseAlign, expectedTargetAlign);

  auto &baseLayout = getContext().getASTRecordLayout(baseDecl);
  CharUnits expectedBaseAlign = baseLayout.getNonVirtualAlignment();

  // If the class is properly aligned, assume the target offset is, too.
  //
  // This actually isn't necessarily the right thing to do --- if the
  // class is a complete object, but it's only properly aligned for a
  // base subobject, then the alignments of things relative to it are
  // probably off as well.  (Note that this requires the alignment of
  // the target to be greater than the NV alignment of the derived
  // class.)
  //
  // However, our approach to this kind of under-alignment can only
  // ever be best effort; after all, we're never going to propagate
  // alignments through variables or parameters.  Note, in particular,
  // that constructing a polymorphic type in an address that's less
  // than pointer-aligned will generally trap in the constructor,
  // unless we someday add some sort of attribute to change the
  // assumed alignment of 'this'.  So our goal here is pretty much
  // just to allow the user to explicitly say that a pointer is
  // under-aligned and then safely access its fields and vtables.
  if (actualBaseAlign >= expectedBaseAlign) {
    return expectedTargetAlign;
  }

  // Otherwise, we might be offset by an arbitrary multiple of the
  // actual alignment.  The correct adjustment is to take the min of
  // the two alignments.
  return std::min(actualBaseAlign, expectedTargetAlign);
}

Address CodeGenFunction::LoadCXXThisAddress() {
  assert(CurFuncDecl && "loading 'this' without a func declaration?");
  assert(isa<CXXMethodDecl>(CurFuncDecl));

  // Lazily compute CXXThisAlignment.
  if (CXXThisAlignment.isZero()) {
    // Just use the best known alignment for the parent.
    // TODO: if we're currently emitting a complete-object ctor/dtor,
    // we can always use the complete-object alignment.
    auto RD = cast<CXXMethodDecl>(CurFuncDecl)->getParent();
    CXXThisAlignment = CGM.getClassPointerAlignment(RD);
  }

  return Address(LoadCXXThis(), CXXThisAlignment);
}

/// Emit the address of a field using a member data pointer.
///
/// \param E Only used for emergency diagnostics
Address
CodeGenFunction::EmitCXXMemberDataPointerAddress(const Expr *E, Address base,
                                                 llvm::Value *memberPtr,
                                      const MemberPointerType *memberPtrType,
                                                 AlignmentSource *alignSource) {
  // Ask the ABI to compute the actual address.
  llvm::Value *ptr =
    CGM.getCXXABI().EmitMemberDataPointerAddress(*this, E, base,
                                                 memberPtr, memberPtrType);

  QualType memberType = memberPtrType->getPointeeType();
  CharUnits memberAlign = getNaturalTypeAlignment(memberType, alignSource);
  memberAlign =
    CGM.getDynamicOffsetAlignment(base.getAlignment(),
                            memberPtrType->getClass()->getAsCXXRecordDecl(),
                                  memberAlign);
  return Address(ptr, memberAlign);
}

CharUnits CodeGenModule::computeNonVirtualBaseClassOffset(
    const CXXRecordDecl *DerivedClass, CastExpr::path_const_iterator Start,
    CastExpr::path_const_iterator End) {
  CharUnits Offset = CharUnits::Zero();

  const ASTContext &Context = getContext();
  const CXXRecordDecl *RD = DerivedClass;

  for (CastExpr::path_const_iterator I = Start; I != End; ++I) {
    const CXXBaseSpecifier *Base = *I;
    assert(!Base->isVirtual() && "Should not see virtual bases here!");

    // Get the layout.
    const ASTRecordLayout &Layout = Context.getASTRecordLayout(RD);

    const CXXRecordDecl *BaseDecl =
      cast<CXXRecordDecl>(Base->getType()->getAs<RecordType>()->getDecl());

    // Add the offset.
    Offset += Layout.getBaseClassOffset(BaseDecl);

    RD = BaseDecl;
  }

  return Offset;
}

llvm::Constant *
CodeGenModule::GetNonVirtualBaseClassOffset(const CXXRecordDecl *ClassDecl,
                                   CastExpr::path_const_iterator PathBegin,
                                   CastExpr::path_const_iterator PathEnd) {
  assert(PathBegin != PathEnd && "Base path should not be empty!");

  CharUnits Offset =
      computeNonVirtualBaseClassOffset(ClassDecl, PathBegin, PathEnd);
  if (Offset.isZero())
    return nullptr;

  llvm::Type *PtrDiffTy =
  Types.ConvertType(getContext().getPointerDiffType());

  return llvm::ConstantInt::get(PtrDiffTy, Offset.getQuantity());
}

/// Gets the address of a direct base class within a complete object.
/// This should only be used for (1) non-virtual bases or (2) virtual bases
/// when the type is known to be complete (e.g. in complete destructors).
///
/// The object pointed to by 'This' is assumed to be non-null.
Address
CodeGenFunction::GetAddressOfDirectBaseInCompleteClass(Address This,
                                                   const CXXRecordDecl *Derived,
                                                   const CXXRecordDecl *Base,
                                                   bool BaseIsVirtual) {
  // 'this' must be a pointer (in some address space) to Derived.
  assert(This.getElementType() == ConvertType(Derived));

  // Compute the offset of the virtual base.
  CharUnits Offset;
  const ASTRecordLayout &Layout = getContext().getASTRecordLayout(Derived);
  if (BaseIsVirtual)
    Offset = Layout.getVBaseClassOffset(Base);
  else
    Offset = Layout.getBaseClassOffset(Base);

  // Shift and cast down to the base type.
  // TODO: for complete types, this should be possible with a GEP.
  Address V = This;
  if (!Offset.isZero()) {
    V = Builder.CreateElementBitCast(V, Int8Ty);
    V = Builder.CreateConstInBoundsByteGEP(V, Offset);
  }
  V = Builder.CreateElementBitCast(V, ConvertType(Base));

  return V;
}

static Address
ApplyNonVirtualAndVirtualOffset(CodeGenFunction &CGF, Address addr,
                                CharUnits nonVirtualOffset,
                                llvm::Value *virtualOffset,
                                const CXXRecordDecl *derivedClass,
                                const CXXRecordDecl *nearestVBase) {
  // Assert that we have something to do.
  assert(!nonVirtualOffset.isZero() || virtualOffset != nullptr);

  // Compute the offset from the static and dynamic components.
  llvm::Value *baseOffset;
  if (!nonVirtualOffset.isZero()) {
    baseOffset = llvm::ConstantInt::get(CGF.PtrDiffTy,
                                        nonVirtualOffset.getQuantity());
    if (virtualOffset) {
      baseOffset = CGF.Builder.CreateAdd(virtualOffset, baseOffset);
    }
  } else {
    baseOffset = virtualOffset;
  }

  // Apply the base offset.
  llvm::Value *ptr = addr.getPointer();
  ptr = CGF.Builder.CreateBitCast(ptr, CGF.Int8PtrTy);
  ptr = CGF.Builder.CreateInBoundsGEP(ptr, baseOffset, "add.ptr");

  // If we have a virtual component, the alignment of the result will
  // be relative only to the known alignment of that vbase.
  CharUnits alignment;
  if (virtualOffset) {
    assert(nearestVBase && "virtual offset without vbase?");
    alignment = CGF.CGM.getVBaseAlignment(addr.getAlignment(),
                                          derivedClass, nearestVBase);
  } else {
    alignment = addr.getAlignment();
  }
  alignment = alignment.alignmentAtOffset(nonVirtualOffset);

  return Address(ptr, alignment);
}

Address CodeGenFunction::GetAddressOfBaseClass(
    Address Value, const CXXRecordDecl *Derived,
    CastExpr::path_const_iterator PathBegin,
    CastExpr::path_const_iterator PathEnd, bool NullCheckValue,
    SourceLocation Loc) {
  assert(PathBegin != PathEnd && "Base path should not be empty!");

  CastExpr::path_const_iterator Start = PathBegin;
  const CXXRecordDecl *VBase = nullptr;

  // Sema has done some convenient canonicalization here: if the
  // access path involved any virtual steps, the conversion path will
  // *start* with a step down to the correct virtual base subobject,
  // and hence will not require any further steps.
  if ((*Start)->isVirtual()) {
    VBase =
      cast<CXXRecordDecl>((*Start)->getType()->getAs<RecordType>()->getDecl());
    ++Start;
  }

  // Compute the static offset of the ultimate destination within its
  // allocating subobject (the virtual base, if there is one, or else
  // the "complete" object that we see).
  CharUnits NonVirtualOffset = CGM.computeNonVirtualBaseClassOffset(
      VBase ? VBase : Derived, Start, PathEnd);

  // If there's a virtual step, we can sometimes "devirtualize" it.
  // For now, that's limited to when the derived type is final.
  // TODO: "devirtualize" this for accesses to known-complete objects.
  if (VBase && Derived->hasAttr<FinalAttr>()) {
    const ASTRecordLayout &layout = getContext().getASTRecordLayout(Derived);
    CharUnits vBaseOffset = layout.getVBaseClassOffset(VBase);
    NonVirtualOffset += vBaseOffset;
    VBase = nullptr; // we no longer have a virtual step
  }

  // Get the base pointer type.
  llvm::Type *BasePtrTy =
    ConvertType((PathEnd[-1])->getType())->getPointerTo();

  QualType DerivedTy = getContext().getRecordType(Derived);
  CharUnits DerivedAlign = CGM.getClassPointerAlignment(Derived);

  // If the static offset is zero and we don't have a virtual step,
  // just do a bitcast; null checks are unnecessary.
  if (NonVirtualOffset.isZero() && !VBase) {
    if (sanitizePerformTypeCheck()) {
      EmitTypeCheck(TCK_Upcast, Loc, Value.getPointer(),
                    DerivedTy, DerivedAlign, !NullCheckValue);
    }
    return Builder.CreateBitCast(Value, BasePtrTy);
  }

  llvm::BasicBlock *origBB = nullptr;
  llvm::BasicBlock *endBB = nullptr;

  // Skip over the offset (and the vtable load) if we're supposed to
  // null-check the pointer.
  if (NullCheckValue) {
    origBB = Builder.GetInsertBlock();
    llvm::BasicBlock *notNullBB = createBasicBlock("cast.notnull");
    endBB = createBasicBlock("cast.end");

    llvm::Value *isNull = Builder.CreateIsNull(Value.getPointer());
    Builder.CreateCondBr(isNull, endBB, notNullBB);
    EmitBlock(notNullBB);
  }

  if (sanitizePerformTypeCheck()) {
    EmitTypeCheck(VBase ? TCK_UpcastToVirtualBase : TCK_Upcast, Loc,
                  Value.getPointer(), DerivedTy, DerivedAlign, true);
  }

  // Compute the virtual offset.
  llvm::Value *VirtualOffset = nullptr;
  if (VBase) {
    VirtualOffset =
      CGM.getCXXABI().GetVirtualBaseClassOffset(*this, Value, Derived, VBase);
  }

  // Apply both offsets.
  Value = ApplyNonVirtualAndVirtualOffset(*this, Value, NonVirtualOffset,
                                          VirtualOffset, Derived, VBase);

  // Cast to the destination type.
  Value = Builder.CreateBitCast(Value, BasePtrTy);

  // Build a phi if we needed a null check.
  if (NullCheckValue) {
    llvm::BasicBlock *notNullBB = Builder.GetInsertBlock();
    Builder.CreateBr(endBB);
    EmitBlock(endBB);

    llvm::PHINode *PHI = Builder.CreatePHI(BasePtrTy, 2, "cast.result");
    PHI->addIncoming(Value.getPointer(), notNullBB);
    PHI->addIncoming(llvm::Constant::getNullValue(BasePtrTy), origBB);
    Value = Address(PHI, Value.getAlignment());
  }

  return Value;
}

Address
CodeGenFunction::GetAddressOfDerivedClass(Address BaseAddr,
                                          const CXXRecordDecl *Derived,
                                        CastExpr::path_const_iterator PathBegin,
                                          CastExpr::path_const_iterator PathEnd,
                                          bool NullCheckValue) {
  assert(PathBegin != PathEnd && "Base path should not be empty!");

  QualType DerivedTy =
    getContext().getCanonicalType(getContext().getTagDeclType(Derived));
  llvm::Type *DerivedPtrTy = ConvertType(DerivedTy)->getPointerTo();

  llvm::Value *NonVirtualOffset =
    CGM.GetNonVirtualBaseClassOffset(Derived, PathBegin, PathEnd);

  if (!NonVirtualOffset) {
    // No offset, we can just cast back.
    return Builder.CreateBitCast(BaseAddr, DerivedPtrTy);
  }

  llvm::BasicBlock *CastNull = nullptr;
  llvm::BasicBlock *CastNotNull = nullptr;
  llvm::BasicBlock *CastEnd = nullptr;

  if (NullCheckValue) {
    CastNull = createBasicBlock("cast.null");
    CastNotNull = createBasicBlock("cast.notnull");
    CastEnd = createBasicBlock("cast.end");

    llvm::Value *IsNull = Builder.CreateIsNull(BaseAddr.getPointer());
    Builder.CreateCondBr(IsNull, CastNull, CastNotNull);
    EmitBlock(CastNotNull);
  }

  // Apply the offset.
  llvm::Value *Value = Builder.CreateBitCast(BaseAddr.getPointer(), Int8PtrTy);
  Value = Builder.CreateGEP(Value, Builder.CreateNeg(NonVirtualOffset),
                            "sub.ptr");

  // Just cast.
  Value = Builder.CreateBitCast(Value, DerivedPtrTy);

  // Produce a PHI if we had a null-check.
  if (NullCheckValue) {
    Builder.CreateBr(CastEnd);
    EmitBlock(CastNull);
    Builder.CreateBr(CastEnd);
    EmitBlock(CastEnd);

    llvm::PHINode *PHI = Builder.CreatePHI(Value->getType(), 2);
    PHI->addIncoming(Value, CastNotNull);
    PHI->addIncoming(llvm::Constant::getNullValue(Value->getType()), CastNull);
    Value = PHI;
  }

  return Address(Value, CGM.getClassPointerAlignment(Derived));
}

llvm::Value *CodeGenFunction::GetVTTParameter(GlobalDecl GD,
                                              bool ForVirtualBase,
                                              bool Delegating) {
  if (!CGM.getCXXABI().NeedsVTTParameter(GD)) {
    // This constructor/destructor does not need a VTT parameter.
    return nullptr;
  }

  const CXXRecordDecl *RD = cast<CXXMethodDecl>(CurCodeDecl)->getParent();
  const CXXRecordDecl *Base = cast<CXXMethodDecl>(GD.getDecl())->getParent();

  llvm::Value *VTT;

  uint64_t SubVTTIndex;

  if (Delegating) {
    // If this is a delegating constructor call, just load the VTT.
    return LoadCXXVTT();
  } else if (RD == Base) {
    // If the record matches the base, this is the complete ctor/dtor
    // variant calling the base variant in a class with virtual bases.
    assert(!CGM.getCXXABI().NeedsVTTParameter(CurGD) &&
           "doing no-op VTT offset in base dtor/ctor?");
    assert(!ForVirtualBase && "Can't have same class as virtual base!");
    SubVTTIndex = 0;
  } else {
    const ASTRecordLayout &Layout = getContext().getASTRecordLayout(RD);
    CharUnits BaseOffset = ForVirtualBase ?
      Layout.getVBaseClassOffset(Base) :
      Layout.getBaseClassOffset(Base);

    SubVTTIndex =
      CGM.getVTables().getSubVTTIndex(RD, BaseSubobject(Base, BaseOffset));
    assert(SubVTTIndex != 0 && "Sub-VTT index must be greater than zero!");
  }

  if (CGM.getCXXABI().NeedsVTTParameter(CurGD)) {
    // A VTT parameter was passed to the constructor, use it.
    VTT = LoadCXXVTT();
    VTT = Builder.CreateConstInBoundsGEP1_64(VTT, SubVTTIndex);
  } else {
    // We're the complete constructor, so get the VTT by name.
    VTT = CGM.getVTables().GetAddrOfVTT(RD);
    VTT = Builder.CreateConstInBoundsGEP2_64(VTT, 0, SubVTTIndex);
  }

  return VTT;
}

namespace {
  /// Call the destructor for a direct base class.
  struct CallBaseDtor final : EHScopeStack::Cleanup {
    const CXXRecordDecl *BaseClass;
    bool BaseIsVirtual;
    CallBaseDtor(const CXXRecordDecl *Base, bool BaseIsVirtual)
      : BaseClass(Base), BaseIsVirtual(BaseIsVirtual) {}

    void Emit(CodeGenFunction &CGF, Flags flags) override {
      const CXXRecordDecl *DerivedClass =
        cast<CXXMethodDecl>(CGF.CurCodeDecl)->getParent();

      const CXXDestructorDecl *D = BaseClass->getDestructor();
      Address Addr =
        CGF.GetAddressOfDirectBaseInCompleteClass(CGF.LoadCXXThisAddress(),
                                                  DerivedClass, BaseClass,
                                                  BaseIsVirtual);
      CGF.EmitCXXDestructorCall(D, Dtor_Base, BaseIsVirtual,
                                /*Delegating=*/false, Addr);
    }
  };

  /// A visitor which checks whether an initializer uses 'this' in a
  /// way which requires the vtable to be properly set.
  struct DynamicThisUseChecker : ConstEvaluatedExprVisitor<DynamicThisUseChecker> {
    typedef ConstEvaluatedExprVisitor<DynamicThisUseChecker> super;

    bool UsesThis;

    DynamicThisUseChecker(const ASTContext &C) : super(C), UsesThis(false) {}

    // Black-list all explicit and implicit references to 'this'.
    //
    // Do we need to worry about external references to 'this' derived
    // from arbitrary code?  If so, then anything which runs arbitrary
    // external code might potentially access the vtable.
    void VisitCXXThisExpr(const CXXThisExpr *E) { UsesThis = true; }
  };
} // end anonymous namespace

static bool BaseInitializerUsesThis(ASTContext &C, const Expr *Init) {
  DynamicThisUseChecker Checker(C);
  Checker.Visit(Init);
  return Checker.UsesThis;
}

static void EmitBaseInitializer(CodeGenFunction &CGF,
                                const CXXRecordDecl *ClassDecl,
                                CXXCtorInitializer *BaseInit,
                                CXXCtorType CtorType) {
  assert(BaseInit->isBaseInitializer() &&
         "Must have base initializer!");

  Address ThisPtr = CGF.LoadCXXThisAddress();

  const Type *BaseType = BaseInit->getBaseClass();
  CXXRecordDecl *BaseClassDecl =
    cast<CXXRecordDecl>(BaseType->getAs<RecordType>()->getDecl());

  bool isBaseVirtual = BaseInit->isBaseVirtual();

  // The base constructor doesn't construct virtual bases.
  if (CtorType == Ctor_Base && isBaseVirtual)
    return;

  // If the initializer for the base (other than the constructor
  // itself) accesses 'this' in any way, we need to initialize the
  // vtables.
  if (BaseInitializerUsesThis(CGF.getContext(), BaseInit->getInit()))
    CGF.InitializeVTablePointers(ClassDecl);

  // We can pretend to be a complete class because it only matters for
  // virtual bases, and we only do virtual bases for complete ctors.
  Address V =
    CGF.GetAddressOfDirectBaseInCompleteClass(ThisPtr, ClassDecl,
                                              BaseClassDecl,
                                              isBaseVirtual);
  AggValueSlot AggSlot =
    AggValueSlot::forAddr(V, Qualifiers(),
                          AggValueSlot::IsDestructed,
                          AggValueSlot::DoesNotNeedGCBarriers,
                          AggValueSlot::IsNotAliased);

  CGF.EmitAggExpr(BaseInit->getInit(), AggSlot);

  if (CGF.CGM.getLangOpts().Exceptions &&
      !BaseClassDecl->hasTrivialDestructor())
    CGF.EHStack.pushCleanup<CallBaseDtor>(EHCleanup, BaseClassDecl,
                                          isBaseVirtual);
}

/// Initialize a member of aggregate type using the given expression
/// as an initializer.
///
/// The member may be an array.  If so:
/// - the destination l-value will be a pointer of the *base* element type,
/// - ArrayIndexVar will be a pointer to a variable containing the current
///   index within the destination array, and
/// - ArrayIndexes will be an array of index variables, one for each level
///   of array nesting, which will need to be updated as appropriate for the
///   array structure.
///
/// On an array, this function will invoke itself recursively.  Each time,
/// it drills into one nesting level of the member type and sets up a
/// loop updating the appropriate array index variable.
static void EmitAggMemberInitializer(CodeGenFunction &CGF,
                                     LValue LHS,
                                     Expr *Init,
                                     Address ArrayIndexVar,
                                     QualType T,
                                     ArrayRef<VarDecl *> ArrayIndexes,
                                     unsigned Index) {
  assert(ArrayIndexVar.isValid() == (ArrayIndexes.size() != 0));

  if (Index == ArrayIndexes.size()) {
    LValue LV = LHS;

    Optional<CodeGenFunction::RunCleanupsScope> Scope;

    if (ArrayIndexVar.isValid()) {
      // When we're processing an array, the temporaries from each
      // element's construction are destroyed immediately.
      Scope.emplace(CGF);

      // If we have an array index variable, load it and use it as an offset.
      // Then, increment the value.
      llvm::Value *Dest = LHS.getPointer();
      llvm::Value *ArrayIndex = CGF.Builder.CreateLoad(ArrayIndexVar);
      Dest = CGF.Builder.CreateInBoundsGEP(Dest, ArrayIndex, "destaddress");
      llvm::Value *Next = llvm::ConstantInt::get(ArrayIndex->getType(), 1);
      Next = CGF.Builder.CreateAdd(ArrayIndex, Next, "inc");
      CGF.Builder.CreateStore(Next, ArrayIndexVar);

      // Update the LValue.
      CharUnits EltSize = CGF.getContext().getTypeSizeInChars(T);
      CharUnits Align = LV.getAlignment().alignmentOfArrayElement(EltSize);
      LV.setAddress(Address(Dest, Align));

      // Enter a partial-array EH cleanup to destroy previous members
      // of the array if this initialization throws.
      if (CGF.CGM.getLangOpts().Exceptions) {
        if (auto DtorKind = T.isDestructedType()) {
          if (CGF.needsEHCleanup(DtorKind)) {
            CGF.pushRegularPartialArrayCleanup(LHS.getPointer(),
                                               LV.getPointer(), T,
                                               LV.getAlignment(),
                                               CGF.getDestroyer(DtorKind));
          }
        }
      }
    }

    switch (CGF.getEvaluationKind(T)) {
    case TEK_Scalar:
      CGF.EmitScalarInit(Init, /*decl*/ nullptr, LV, false);
      break;
    case TEK_Complex:
      CGF.EmitComplexExprIntoLValue(Init, LV, /*isInit*/ true);
      break;
    case TEK_Aggregate: {
      AggValueSlot Slot =
        AggValueSlot::forLValue(LV,
                                AggValueSlot::IsDestructed,
                                AggValueSlot::DoesNotNeedGCBarriers,
                                AggValueSlot::IsNotAliased);

      CGF.EmitAggExpr(Init, Slot);
      break;
    }
    }

    return;
  }

  const ConstantArrayType *Array = CGF.getContext().getAsConstantArrayType(T);
  assert(Array && "Array initialization without the array type?");
  Address IndexVar = CGF.GetAddrOfLocalVar(ArrayIndexes[Index]);

  // Initialize this index variable to zero.
  llvm::Value* Zero
    = llvm::Constant::getNullValue(IndexVar.getElementType());
  CGF.Builder.CreateStore(Zero, IndexVar);

  // Start the loop with a block that tests the condition.
  llvm::BasicBlock *CondBlock = CGF.createBasicBlock("for.cond");
  llvm::BasicBlock *AfterFor = CGF.createBasicBlock("for.end");

  CGF.EmitBlock(CondBlock);

  llvm::BasicBlock *ForBody = CGF.createBasicBlock("for.body");
  // Generate: if (loop-index < number-of-elements) fall to the loop body,
  // otherwise, go to the block after the for-loop.
  uint64_t NumElements = Array->getSize().getZExtValue();
  llvm::Value *Counter = CGF.Builder.CreateLoad(IndexVar);
  llvm::Value *NumElementsPtr =
    llvm::ConstantInt::get(Counter->getType(), NumElements);
  llvm::Value *IsLess = CGF.Builder.CreateICmpULT(Counter, NumElementsPtr,
                                                  "isless");

  // If the condition is true, execute the body.
  CGF.Builder.CreateCondBr(IsLess, ForBody, AfterFor);

  CGF.EmitBlock(ForBody);
  llvm::BasicBlock *ContinueBlock = CGF.createBasicBlock("for.inc");

  // Inside the loop body recurse to emit the inner loop or, eventually, the
  // constructor call.
  EmitAggMemberInitializer(CGF, LHS, Init, ArrayIndexVar,
                           Array->getElementType(), ArrayIndexes, Index + 1);

  CGF.EmitBlock(ContinueBlock);

  // Emit the increment of the loop counter.
  llvm::Value *NextVal = llvm::ConstantInt::get(Counter->getType(), 1);
  Counter = CGF.Builder.CreateLoad(IndexVar);
  NextVal = CGF.Builder.CreateAdd(Counter, NextVal, "inc");
  CGF.Builder.CreateStore(NextVal, IndexVar);

  // Finally, branch back up to the condition for the next iteration.
  CGF.EmitBranch(CondBlock);

  // Emit the fall-through block.
  CGF.EmitBlock(AfterFor, true);
}

static bool isMemcpyEquivalentSpecialMember(const CXXMethodDecl *D) {
  auto *CD = dyn_cast<CXXConstructorDecl>(D);
  if (!(CD && CD->isCopyOrMoveConstructor()) &&
      !D->isCopyAssignmentOperator() && !D->isMoveAssignmentOperator())
    return false;

  // We can emit a memcpy for a trivial copy or move constructor/assignment.
  if (D->isTrivial() && !D->getParent()->mayInsertExtraPadding())
    return true;

  // We *must* emit a memcpy for a defaulted union copy or move op.
  if (D->getParent()->isUnion() && D->isDefaulted())
    return true;

  return false;
}

static void EmitLValueForAnyFieldInitialization(CodeGenFunction &CGF,
                                                CXXCtorInitializer *MemberInit,
                                                LValue &LHS) {
  FieldDecl *Field = MemberInit->getAnyMember();
  if (MemberInit->isIndirectMemberInitializer()) {
    // If we are initializing an anonymous union field, drill down to the field.
    IndirectFieldDecl *IndirectField = MemberInit->getIndirectMember();
    for (const auto *I : IndirectField->chain())
      LHS = CGF.EmitLValueForFieldInitialization(LHS, cast<FieldDecl>(I));
  } else {
    LHS = CGF.EmitLValueForFieldInitialization(LHS, Field);
  }
}

static void EmitMemberInitializer(CodeGenFunction &CGF,
                                  const CXXRecordDecl *ClassDecl,
                                  CXXCtorInitializer *MemberInit,
                                  const CXXConstructorDecl *Constructor,
                                  FunctionArgList &Args) {
  ApplyDebugLocation Loc(CGF, MemberInit->getSourceLocation());
  assert(MemberInit->isAnyMemberInitializer() &&
         "Must have member initializer!");
  assert(MemberInit->getInit() && "Must have initializer!");

  // non-static data member initializers.
  FieldDecl *Field = MemberInit->getAnyMember();
  QualType FieldType = Field->getType();

  llvm::Value *ThisPtr = CGF.LoadCXXThis();
  QualType RecordTy = CGF.getContext().getTypeDeclType(ClassDecl);
  LValue LHS = CGF.MakeNaturalAlignAddrLValue(ThisPtr, RecordTy);

  EmitLValueForAnyFieldInitialization(CGF, MemberInit, LHS);

  // Special case: if we are in a copy or move constructor, and we are copying
  // an array of PODs or classes with trivial copy constructors, ignore the
  // AST and perform the copy we know is equivalent.
  // FIXME: This is hacky at best... if we had a bit more explicit information
  // in the AST, we could generalize it more easily.
  const ConstantArrayType *Array
    = CGF.getContext().getAsConstantArrayType(FieldType);
  if (Array && Constructor->isDefaulted() &&
      Constructor->isCopyOrMoveConstructor()) {
    QualType BaseElementTy = CGF.getContext().getBaseElementType(Array);
    CXXConstructExpr *CE = dyn_cast<CXXConstructExpr>(MemberInit->getInit());
    if (BaseElementTy.isPODType(CGF.getContext()) ||
        (CE && isMemcpyEquivalentSpecialMember(CE->getConstructor()))) {
      unsigned SrcArgIndex =
          CGF.CGM.getCXXABI().getSrcArgforCopyCtor(Constructor, Args);
      llvm::Value *SrcPtr
        = CGF.Builder.CreateLoad(CGF.GetAddrOfLocalVar(Args[SrcArgIndex]));
      LValue ThisRHSLV = CGF.MakeNaturalAlignAddrLValue(SrcPtr, RecordTy);
      LValue Src = CGF.EmitLValueForFieldInitialization(ThisRHSLV, Field);

      // Copy the aggregate.
      CGF.EmitAggregateCopy(LHS.getAddress(), Src.getAddress(), FieldType,
                            LHS.isVolatileQualified());
      // Ensure that we destroy the objects if an exception is thrown later in
      // the constructor.
      QualType::DestructionKind dtorKind = FieldType.isDestructedType();
      if (CGF.needsEHCleanup(dtorKind))
        CGF.pushEHDestroy(dtorKind, LHS.getAddress(), FieldType); 
      return;
    }
  }

  ArrayRef<VarDecl *> ArrayIndexes;
  if (MemberInit->getNumArrayIndices())
    ArrayIndexes = MemberInit->getArrayIndices();
  CGF.EmitInitializerForField(Field, LHS, MemberInit->getInit(), ArrayIndexes);
}

void CodeGenFunction::EmitInitializerForField(FieldDecl *Field, LValue LHS,
                                Expr *Init, ArrayRef<VarDecl *> ArrayIndexes) {
  QualType FieldType = Field->getType();
  switch (getEvaluationKind(FieldType)) {
  case TEK_Scalar:
    if (LHS.isSimple()) {
      EmitExprAsInit(Init, Field, LHS, false);
    } else {
      RValue RHS = RValue::get(EmitScalarExpr(Init));
      EmitStoreThroughLValue(RHS, LHS);
    }
    break;
  case TEK_Complex:
    EmitComplexExprIntoLValue(Init, LHS, /*isInit*/ true);
    break;
  case TEK_Aggregate: {
    Address ArrayIndexVar = Address::invalid();
    if (ArrayIndexes.size()) {
      // The LHS is a pointer to the first object we'll be constructing, as
      // a flat array.
      QualType BaseElementTy = getContext().getBaseElementType(FieldType);
      llvm::Type *BasePtr = ConvertType(BaseElementTy);
      BasePtr = llvm::PointerType::getUnqual(BasePtr);
      Address BaseAddrPtr = Builder.CreateBitCast(LHS.getAddress(), BasePtr);
      LHS = MakeAddrLValue(BaseAddrPtr, BaseElementTy);

      // Create an array index that will be used to walk over all of the
      // objects we're constructing.
      ArrayIndexVar = CreateMemTemp(getContext().getSizeType(), "object.index");
      llvm::Value *Zero =
        llvm::Constant::getNullValue(ArrayIndexVar.getElementType());
      Builder.CreateStore(Zero, ArrayIndexVar);

      // Emit the block variables for the array indices, if any.
      for (unsigned I = 0, N = ArrayIndexes.size(); I != N; ++I)
        EmitAutoVarDecl(*ArrayIndexes[I]);
    }

    EmitAggMemberInitializer(*this, LHS, Init, ArrayIndexVar, FieldType,
                             ArrayIndexes, 0);
  }
  }

  // Ensure that we destroy this object if an exception is thrown
  // later in the constructor.
  QualType::DestructionKind dtorKind = FieldType.isDestructedType();
  if (needsEHCleanup(dtorKind))
    pushEHDestroy(dtorKind, LHS.getAddress(), FieldType);
}

/// Checks whether the given constructor is a valid subject for the
/// complete-to-base constructor delegation optimization, i.e.
/// emitting the complete constructor as a simple call to the base
/// constructor.
static bool IsConstructorDelegationValid(const CXXConstructorDecl *Ctor) {

  // Currently we disable the optimization for classes with virtual
  // bases because (1) the addresses of parameter variables need to be
  // consistent across all initializers but (2) the delegate function
  // call necessarily creates a second copy of the parameter variable.
  //
  // The limiting example (purely theoretical AFAIK):
  //   struct A { A(int &c) { c++; } };
  //   struct B : virtual A {
  //     B(int count) : A(count) { printf("%d\n", count); }
  //   };
  // ...although even this example could in principle be emitted as a
  // delegation since the address of the parameter doesn't escape.
  if (Ctor->getParent()->getNumVBases()) {
    // TODO: white-list trivial vbase initializers.  This case wouldn't
    // be subject to the restrictions below.

    // TODO: white-list cases where:
    //  - there are no non-reference parameters to the constructor
    //  - the initializers don't access any non-reference parameters
    //  - the initializers don't take the address of non-reference
    //    parameters
    //  - etc.
    // If we ever add any of the above cases, remember that:
    //  - function-try-blocks will always blacklist this optimization
    //  - we need to perform the constructor prologue and cleanup in
    //    EmitConstructorBody.

    return false;
  }

  // We also disable the optimization for variadic functions because
  // it's impossible to "re-pass" varargs.
  if (Ctor->getType()->getAs<FunctionProtoType>()->isVariadic())
    return false;

  // FIXME: Decide if we can do a delegation of a delegating constructor.
  if (Ctor->isDelegatingConstructor())
    return false;

  return true;
}

// Emit code in ctor (Prologue==true) or dtor (Prologue==false)
// to poison the extra field paddings inserted under
// -fsanitize-address-field-padding=1|2.
void CodeGenFunction::EmitAsanPrologueOrEpilogue(bool Prologue) {
  ASTContext &Context = getContext();
  const CXXRecordDecl *ClassDecl =
      Prologue ? cast<CXXConstructorDecl>(CurGD.getDecl())->getParent()
               : cast<CXXDestructorDecl>(CurGD.getDecl())->getParent();
  if (!ClassDecl->mayInsertExtraPadding()) return;

  struct SizeAndOffset {
    uint64_t Size;
    uint64_t Offset;
  };

  unsigned PtrSize = CGM.getDataLayout().getPointerSizeInBits();
  const ASTRecordLayout &Info = Context.getASTRecordLayout(ClassDecl);

  // Populate sizes and offsets of fields.
  SmallVector<SizeAndOffset, 16> SSV(Info.getFieldCount());
  for (unsigned i = 0, e = Info.getFieldCount(); i != e; ++i)
    SSV[i].Offset =
        Context.toCharUnitsFromBits(Info.getFieldOffset(i)).getQuantity();

  size_t NumFields = 0;
  for (const auto *Field : ClassDecl->fields()) {
    const FieldDecl *D = Field;
    std::pair<CharUnits, CharUnits> FieldInfo =
        Context.getTypeInfoInChars(D->getType());
    CharUnits FieldSize = FieldInfo.first;
    assert(NumFields < SSV.size());
    SSV[NumFields].Size = D->isBitField() ? 0 : FieldSize.getQuantity();
    NumFields++;
  }
  assert(NumFields == SSV.size());
  if (SSV.size() <= 1) return;

  // We will insert calls to __asan_* run-time functions.
  // LLVM AddressSanitizer pass may decide to inline them later.
  llvm::Type *Args[2] = {IntPtrTy, IntPtrTy};
  llvm::FunctionType *FTy =
      llvm::FunctionType::get(CGM.VoidTy, Args, false);
  llvm::Constant *F = CGM.CreateRuntimeFunction(
      FTy, Prologue ? "__asan_poison_intra_object_redzone"
                    : "__asan_unpoison_intra_object_redzone");

  llvm::Value *ThisPtr = LoadCXXThis();
  ThisPtr = Builder.CreatePtrToInt(ThisPtr, IntPtrTy);
  uint64_t TypeSize = Info.getNonVirtualSize().getQuantity();
  // For each field check if it has sufficient padding,
  // if so (un)poison it with a call.
  for (size_t i = 0; i < SSV.size(); i++) {
    uint64_t AsanAlignment = 8;
    uint64_t NextField = i == SSV.size() - 1 ? TypeSize : SSV[i + 1].Offset;
    uint64_t PoisonSize = NextField - SSV[i].Offset - SSV[i].Size;
    uint64_t EndOffset = SSV[i].Offset + SSV[i].Size;
    if (PoisonSize < AsanAlignment || !SSV[i].Size ||
        (NextField % AsanAlignment) != 0)
      continue;
    Builder.CreateCall(
        F, {Builder.CreateAdd(ThisPtr, Builder.getIntN(PtrSize, EndOffset)),
            Builder.getIntN(PtrSize, PoisonSize)});
  }
}

/// EmitConstructorBody - Emits the body of the current constructor.
void CodeGenFunction::EmitConstructorBody(FunctionArgList &Args) {
  EmitAsanPrologueOrEpilogue(true);
  const CXXConstructorDecl *Ctor = cast<CXXConstructorDecl>(CurGD.getDecl());
  CXXCtorType CtorType = CurGD.getCtorType();

  assert((CGM.getTarget().getCXXABI().hasConstructorVariants() ||
          CtorType == Ctor_Complete) &&
         "can only generate complete ctor for this ABI");

  // Before we go any further, try the complete->base constructor
  // delegation optimization.
  if (CtorType == Ctor_Complete && IsConstructorDelegationValid(Ctor) &&
      CGM.getTarget().getCXXABI().hasConstructorVariants()) {
    EmitDelegateCXXConstructorCall(Ctor, Ctor_Base, Args, Ctor->getLocEnd());
    return;
  }

  const FunctionDecl *Definition = nullptr;
  Stmt *Body = Ctor->getBody(Definition);
  assert(Definition == Ctor && "emitting wrong constructor body");

  // Enter the function-try-block before the constructor prologue if
  // applicable.
  bool IsTryBody = (Body && isa<CXXTryStmt>(Body));
  if (IsTryBody)
    EnterCXXTryStmt(*cast<CXXTryStmt>(Body), true);

  incrementProfileCounter(Body);

  RunCleanupsScope RunCleanups(*this);

  // TODO: in restricted cases, we can emit the vbase initializers of
  // a complete ctor and then delegate to the base ctor.

  // Emit the constructor prologue, i.e. the base and member
  // initializers.
  EmitCtorPrologue(Ctor, CtorType, Args);

  // Emit the body of the statement.
  if (IsTryBody)
    EmitStmt(cast<CXXTryStmt>(Body)->getTryBlock());
  else if (Body)
    EmitStmt(Body);

  // Emit any cleanup blocks associated with the member or base
  // initializers, which includes (along the exceptional path) the
  // destructors for those members and bases that were fully
  // constructed.
  RunCleanups.ForceCleanup();

  if (IsTryBody)
    ExitCXXTryStmt(*cast<CXXTryStmt>(Body), true);
}

namespace {
  /// RAII object to indicate that codegen is copying the value representation
  /// instead of the object representation. Useful when copying a struct or
  /// class which has uninitialized members and we're only performing
  /// lvalue-to-rvalue conversion on the object but not its members.
  class CopyingValueRepresentation {
  public:
    explicit CopyingValueRepresentation(CodeGenFunction &CGF)
        : CGF(CGF), OldSanOpts(CGF.SanOpts) {
      CGF.SanOpts.set(SanitizerKind::Bool, false);
      CGF.SanOpts.set(SanitizerKind::Enum, false);
    }
    ~CopyingValueRepresentation() {
      CGF.SanOpts = OldSanOpts;
    }
  private:
    CodeGenFunction &CGF;
    SanitizerSet OldSanOpts;
  };
} // end anonymous namespace
 
namespace {
  class FieldMemcpyizer {
  public:
    FieldMemcpyizer(CodeGenFunction &CGF, const CXXRecordDecl *ClassDecl,
                    const VarDecl *SrcRec)
      : CGF(CGF), ClassDecl(ClassDecl), SrcRec(SrcRec),
        RecLayout(CGF.getContext().getASTRecordLayout(ClassDecl)),
        FirstField(nullptr), LastField(nullptr), FirstFieldOffset(0),
        LastFieldOffset(0), LastAddedFieldIndex(0) {}

    bool isMemcpyableField(FieldDecl *F) const {
      // Never memcpy fields when we are adding poisoned paddings.
      if (CGF.getContext().getLangOpts().SanitizeAddressFieldPadding)
        return false;
      Qualifiers Qual = F->getType().getQualifiers();
      if (Qual.hasVolatile() || Qual.hasObjCLifetime())
        return false;
      return true;
    }

    void addMemcpyableField(FieldDecl *F) {
      if (!FirstField)
        addInitialField(F);
      else
        addNextField(F);
    }

    CharUnits getMemcpySize(uint64_t FirstByteOffset) const {
      unsigned LastFieldSize =
        LastField->isBitField() ?
          LastField->getBitWidthValue(CGF.getContext()) :
          CGF.getContext().getTypeSize(LastField->getType());
      uint64_t MemcpySizeBits =
        LastFieldOffset + LastFieldSize - FirstByteOffset +
        CGF.getContext().getCharWidth() - 1;
      CharUnits MemcpySize =
        CGF.getContext().toCharUnitsFromBits(MemcpySizeBits);
      return MemcpySize;
    }

    void emitMemcpy() {
      // Give the subclass a chance to bail out if it feels the memcpy isn't
      // worth it (e.g. Hasn't aggregated enough data).
      if (!FirstField) {
        return;
      }

      uint64_t FirstByteOffset;
      if (FirstField->isBitField()) {
        const CGRecordLayout &RL =
          CGF.getTypes().getCGRecordLayout(FirstField->getParent());
        const CGBitFieldInfo &BFInfo = RL.getBitFieldInfo(FirstField);
        // FirstFieldOffset is not appropriate for bitfields,
        // we need to use the storage offset instead.
        FirstByteOffset = CGF.getContext().toBits(BFInfo.StorageOffset);
      } else {
        FirstByteOffset = FirstFieldOffset;
      }

      CharUnits MemcpySize = getMemcpySize(FirstByteOffset);
      QualType RecordTy = CGF.getContext().getTypeDeclType(ClassDecl);
      Address ThisPtr = CGF.LoadCXXThisAddress();
      LValue DestLV = CGF.MakeAddrLValue(ThisPtr, RecordTy);
      LValue Dest = CGF.EmitLValueForFieldInitialization(DestLV, FirstField);
      llvm::Value *SrcPtr = CGF.Builder.CreateLoad(CGF.GetAddrOfLocalVar(SrcRec));
      LValue SrcLV = CGF.MakeNaturalAlignAddrLValue(SrcPtr, RecordTy);
      LValue Src = CGF.EmitLValueForFieldInitialization(SrcLV, FirstField);

      emitMemcpyIR(Dest.isBitField() ? Dest.getBitFieldAddress() : Dest.getAddress(),
                   Src.isBitField() ? Src.getBitFieldAddress() : Src.getAddress(),
                   MemcpySize);
      reset();
    }

    void reset() {
      FirstField = nullptr;
    }

  protected:
    CodeGenFunction &CGF;
    const CXXRecordDecl *ClassDecl;

  private:
    void emitMemcpyIR(Address DestPtr, Address SrcPtr, CharUnits Size) {
      llvm::PointerType *DPT = DestPtr.getType();
      llvm::Type *DBP =
        llvm::Type::getInt8PtrTy(CGF.getLLVMContext(), DPT->getAddressSpace());
      DestPtr = CGF.Builder.CreateBitCast(DestPtr, DBP);

      llvm::PointerType *SPT = SrcPtr.getType();
      llvm::Type *SBP =
        llvm::Type::getInt8PtrTy(CGF.getLLVMContext(), SPT->getAddressSpace());
      SrcPtr = CGF.Builder.CreateBitCast(SrcPtr, SBP);

      CGF.Builder.CreateMemCpy(DestPtr, SrcPtr, Size.getQuantity());
    }

    void addInitialField(FieldDecl *F) {
      FirstField = F;
      LastField = F;
      FirstFieldOffset = RecLayout.getFieldOffset(F->getFieldIndex());
      LastFieldOffset = FirstFieldOffset;
      LastAddedFieldIndex = F->getFieldIndex();
    }

    void addNextField(FieldDecl *F) {
      // For the most part, the following invariant will hold:
      //   F->getFieldIndex() == LastAddedFieldIndex + 1
      // The one exception is that Sema won't add a copy-initializer for an
      // unnamed bitfield, which will show up here as a gap in the sequence.
      assert(F->getFieldIndex() >= LastAddedFieldIndex + 1 &&
             "Cannot aggregate fields out of order.");
      LastAddedFieldIndex = F->getFieldIndex();

      // The 'first' and 'last' fields are chosen by offset, rather than field
      // index. This allows the code to support bitfields, as well as regular
      // fields.
      uint64_t FOffset = RecLayout.getFieldOffset(F->getFieldIndex());
      if (FOffset < FirstFieldOffset) {
        FirstField = F;
        FirstFieldOffset = FOffset;
      } else if (FOffset > LastFieldOffset) {
        LastField = F;
        LastFieldOffset = FOffset;
      }
    }

    const VarDecl *SrcRec;
    const ASTRecordLayout &RecLayout;
    FieldDecl *FirstField;
    FieldDecl *LastField;
    uint64_t FirstFieldOffset, LastFieldOffset;
    unsigned LastAddedFieldIndex;
  };

  class ConstructorMemcpyizer : public FieldMemcpyizer {
  private:
    /// Get source argument for copy constructor. Returns null if not a copy
    /// constructor.
    static const VarDecl *getTrivialCopySource(CodeGenFunction &CGF,
                                               const CXXConstructorDecl *CD,
                                               FunctionArgList &Args) {
      if (CD->isCopyOrMoveConstructor() && CD->isDefaulted())
        return Args[CGF.CGM.getCXXABI().getSrcArgforCopyCtor(CD, Args)];
      return nullptr;
    }

    // Returns true if a CXXCtorInitializer represents a member initialization
    // that can be rolled into a memcpy.
    bool isMemberInitMemcpyable(CXXCtorInitializer *MemberInit) const {
      if (!MemcpyableCtor)
        return false;
      FieldDecl *Field = MemberInit->getMember();
      assert(Field && "No field for member init.");
      QualType FieldType = Field->getType();
      CXXConstructExpr *CE = dyn_cast<CXXConstructExpr>(MemberInit->getInit());

      // Bail out on non-memcpyable, not-trivially-copyable members.
      if (!(CE && isMemcpyEquivalentSpecialMember(CE->getConstructor())) &&
          !(FieldType.isTriviallyCopyableType(CGF.getContext()) ||
            FieldType->isReferenceType()))
        return false;

      // Bail out on volatile fields.
      if (!isMemcpyableField(Field))
        return false;

      // Otherwise we're good.
      return true;
    }

  public:
    ConstructorMemcpyizer(CodeGenFunction &CGF, const CXXConstructorDecl *CD,
                          FunctionArgList &Args)
      : FieldMemcpyizer(CGF, CD->getParent(), getTrivialCopySource(CGF, CD, Args)),
        ConstructorDecl(CD),
        MemcpyableCtor(CD->isDefaulted() &&
                       CD->isCopyOrMoveConstructor() &&
                       CGF.getLangOpts().getGC() == LangOptions::NonGC),
        Args(Args) { }

    void addMemberInitializer(CXXCtorInitializer *MemberInit) {
      if (isMemberInitMemcpyable(MemberInit)) {
        AggregatedInits.push_back(MemberInit);
        addMemcpyableField(MemberInit->getMember());
      } else {
        emitAggregatedInits();
        EmitMemberInitializer(CGF, ConstructorDecl->getParent(), MemberInit,
                              ConstructorDecl, Args);
      }
    }

    void emitAggregatedInits() {
      if (AggregatedInits.size() <= 1) {
        // This memcpy is too small to be worthwhile. Fall back on default
        // codegen.
        if (!AggregatedInits.empty()) {
          CopyingValueRepresentation CVR(CGF);
          EmitMemberInitializer(CGF, ConstructorDecl->getParent(),
                                AggregatedInits[0], ConstructorDecl, Args);
          AggregatedInits.clear();
        }
        reset();
        return;
      }

      pushEHDestructors();
      emitMemcpy();
      AggregatedInits.clear();
    }

    void pushEHDestructors() {
      Address ThisPtr = CGF.LoadCXXThisAddress();
      QualType RecordTy = CGF.getContext().getTypeDeclType(ClassDecl);
      LValue LHS = CGF.MakeAddrLValue(ThisPtr, RecordTy);

      for (unsigned i = 0; i < AggregatedInits.size(); ++i) {
        CXXCtorInitializer *MemberInit = AggregatedInits[i];
        QualType FieldType = MemberInit->getAnyMember()->getType();
        QualType::DestructionKind dtorKind = FieldType.isDestructedType();
        if (!CGF.needsEHCleanup(dtorKind))
          continue;
        LValue FieldLHS = LHS;
        EmitLValueForAnyFieldInitialization(CGF, MemberInit, FieldLHS);
        CGF.pushEHDestroy(dtorKind, FieldLHS.getAddress(), FieldType);
      }
    }

    void finish() {
      emitAggregatedInits();
    }

  private:
    const CXXConstructorDecl *ConstructorDecl;
    bool MemcpyableCtor;
    FunctionArgList &Args;
    SmallVector<CXXCtorInitializer*, 16> AggregatedInits;
  };

  class AssignmentMemcpyizer : public FieldMemcpyizer {
  private:
    // Returns the memcpyable field copied by the given statement, if one
    // exists. Otherwise returns null.
    FieldDecl *getMemcpyableField(Stmt *S) {
      if (!AssignmentsMemcpyable)
        return nullptr;
      if (BinaryOperator *BO = dyn_cast<BinaryOperator>(S)) {
        // Recognise trivial assignments.
        if (BO->getOpcode() != BO_Assign)
          return nullptr;
        MemberExpr *ME = dyn_cast<MemberExpr>(BO->getLHS());
        if (!ME)
          return nullptr;
        FieldDecl *Field = dyn_cast<FieldDecl>(ME->getMemberDecl());
        if (!Field || !isMemcpyableField(Field))
          return nullptr;
        Stmt *RHS = BO->getRHS();
        if (ImplicitCastExpr *EC = dyn_cast<ImplicitCastExpr>(RHS))
          RHS = EC->getSubExpr();
        if (!RHS)
          return nullptr;
        MemberExpr *ME2 = dyn_cast<MemberExpr>(RHS);
        if (dyn_cast<FieldDecl>(ME2->getMemberDecl()) != Field)
          return nullptr;
        return Field;
      } else if (CXXMemberCallExpr *MCE = dyn_cast<CXXMemberCallExpr>(S)) {
        CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(MCE->getCalleeDecl());
        if (!(MD && isMemcpyEquivalentSpecialMember(MD)))
          return nullptr;
        MemberExpr *IOA = dyn_cast<MemberExpr>(MCE->getImplicitObjectArgument());
        if (!IOA)
          return nullptr;
        FieldDecl *Field = dyn_cast<FieldDecl>(IOA->getMemberDecl());
        if (!Field || !isMemcpyableField(Field))
          return nullptr;
        MemberExpr *Arg0 = dyn_cast<MemberExpr>(MCE->getArg(0));
        if (!Arg0 || Field != dyn_cast<FieldDecl>(Arg0->getMemberDecl()))
          return nullptr;
        return Field;
      } else if (CallExpr *CE = dyn_cast<CallExpr>(S)) {
        FunctionDecl *FD = dyn_cast<FunctionDecl>(CE->getCalleeDecl());
        if (!FD || FD->getBuiltinID() != Builtin::BI__builtin_memcpy)
          return nullptr;
        Expr *DstPtr = CE->getArg(0);
        if (ImplicitCastExpr *DC = dyn_cast<ImplicitCastExpr>(DstPtr))
          DstPtr = DC->getSubExpr();
        UnaryOperator *DUO = dyn_cast<UnaryOperator>(DstPtr);
        if (!DUO || DUO->getOpcode() != UO_AddrOf)
          return nullptr;
        MemberExpr *ME = dyn_cast<MemberExpr>(DUO->getSubExpr());
        if (!ME)
          return nullptr;
        FieldDecl *Field = dyn_cast<FieldDecl>(ME->getMemberDecl());
        if (!Field || !isMemcpyableField(Field))
          return nullptr;
        Expr *SrcPtr = CE->getArg(1);
        if (ImplicitCastExpr *SC = dyn_cast<ImplicitCastExpr>(SrcPtr))
          SrcPtr = SC->getSubExpr();
        UnaryOperator *SUO = dyn_cast<UnaryOperator>(SrcPtr);
        if (!SUO || SUO->getOpcode() != UO_AddrOf)
          return nullptr;
        MemberExpr *ME2 = dyn_cast<MemberExpr>(SUO->getSubExpr());
        if (!ME2 || Field != dyn_cast<FieldDecl>(ME2->getMemberDecl()))
          return nullptr;
        return Field;
      }

      return nullptr;
    }

    bool AssignmentsMemcpyable;
    SmallVector<Stmt*, 16> AggregatedStmts;

  public:
    AssignmentMemcpyizer(CodeGenFunction &CGF, const CXXMethodDecl *AD,
                         FunctionArgList &Args)
      : FieldMemcpyizer(CGF, AD->getParent(), Args[Args.size() - 1]),
        AssignmentsMemcpyable(CGF.getLangOpts().getGC() == LangOptions::NonGC) {
      assert(Args.size() == 2);
    }

    void emitAssignment(Stmt *S) {
      FieldDecl *F = getMemcpyableField(S);
      if (F) {
        addMemcpyableField(F);
        AggregatedStmts.push_back(S);
      } else {
        emitAggregatedStmts();
        CGF.EmitStmt(S);
      }
    }

    void emitAggregatedStmts() {
      if (AggregatedStmts.size() <= 1) {
        if (!AggregatedStmts.empty()) {
          CopyingValueRepresentation CVR(CGF);
          CGF.EmitStmt(AggregatedStmts[0]);
        }
        reset();
      }

      emitMemcpy();
      AggregatedStmts.clear();
    }

    void finish() {
      emitAggregatedStmts();
    }
  };
} // end anonymous namespace

static bool isInitializerOfDynamicClass(const CXXCtorInitializer *BaseInit) {
  const Type *BaseType = BaseInit->getBaseClass();
  const auto *BaseClassDecl =
          cast<CXXRecordDecl>(BaseType->getAs<RecordType>()->getDecl());
  return BaseClassDecl->isDynamicClass();
}

/// EmitCtorPrologue - This routine generates necessary code to initialize
/// base classes and non-static data members belonging to this constructor.
void CodeGenFunction::EmitCtorPrologue(const CXXConstructorDecl *CD,
                                       CXXCtorType CtorType,
                                       FunctionArgList &Args) {
  if (CD->isDelegatingConstructor())
    return EmitDelegatingCXXConstructorCall(CD, Args);

  const CXXRecordDecl *ClassDecl = CD->getParent();

  CXXConstructorDecl::init_const_iterator B = CD->init_begin(),
                                          E = CD->init_end();

  llvm::BasicBlock *BaseCtorContinueBB = nullptr;
  if (ClassDecl->getNumVBases() &&
      !CGM.getTarget().getCXXABI().hasConstructorVariants()) {
    // The ABIs that don't have constructor variants need to put a branch
    // before the virtual base initialization code.
    BaseCtorContinueBB =
      CGM.getCXXABI().EmitCtorCompleteObjectHandler(*this, ClassDecl);
    assert(BaseCtorContinueBB);
  }

  llvm::Value *const OldThis = CXXThisValue;
  // Virtual base initializers first.
  for (; B != E && (*B)->isBaseInitializer() && (*B)->isBaseVirtual(); B++) {
    if (CGM.getCodeGenOpts().StrictVTablePointers &&
        CGM.getCodeGenOpts().OptimizationLevel > 0 &&
        isInitializerOfDynamicClass(*B))
      CXXThisValue = Builder.CreateInvariantGroupBarrier(LoadCXXThis());
    EmitBaseInitializer(*this, ClassDecl, *B, CtorType);
  }

  if (BaseCtorContinueBB) {
    // Complete object handler should continue to the remaining initializers.
    Builder.CreateBr(BaseCtorContinueBB);
    EmitBlock(BaseCtorContinueBB);
  }

  // Then, non-virtual base initializers.
  for (; B != E && (*B)->isBaseInitializer(); B++) {
    assert(!(*B)->isBaseVirtual());

    if (CGM.getCodeGenOpts().StrictVTablePointers &&
        CGM.getCodeGenOpts().OptimizationLevel > 0 &&
        isInitializerOfDynamicClass(*B))
      CXXThisValue = Builder.CreateInvariantGroupBarrier(LoadCXXThis());
    EmitBaseInitializer(*this, ClassDecl, *B, CtorType);
  }

  CXXThisValue = OldThis;

  InitializeVTablePointers(ClassDecl);

  // And finally, initialize class members.
  FieldConstructionScope FCS(*this, LoadCXXThisAddress());
  ConstructorMemcpyizer CM(*this, CD, Args);
  for (; B != E; B++) {
    CXXCtorInitializer *Member = (*B);
    assert(!Member->isBaseInitializer());
    assert(Member->isAnyMemberInitializer() &&
           "Delegating initializer on non-delegating constructor");
    CM.addMemberInitializer(Member);
  }
  CM.finish();
}

static bool
FieldHasTrivialDestructorBody(ASTContext &Context, const FieldDecl *Field);

static bool
HasTrivialDestructorBody(ASTContext &Context,
                         const CXXRecordDecl *BaseClassDecl,
                         const CXXRecordDecl *MostDerivedClassDecl)
{
  // If the destructor is trivial we don't have to check anything else.
  if (BaseClassDecl->hasTrivialDestructor())
    return true;

  if (!BaseClassDecl->getDestructor()->hasTrivialBody())
    return false;

  // Check fields.
  for (const auto *Field : BaseClassDecl->fields())
    if (!FieldHasTrivialDestructorBody(Context, Field))
      return false;

  // Check non-virtual bases.
  for (const auto &I : BaseClassDecl->bases()) {
    if (I.isVirtual())
      continue;

    const CXXRecordDecl *NonVirtualBase =
      cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
    if (!HasTrivialDestructorBody(Context, NonVirtualBase,
                                  MostDerivedClassDecl))
      return false;
  }

  if (BaseClassDecl == MostDerivedClassDecl) {
    // Check virtual bases.
    for (const auto &I : BaseClassDecl->vbases()) {
      const CXXRecordDecl *VirtualBase =
        cast<CXXRecordDecl>(I.getType()->castAs<RecordType>()->getDecl());
      if (!HasTrivialDestructorBody(Context, VirtualBase,
                                    MostDerivedClassDecl))
        return false;
    }
  }

  return true;
}

static bool
FieldHasTrivialDestructorBody(ASTContext &Context,
                                          const FieldDecl *Field)
{
  QualType FieldBaseElementType = Context.getBaseElementType(Field->getType());

  const RecordType *RT = FieldBaseElementType->getAs<RecordType>();
  if (!RT)
    return true;

  CXXRecordDecl *FieldClassDecl = cast<CXXRecordDecl>(RT->getDecl());

  // The destructor for an implicit anonymous union member is never invoked.
  if (FieldClassDecl->isUnion() && FieldClassDecl->isAnonymousStructOrUnion())
    return false;

  return HasTrivialDestructorBody(Context, FieldClassDecl, FieldClassDecl);
}

/// CanSkipVTablePointerInitialization - Check whether we need to initialize
/// any vtable pointers before calling this destructor.
static bool CanSkipVTablePointerInitialization(CodeGenFunction &CGF,
                                               const CXXDestructorDecl *Dtor) {
  const CXXRecordDecl *ClassDecl = Dtor->getParent();
  if (!ClassDecl->isDynamicClass())
    return true;

  if (!Dtor->hasTrivialBody())
    return false;

  // Check the fields.
  for (const auto *Field : ClassDecl->fields())
    if (!FieldHasTrivialDestructorBody(CGF.getContext(), Field))
      return false;

  return true;
}

/// EmitDestructorBody - Emits the body of the current destructor.
void CodeGenFunction::EmitDestructorBody(FunctionArgList &Args) {
  const CXXDestructorDecl *Dtor = cast<CXXDestructorDecl>(CurGD.getDecl());
  CXXDtorType DtorType = CurGD.getDtorType();

  Stmt *Body = Dtor->getBody();
  if (Body)
    incrementProfileCounter(Body);

  // The call to operator delete in a deleting destructor happens
  // outside of the function-try-block, which means it's always
  // possible to delegate the destructor body to the complete
  // destructor.  Do so.
  if (DtorType == Dtor_Deleting) {
    EnterDtorCleanups(Dtor, Dtor_Deleting);
    EmitCXXDestructorCall(Dtor, Dtor_Complete, /*ForVirtualBase=*/false,
                          /*Delegating=*/false, LoadCXXThisAddress());
    PopCleanupBlock();
    return;
  }

  // If the body is a function-try-block, enter the try before
  // anything else.
  bool isTryBody = (Body && isa<CXXTryStmt>(Body));
  if (isTryBody)
    EnterCXXTryStmt(*cast<CXXTryStmt>(Body), true);
  EmitAsanPrologueOrEpilogue(false);

  // Enter the epilogue cleanups.
  RunCleanupsScope DtorEpilogue(*this);

  // If this is the complete variant, just invoke the base variant;
  // the epilogue will destruct the virtual bases.  But we can't do
  // this optimization if the body is a function-try-block, because
  // we'd introduce *two* handler blocks.  In the Microsoft ABI, we
  // always delegate because we might not have a definition in this TU.
  switch (DtorType) {
  case Dtor_Comdat:
    llvm_unreachable("not expecting a COMDAT");

  case Dtor_Deleting: llvm_unreachable("already handled deleting case");

  case Dtor_Complete:
    assert((Body || getTarget().getCXXABI().isMicrosoft()) &&
           "can't emit a dtor without a body for non-Microsoft ABIs");

    // Enter the cleanup scopes for virtual bases.
    EnterDtorCleanups(Dtor, Dtor_Complete);

    if (!isTryBody) {
      EmitCXXDestructorCall(Dtor, Dtor_Base, /*ForVirtualBase=*/false,
                            /*Delegating=*/false, LoadCXXThisAddress());
      break;
    }
    // Fallthrough: act like we're in the base variant.

  case Dtor_Base:
    assert(Body);

    // Enter the cleanup scopes for fields and non-virtual bases.
    EnterDtorCleanups(Dtor, Dtor_Base);

    // Initialize the vtable pointers before entering the body.
    if (!CanSkipVTablePointerInitialization(*this, Dtor)) {
      // Insert the llvm.invariant.group.barrier intrinsic before initializing
      // the vptrs to cancel any previous assumptions we might have made.
      if (CGM.getCodeGenOpts().StrictVTablePointers &&
          CGM.getCodeGenOpts().OptimizationLevel > 0)
        CXXThisValue = Builder.CreateInvariantGroupBarrier(LoadCXXThis());
      InitializeVTablePointers(Dtor->getParent());
    }

    if (isTryBody)
      EmitStmt(cast<CXXTryStmt>(Body)->getTryBlock());
    else if (Body)
      EmitStmt(Body);
    else {
      assert(Dtor->isImplicit() && "bodyless dtor not implicit");
      // nothing to do besides what's in the epilogue
    }
    // -fapple-kext must inline any call to this dtor into
    // the caller's body.
    if (getLangOpts().AppleKext)
      CurFn->addFnAttr(llvm::Attribute::AlwaysInline);

    break;
  }

  // Jump out through the epilogue cleanups.
  DtorEpilogue.ForceCleanup();

  // Exit the try if applicable.
  if (isTryBody)
    ExitCXXTryStmt(*cast<CXXTryStmt>(Body), true);
}

void CodeGenFunction::emitImplicitAssignmentOperatorBody(FunctionArgList &Args) {
  const CXXMethodDecl *AssignOp = cast<CXXMethodDecl>(CurGD.getDecl());
  const Stmt *RootS = AssignOp->getBody();
  assert(isa<CompoundStmt>(RootS) &&
         "Body of an implicit assignment operator should be compound stmt.");
  const CompoundStmt *RootCS = cast<CompoundStmt>(RootS);

  LexicalScope Scope(*this, RootCS->getSourceRange());

  incrementProfileCounter(RootCS);
  AssignmentMemcpyizer AM(*this, AssignOp, Args);
  for (auto *I : RootCS->body())
    AM.emitAssignment(I);
  AM.finish();
}

namespace {
  /// Call the operator delete associated with the current destructor.
  struct CallDtorDelete final : EHScopeStack::Cleanup {
    CallDtorDelete() {}

    void Emit(CodeGenFunction &CGF, Flags flags) override {
      const CXXDestructorDecl *Dtor = cast<CXXDestructorDecl>(CGF.CurCodeDecl);
      const CXXRecordDecl *ClassDecl = Dtor->getParent();
      CGF.EmitDeleteCall(Dtor->getOperatorDelete(), CGF.LoadCXXThis(),
                         CGF.getContext().getTagDeclType(ClassDecl));
    }
  };

  struct CallDtorDeleteConditional final : EHScopeStack::Cleanup {
    llvm::Value *ShouldDeleteCondition;

  public:
    CallDtorDeleteConditional(llvm::Value *ShouldDeleteCondition)
        : ShouldDeleteCondition(ShouldDeleteCondition) {
      assert(ShouldDeleteCondition != nullptr);
    }

    void Emit(CodeGenFunction &CGF, Flags flags) override {
      llvm::BasicBlock *callDeleteBB = CGF.createBasicBlock("dtor.call_delete");
      llvm::BasicBlock *continueBB = CGF.createBasicBlock("dtor.continue");
      llvm::Value *ShouldCallDelete
        = CGF.Builder.CreateIsNull(ShouldDeleteCondition);
      CGF.Builder.CreateCondBr(ShouldCallDelete, continueBB, callDeleteBB);

      CGF.EmitBlock(callDeleteBB);
      const CXXDestructorDecl *Dtor = cast<CXXDestructorDecl>(CGF.CurCodeDecl);
      const CXXRecordDecl *ClassDecl = Dtor->getParent();
      CGF.EmitDeleteCall(Dtor->getOperatorDelete(), CGF.LoadCXXThis(),
                         CGF.getContext().getTagDeclType(ClassDecl));
      CGF.Builder.CreateBr(continueBB);

      CGF.EmitBlock(continueBB);
    }
  };

  class DestroyField  final : public EHScopeStack::Cleanup {
    const FieldDecl *field;
    CodeGenFunction::Destroyer *destroyer;
    bool useEHCleanupForArray;

  public:
    DestroyField(const FieldDecl *field, CodeGenFunction::Destroyer *destroyer,
                 bool useEHCleanupForArray)
        : field(field), destroyer(destroyer),
          useEHCleanupForArray(useEHCleanupForArray) {}

    void Emit(CodeGenFunction &CGF, Flags flags) override {
      // Find the address of the field.
      Address thisValue = CGF.LoadCXXThisAddress();
      QualType RecordTy = CGF.getContext().getTagDeclType(field->getParent());
      LValue ThisLV = CGF.MakeAddrLValue(thisValue, RecordTy);
      LValue LV = CGF.EmitLValueForField(ThisLV, field);
      assert(LV.isSimple());

      CGF.emitDestroy(LV.getAddress(), field->getType(), destroyer,
                      flags.isForNormalCleanup() && useEHCleanupForArray);
    }
  };

 static void EmitSanitizerDtorCallback(CodeGenFunction &CGF, llvm::Value *Ptr,
             CharUnits::QuantityType PoisonSize) {
   // Pass in void pointer and size of region as arguments to runtime
   // function
   llvm::Value *Args[] = {CGF.Builder.CreateBitCast(Ptr, CGF.VoidPtrTy),
                          llvm::ConstantInt::get(CGF.SizeTy, PoisonSize)};

   llvm::Type *ArgTypes[] = {CGF.VoidPtrTy, CGF.SizeTy};

   llvm::FunctionType *FnType =
       llvm::FunctionType::get(CGF.VoidTy, ArgTypes, false);
   llvm::Value *Fn =
       CGF.CGM.CreateRuntimeFunction(FnType, "__sanitizer_dtor_callback");
   CGF.EmitNounwindRuntimeCall(Fn, Args);
 }

  class SanitizeDtorMembers final : public EHScopeStack::Cleanup {
    const CXXDestructorDecl *Dtor;

  public:
    SanitizeDtorMembers(const CXXDestructorDecl *Dtor) : Dtor(Dtor) {}

    // Generate function call for handling object poisoning.
    // Disables tail call elimination, to prevent the current stack frame
    // from disappearing from the stack trace.
    void Emit(CodeGenFunction &CGF, Flags flags) override {
      const ASTRecordLayout &Layout =
          CGF.getContext().getASTRecordLayout(Dtor->getParent());

      // Nothing to poison.
      if (Layout.getFieldCount() == 0)
        return;

      // Prevent the current stack frame from disappearing from the stack trace.
      CGF.CurFn->addFnAttr("disable-tail-calls", "true");

      // Construct pointer to region to begin poisoning, and calculate poison
      // size, so that only members declared in this class are poisoned.
      ASTContext &Context = CGF.getContext();
      unsigned fieldIndex = 0;
      int startIndex = -1;
      // RecordDecl::field_iterator Field;
      for (const FieldDecl *Field : Dtor->getParent()->fields()) {
        // Poison field if it is trivial
        if (FieldHasTrivialDestructorBody(Context, Field)) {
          // Start sanitizing at this field
          if (startIndex < 0)
            startIndex = fieldIndex;

          // Currently on the last field, and it must be poisoned with the
          // current block.
          if (fieldIndex == Layout.getFieldCount() - 1) {
            PoisonMembers(CGF, startIndex, Layout.getFieldCount());
          }
        } else if (startIndex >= 0) {
          // No longer within a block of memory to poison, so poison the block
          PoisonMembers(CGF, startIndex, fieldIndex);
          // Re-set the start index
          startIndex = -1;
        }
        fieldIndex += 1;
      }
    }

  private:
    /// \param layoutStartOffset index of the ASTRecordLayout field to
    ///     start poisoning (inclusive)
    /// \param layoutEndOffset index of the ASTRecordLayout field to
    ///     end poisoning (exclusive)
    void PoisonMembers(CodeGenFunction &CGF, unsigned layoutStartOffset,
                     unsigned layoutEndOffset) {
      ASTContext &Context = CGF.getContext();
      const ASTRecordLayout &Layout =
          Context.getASTRecordLayout(Dtor->getParent());

      llvm::ConstantInt *OffsetSizePtr = llvm::ConstantInt::get(
          CGF.SizeTy,
          Context.toCharUnitsFromBits(Layout.getFieldOffset(layoutStartOffset))
              .getQuantity());

      llvm::Value *OffsetPtr = CGF.Builder.CreateGEP(
          CGF.Builder.CreateBitCast(CGF.LoadCXXThis(), CGF.Int8PtrTy),
          OffsetSizePtr);

      CharUnits::QuantityType PoisonSize;
      if (layoutEndOffset >= Layout.getFieldCount()) {
        PoisonSize = Layout.getNonVirtualSize().getQuantity() -
                     Context.toCharUnitsFromBits(
                                Layout.getFieldOffset(layoutStartOffset))
                         .getQuantity();
      } else {
        PoisonSize = Context.toCharUnitsFromBits(
                                Layout.getFieldOffset(layoutEndOffset) -
                                Layout.getFieldOffset(layoutStartOffset))
                         .getQuantity();
      }

      if (PoisonSize == 0)
        return;

      EmitSanitizerDtorCallback(CGF, OffsetPtr, PoisonSize);
    }
  };

 class SanitizeDtorVTable final : public EHScopeStack::Cleanup {
    const CXXDestructorDecl *Dtor;

  public:
    SanitizeDtorVTable(const CXXDestructorDecl *Dtor) : Dtor(Dtor) {}

    // Generate function call for handling vtable pointer poisoning.
    void Emit(CodeGenFunction &CGF, Flags flags) override {
      assert(Dtor->getParent()->isDynamicClass());
      (void)Dtor;
      ASTContext &Context = CGF.getContext();
      // Poison vtable and vtable ptr if they exist for this class.
      llvm::Value *VTablePtr = CGF.LoadCXXThis();

      CharUnits::QuantityType PoisonSize =
          Context.toCharUnitsFromBits(CGF.PointerWidthInBits).getQuantity();
      // Pass in void pointer and size of region as arguments to runtime
      // function
      EmitSanitizerDtorCallback(CGF, VTablePtr, PoisonSize);
    }
 };
} // end anonymous namespace

/// \brief Emit all code that comes at the end of class's
/// destructor. This is to call destructors on members and base classes
/// in reverse order of their construction.
void CodeGenFunction::EnterDtorCleanups(const CXXDestructorDecl *DD,
                                        CXXDtorType DtorType) {
  assert((!DD->isTrivial() || DD->hasAttr<DLLExportAttr>()) &&
         "Should not emit dtor epilogue for non-exported trivial dtor!");

  // The deleting-destructor phase just needs to call the appropriate
  // operator delete that Sema picked up.
  if (DtorType == Dtor_Deleting) {
    assert(DD->getOperatorDelete() &&
           "operator delete missing - EnterDtorCleanups");
    if (CXXStructorImplicitParamValue) {
      // If there is an implicit param to the deleting dtor, it's a boolean
      // telling whether we should call delete at the end of the dtor.
      EHStack.pushCleanup<CallDtorDeleteConditional>(
          NormalAndEHCleanup, CXXStructorImplicitParamValue);
    } else {
      EHStack.pushCleanup<CallDtorDelete>(NormalAndEHCleanup);
    }
    return;
  }

  const CXXRecordDecl *ClassDecl = DD->getParent();

  // Unions have no bases and do not call field destructors.
  if (ClassDecl->isUnion())
    return;

  // The complete-destructor phase just destructs all the virtual bases.
  if (DtorType == Dtor_Complete) {
    // Poison the vtable pointer such that access after the base
    // and member destructors are invoked is invalid.
    if (CGM.getCodeGenOpts().SanitizeMemoryUseAfterDtor &&
        SanOpts.has(SanitizerKind::Memory) && ClassDecl->getNumVBases() &&
        ClassDecl->isPolymorphic())
      EHStack.pushCleanup<SanitizeDtorVTable>(NormalAndEHCleanup, DD);

    // We push them in the forward order so that they'll be popped in
    // the reverse order.
    for (const auto &Base : ClassDecl->vbases()) {
      CXXRecordDecl *BaseClassDecl
        = cast<CXXRecordDecl>(Base.getType()->getAs<RecordType>()->getDecl());

      // Ignore trivial destructors.
      if (BaseClassDecl->hasTrivialDestructor())
        continue;

      EHStack.pushCleanup<CallBaseDtor>(NormalAndEHCleanup,
                                        BaseClassDecl,
                                        /*BaseIsVirtual*/ true);
    }

    return;
  }

  assert(DtorType == Dtor_Base);
  // Poison the vtable pointer if it has no virtual bases, but inherits
  // virtual functions.
  if (CGM.getCodeGenOpts().SanitizeMemoryUseAfterDtor &&
      SanOpts.has(SanitizerKind::Memory) && !ClassDecl->getNumVBases() &&
      ClassDecl->isPolymorphic())
    EHStack.pushCleanup<SanitizeDtorVTable>(NormalAndEHCleanup, DD);

  // Destroy non-virtual bases.
  for (const auto &Base : ClassDecl->bases()) {
    // Ignore virtual bases.
    if (Base.isVirtual())
      continue;

    CXXRecordDecl *BaseClassDecl = Base.getType()->getAsCXXRecordDecl();

    // Ignore trivial destructors.
    if (BaseClassDecl->hasTrivialDestructor())
      continue;

    EHStack.pushCleanup<CallBaseDtor>(NormalAndEHCleanup,
                                      BaseClassDecl,
                                      /*BaseIsVirtual*/ false);
  }

  // Poison fields such that access after their destructors are
  // invoked, and before the base class destructor runs, is invalid.
  if (CGM.getCodeGenOpts().SanitizeMemoryUseAfterDtor &&
      SanOpts.has(SanitizerKind::Memory))
    EHStack.pushCleanup<SanitizeDtorMembers>(NormalAndEHCleanup, DD);

  // Destroy direct fields.
  for (const auto *Field : ClassDecl->fields()) {
    QualType type = Field->getType();
    QualType::DestructionKind dtorKind = type.isDestructedType();
    if (!dtorKind) continue;

    // Anonymous union members do not have their destructors called.
    const RecordType *RT = type->getAsUnionType();
    if (RT && RT->getDecl()->isAnonymousStructOrUnion()) continue;

    CleanupKind cleanupKind = getCleanupKind(dtorKind);
    EHStack.pushCleanup<DestroyField>(cleanupKind, Field,
                                      getDestroyer(dtorKind),
                                      cleanupKind & EHCleanup);
  }
}

/// EmitCXXAggrConstructorCall - Emit a loop to call a particular
/// constructor for each of several members of an array.
///
/// \param ctor the constructor to call for each element
/// \param arrayType the type of the array to initialize
/// \param arrayBegin an arrayType*
/// \param zeroInitialize true if each element should be
///   zero-initialized before it is constructed
void CodeGenFunction::EmitCXXAggrConstructorCall(
    const CXXConstructorDecl *ctor, const ArrayType *arrayType,
    Address arrayBegin, const CXXConstructExpr *E, bool zeroInitialize) {
  QualType elementType;
  llvm::Value *numElements =
    emitArrayLength(arrayType, elementType, arrayBegin);

  EmitCXXAggrConstructorCall(ctor, numElements, arrayBegin, E, zeroInitialize);
}

/// EmitCXXAggrConstructorCall - Emit a loop to call a particular
/// constructor for each of several members of an array.
///
/// \param ctor the constructor to call for each element
/// \param numElements the number of elements in the array;
///   may be zero
/// \param arrayBase a T*, where T is the type constructed by ctor
/// \param zeroInitialize true if each element should be
///   zero-initialized before it is constructed
void CodeGenFunction::EmitCXXAggrConstructorCall(const CXXConstructorDecl *ctor,
                                                 llvm::Value *numElements,
                                                 Address arrayBase,
                                                 const CXXConstructExpr *E,
                                                 bool zeroInitialize) {
  // It's legal for numElements to be zero.  This can happen both
  // dynamically, because x can be zero in 'new A[x]', and statically,
  // because of GCC extensions that permit zero-length arrays.  There
  // are probably legitimate places where we could assume that this
  // doesn't happen, but it's not clear that it's worth it.
  llvm::BranchInst *zeroCheckBranch = nullptr;

  // Optimize for a constant count.
  llvm::ConstantInt *constantCount
    = dyn_cast<llvm::ConstantInt>(numElements);
  if (constantCount) {
    // Just skip out if the constant count is zero.
    if (constantCount->isZero()) return;

  // Otherwise, emit the check.
  } else {
    llvm::BasicBlock *loopBB = createBasicBlock("new.ctorloop");
    llvm::Value *iszero = Builder.CreateIsNull(numElements, "isempty");
    zeroCheckBranch = Builder.CreateCondBr(iszero, loopBB, loopBB);
    EmitBlock(loopBB);
  }

  // Find the end of the array.
  llvm::Value *arrayBegin = arrayBase.getPointer();
  llvm::Value *arrayEnd = Builder.CreateInBoundsGEP(arrayBegin, numElements,
                                                    "arrayctor.end");

  // Enter the loop, setting up a phi for the current location to initialize.
  llvm::BasicBlock *entryBB = Builder.GetInsertBlock();
  llvm::BasicBlock *loopBB = createBasicBlock("arrayctor.loop");
  EmitBlock(loopBB);
  llvm::PHINode *cur = Builder.CreatePHI(arrayBegin->getType(), 2,
                                         "arrayctor.cur");
  cur->addIncoming(arrayBegin, entryBB);

  // Inside the loop body, emit the constructor call on the array element.

  // The alignment of the base, adjusted by the size of a single element,
  // provides a conservative estimate of the alignment of every element.
  // (This assumes we never start tracking offsetted alignments.)
  // 
  // Note that these are complete objects and so we don't need to
  // use the non-virtual size or alignment.
  QualType type = getContext().getTypeDeclType(ctor->getParent());
  CharUnits eltAlignment =
    arrayBase.getAlignment()
             .alignmentOfArrayElement(getContext().getTypeSizeInChars(type));
  Address curAddr = Address(cur, eltAlignment);

  // Zero initialize the storage, if requested.
  if (zeroInitialize)
    EmitNullInitialization(curAddr, type);

  // C++ [class.temporary]p4:
  // There are two contexts in which temporaries are destroyed at a different
  // point than the end of the full-expression. The first context is when a
  // default constructor is called to initialize an element of an array.
  // If the constructor has one or more default arguments, the destruction of
  // every temporary created in a default argument expression is sequenced
  // before the construction of the next array element, if any.

  {
    RunCleanupsScope Scope(*this);

    // Evaluate the constructor and its arguments in a regular
    // partial-destroy cleanup.
    if (getLangOpts().Exceptions &&
        !ctor->getParent()->hasTrivialDestructor()) {
      Destroyer *destroyer = destroyCXXObject;
      pushRegularPartialArrayCleanup(arrayBegin, cur, type, eltAlignment,
                                     *destroyer);
    }

    EmitCXXConstructorCall(ctor, Ctor_Complete, /*ForVirtualBase=*/false,
                           /*Delegating=*/false, curAddr, E);
  }

  // Go to the next element.
  llvm::Value *next =
    Builder.CreateInBoundsGEP(cur, llvm::ConstantInt::get(SizeTy, 1),
                              "arrayctor.next");
  cur->addIncoming(next, Builder.GetInsertBlock());

  // Check whether that's the end of the loop.
  llvm::Value *done = Builder.CreateICmpEQ(next, arrayEnd, "arrayctor.done");
  llvm::BasicBlock *contBB = createBasicBlock("arrayctor.cont");
  Builder.CreateCondBr(done, contBB, loopBB);

  // Patch the earlier check to skip over the loop.
  if (zeroCheckBranch) zeroCheckBranch->setSuccessor(0, contBB);

  EmitBlock(contBB);
}

void CodeGenFunction::destroyCXXObject(CodeGenFunction &CGF,
                                       Address addr,
                                       QualType type) {
  const RecordType *rtype = type->castAs<RecordType>();
  const CXXRecordDecl *record = cast<CXXRecordDecl>(rtype->getDecl());
  const CXXDestructorDecl *dtor = record->getDestructor();
  assert(!dtor->isTrivial());
  CGF.EmitCXXDestructorCall(dtor, Dtor_Complete, /*for vbase*/ false,
                            /*Delegating=*/false, addr);
}

void CodeGenFunction::EmitCXXConstructorCall(const CXXConstructorDecl *D,
                                             CXXCtorType Type,
                                             bool ForVirtualBase,
                                             bool Delegating, Address This,
                                             const CXXConstructExpr *E) {
  CallArgList Args;

  // Push the this ptr.
  Args.add(RValue::get(This.getPointer()), D->getThisType(getContext()));

  // If this is a trivial constructor, emit a memcpy now before we lose
  // the alignment information on the argument.
  // FIXME: It would be better to preserve alignment information into CallArg.
  if (isMemcpyEquivalentSpecialMember(D)) {
    assert(E->getNumArgs() == 1 && "unexpected argcount for trivial ctor");

    const Expr *Arg = E->getArg(0);
    QualType SrcTy = Arg->getType();
    Address Src = EmitLValue(Arg).getAddress();
    QualType DestTy = getContext().getTypeDeclType(D->getParent());
    EmitAggregateCopyCtor(This, Src, DestTy, SrcTy);
    return;
  }

  // Add the rest of the user-supplied arguments.
  const FunctionProtoType *FPT = D->getType()->castAs<FunctionProtoType>();
  EmitCallArgs(Args, FPT, E->arguments(), E->getConstructor());

  EmitCXXConstructorCall(D, Type, ForVirtualBase, Delegating, This, Args);
}

static bool canEmitDelegateCallArgs(CodeGenFunction &CGF,
                                    const CXXConstructorDecl *Ctor,
                                    CXXCtorType Type, CallArgList &Args) {
  // We can't forward a variadic call.
  if (Ctor->isVariadic())
    return false;

  if (CGF.getTarget().getCXXABI().areArgsDestroyedLeftToRightInCallee()) {
    // If the parameters are callee-cleanup, it's not safe to forward.
    for (auto *P : Ctor->parameters())
      if (P->getType().isDestructedType())
        return false;

    // Likewise if they're inalloca.
    const CGFunctionInfo &Info =
        CGF.CGM.getTypes().arrangeCXXConstructorCall(Args, Ctor, Type, 0);
    if (Info.usesInAlloca())
      return false;
  }

  // Anything else should be OK.
  return true;
}

void CodeGenFunction::EmitCXXConstructorCall(const CXXConstructorDecl *D,
                                             CXXCtorType Type,
                                             bool ForVirtualBase,
                                             bool Delegating,
                                             Address This,
                                             CallArgList &Args) {
  const CXXRecordDecl *ClassDecl = D->getParent();

  // C++11 [class.mfct.non-static]p2:
  //   If a non-static member function of a class X is called for an object that
  //   is not of type X, or of a type derived from X, the behavior is undefined.
  // FIXME: Provide a source location here.
  EmitTypeCheck(CodeGenFunction::TCK_ConstructorCall, SourceLocation(),
                This.getPointer(), getContext().getRecordType(ClassDecl));

  if (D->isTrivial() && D->isDefaultConstructor()) {
    assert(Args.size() == 1 && "trivial default ctor with args");
    return;
  }

  // If this is a trivial constructor, just emit what's needed. If this is a
  // union copy constructor, we must emit a memcpy, because the AST does not
  // model that copy.
  if (isMemcpyEquivalentSpecialMember(D)) {
    assert(Args.size() == 2 && "unexpected argcount for trivial ctor");

    QualType SrcTy = D->getParamDecl(0)->getType().getNonReferenceType();
    Address Src(Args[1].RV.getScalarVal(), getNaturalTypeAlignment(SrcTy));
    QualType DestTy = getContext().getTypeDeclType(ClassDecl);
    EmitAggregateCopyCtor(This, Src, DestTy, SrcTy);
    return;
  }

  // Check whether we can actually emit the constructor before trying to do so.
  if (auto Inherited = D->getInheritedConstructor()) {
    if (getTypes().inheritingCtorHasParams(Inherited, Type) &&
        !canEmitDelegateCallArgs(*this, D, Type, Args)) {
      EmitInlinedInheritingCXXConstructorCall(D, Type, ForVirtualBase,
                                              Delegating, Args);
      return;
    }
  }

  // Insert any ABI-specific implicit constructor arguments.
  unsigned ExtraArgs = CGM.getCXXABI().addImplicitConstructorArgs(
      *this, D, Type, ForVirtualBase, Delegating, Args);

  // Emit the call.
  llvm::Value *Callee = CGM.getAddrOfCXXStructor(D, getFromCtorType(Type));
  const CGFunctionInfo &Info =
      CGM.getTypes().arrangeCXXConstructorCall(Args, D, Type, ExtraArgs);
  EmitCall(Info, Callee, ReturnValueSlot(), Args, D);

  // Generate vtable assumptions if we're constructing a complete object
  // with a vtable.  We don't do this for base subobjects for two reasons:
  // first, it's incorrect for classes with virtual bases, and second, we're
  // about to overwrite the vptrs anyway.
  // We also have to make sure if we can refer to vtable:
  // - Otherwise we can refer to vtable if it's safe to speculatively emit.
  // FIXME: If vtable is used by ctor/dtor, or if vtable is external and we are
  // sure that definition of vtable is not hidden,
  // then we are always safe to refer to it.
  // FIXME: It looks like InstCombine is very inefficient on dealing with
  // assumes. Make assumption loads require -fstrict-vtable-pointers temporarily.
  if (CGM.getCodeGenOpts().OptimizationLevel > 0 &&
      ClassDecl->isDynamicClass() && Type != Ctor_Base &&
      CGM.getCXXABI().canSpeculativelyEmitVTable(ClassDecl) &&
      CGM.getCodeGenOpts().StrictVTablePointers)
    EmitVTableAssumptionLoads(ClassDecl, This);
}

void CodeGenFunction::EmitInheritedCXXConstructorCall(
    const CXXConstructorDecl *D, bool ForVirtualBase, Address This,
    bool InheritedFromVBase, const CXXInheritedCtorInitExpr *E) {
  CallArgList Args;
  CallArg ThisArg(RValue::get(This.getPointer()), D->getThisType(getContext()),
                  /*NeedsCopy=*/false);

  // Forward the parameters.
  if (InheritedFromVBase &&
      CGM.getTarget().getCXXABI().hasConstructorVariants()) {
    // Nothing to do; this construction is not responsible for constructing
    // the base class containing the inherited constructor.
    // FIXME: Can we just pass undef's for the remaining arguments if we don't
    // have constructor variants?
    Args.push_back(ThisArg);
  } else if (!CXXInheritedCtorInitExprArgs.empty()) {
    // The inheriting constructor was inlined; just inject its arguments.
    assert(CXXInheritedCtorInitExprArgs.size() >= D->getNumParams() &&
           "wrong number of parameters for inherited constructor call");
    Args = CXXInheritedCtorInitExprArgs;
    Args[0] = ThisArg;
  } else {
    // The inheriting constructor was not inlined. Emit delegating arguments.
    Args.push_back(ThisArg);
    const auto *OuterCtor = cast<CXXConstructorDecl>(CurCodeDecl);
    assert(OuterCtor->getNumParams() == D->getNumParams());
    assert(!OuterCtor->isVariadic() && "should have been inlined");

    for (const auto *Param : OuterCtor->parameters()) {
      assert(getContext().hasSameUnqualifiedType(
          OuterCtor->getParamDecl(Param->getFunctionScopeIndex())->getType(),
          Param->getType()));
      EmitDelegateCallArg(Args, Param, E->getLocation());

      // Forward __attribute__(pass_object_size).
      if (Param->hasAttr<PassObjectSizeAttr>()) {
        auto *POSParam = SizeArguments[Param];
        assert(POSParam && "missing pass_object_size value for forwarding");
        EmitDelegateCallArg(Args, POSParam, E->getLocation());
      }
    }
  }

  EmitCXXConstructorCall(D, Ctor_Base, ForVirtualBase, /*Delegating*/false,
                         This, Args);
}

void CodeGenFunction::EmitInlinedInheritingCXXConstructorCall(
    const CXXConstructorDecl *Ctor, CXXCtorType CtorType, bool ForVirtualBase,
    bool Delegating, CallArgList &Args) {
  InlinedInheritingConstructorScope Scope(*this, GlobalDecl(Ctor, CtorType));

  // Save the arguments to be passed to the inherited constructor.
  CXXInheritedCtorInitExprArgs = Args;

  FunctionArgList Params;
  QualType RetType = BuildFunctionArgList(CurGD, Params);
  FnRetTy = RetType;

  // Insert any ABI-specific implicit constructor arguments.
  CGM.getCXXABI().addImplicitConstructorArgs(*this, Ctor, CtorType,
                                             ForVirtualBase, Delegating, Args);

  // Emit a simplified prolog. We only need to emit the implicit params.
  assert(Args.size() >= Params.size() && "too few arguments for call");
  for (unsigned I = 0, N = Args.size(); I != N; ++I) {
    if (I < Params.size() && isa<ImplicitParamDecl>(Params[I])) {
      const RValue &RV = Args[I].RV;
      assert(!RV.isComplex() && "complex indirect params not supported");
      ParamValue Val = RV.isScalar()
                           ? ParamValue::forDirect(RV.getScalarVal())
                           : ParamValue::forIndirect(RV.getAggregateAddress());
      EmitParmDecl(*Params[I], Val, I + 1);
    }
  }

  // Create a return value slot if the ABI implementation wants one.
  // FIXME: This is dumb, we should ask the ABI not to try to set the return
  // value instead.
  if (!RetType->isVoidType())
    ReturnValue = CreateIRTemp(RetType, "retval.inhctor");

  CGM.getCXXABI().EmitInstanceFunctionProlog(*this);
  CXXThisValue = CXXABIThisValue;

  // Directly emit the constructor initializers.
  EmitCtorPrologue(Ctor, CtorType, Params);
}

void CodeGenFunction::EmitVTableAssumptionLoad(const VPtr &Vptr, Address This) {
  llvm::Value *VTableGlobal =
      CGM.getCXXABI().getVTableAddressPoint(Vptr.Base, Vptr.VTableClass);
  if (!VTableGlobal)
    return;

  // We can just use the base offset in the complete class.
  CharUnits NonVirtualOffset = Vptr.Base.getBaseOffset();

  if (!NonVirtualOffset.isZero())
    This =
        ApplyNonVirtualAndVirtualOffset(*this, This, NonVirtualOffset, nullptr,
                                        Vptr.VTableClass, Vptr.NearestVBase);

  llvm::Value *VPtrValue =
      GetVTablePtr(This, VTableGlobal->getType(), Vptr.VTableClass);
  llvm::Value *Cmp =
      Builder.CreateICmpEQ(VPtrValue, VTableGlobal, "cmp.vtables");
  Builder.CreateAssumption(Cmp);
}

void CodeGenFunction::EmitVTableAssumptionLoads(const CXXRecordDecl *ClassDecl,
                                                Address This) {
  if (CGM.getCXXABI().doStructorsInitializeVPtrs(ClassDecl))
    for (const VPtr &Vptr : getVTablePointers(ClassDecl))
      EmitVTableAssumptionLoad(Vptr, This);
}

void
CodeGenFunction::EmitSynthesizedCXXCopyCtorCall(const CXXConstructorDecl *D,
                                                Address This, Address Src,
                                                const CXXConstructExpr *E) {
  const FunctionProtoType *FPT = D->getType()->castAs<FunctionProtoType>();

  CallArgList Args;

  // Push the this ptr.
  Args.add(RValue::get(This.getPointer()), D->getThisType(getContext()));

  // Push the src ptr.
  QualType QT = *(FPT->param_type_begin());
  llvm::Type *t = CGM.getTypes().ConvertType(QT);
  Src = Builder.CreateBitCast(Src, t);
  Args.add(RValue::get(Src.getPointer()), QT);

  // Skip over first argument (Src).
  EmitCallArgs(Args, FPT, drop_begin(E->arguments(), 1), E->getConstructor(),
               /*ParamsToSkip*/ 1);

  EmitCXXConstructorCall(D, Ctor_Complete, false, false, This, Args);
}

void
CodeGenFunction::EmitDelegateCXXConstructorCall(const CXXConstructorDecl *Ctor,
                                                CXXCtorType CtorType,
                                                const FunctionArgList &Args,
                                                SourceLocation Loc) {
  CallArgList DelegateArgs;

  FunctionArgList::const_iterator I = Args.begin(), E = Args.end();
  assert(I != E && "no parameters to constructor");

  // this
  Address This = LoadCXXThisAddress();
  DelegateArgs.add(RValue::get(This.getPointer()), (*I)->getType());
  ++I;

  // FIXME: The location of the VTT parameter in the parameter list is
  // specific to the Itanium ABI and shouldn't be hardcoded here.
  if (CGM.getCXXABI().NeedsVTTParameter(CurGD)) {
    assert(I != E && "cannot skip vtt parameter, already done with args");
    assert((*I)->getType()->isPointerType() &&
           "skipping parameter not of vtt type");
    ++I;
  }

  // Explicit arguments.
  for (; I != E; ++I) {
    const VarDecl *param = *I;
    // FIXME: per-argument source location
    EmitDelegateCallArg(DelegateArgs, param, Loc);
  }

  EmitCXXConstructorCall(Ctor, CtorType, /*ForVirtualBase=*/false,
                         /*Delegating=*/true, This, DelegateArgs);
}

namespace {
  struct CallDelegatingCtorDtor final : EHScopeStack::Cleanup {
    const CXXDestructorDecl *Dtor;
    Address Addr;
    CXXDtorType Type;

    CallDelegatingCtorDtor(const CXXDestructorDecl *D, Address Addr,
                           CXXDtorType Type)
      : Dtor(D), Addr(Addr), Type(Type) {}

    void Emit(CodeGenFunction &CGF, Flags flags) override {
      CGF.EmitCXXDestructorCall(Dtor, Type, /*ForVirtualBase=*/false,
                                /*Delegating=*/true, Addr);
    }
  };
} // end anonymous namespace

void
CodeGenFunction::EmitDelegatingCXXConstructorCall(const CXXConstructorDecl *Ctor,
                                                  const FunctionArgList &Args) {
  assert(Ctor->isDelegatingConstructor());

  Address ThisPtr = LoadCXXThisAddress();

  AggValueSlot AggSlot =
    AggValueSlot::forAddr(ThisPtr, Qualifiers(),
                          AggValueSlot::IsDestructed,
                          AggValueSlot::DoesNotNeedGCBarriers,
                          AggValueSlot::IsNotAliased);

  EmitAggExpr(Ctor->init_begin()[0]->getInit(), AggSlot);

  const CXXRecordDecl *ClassDecl = Ctor->getParent();
  if (CGM.getLangOpts().Exceptions && !ClassDecl->hasTrivialDestructor()) {
    CXXDtorType Type =
      CurGD.getCtorType() == Ctor_Complete ? Dtor_Complete : Dtor_Base;

    EHStack.pushCleanup<CallDelegatingCtorDtor>(EHCleanup,
                                                ClassDecl->getDestructor(),
                                                ThisPtr, Type);
  }
}

void CodeGenFunction::EmitCXXDestructorCall(const CXXDestructorDecl *DD,
                                            CXXDtorType Type,
                                            bool ForVirtualBase,
                                            bool Delegating,
                                            Address This) {
  CGM.getCXXABI().EmitDestructorCall(*this, DD, Type, ForVirtualBase,
                                     Delegating, This);
}

namespace {
  struct CallLocalDtor final : EHScopeStack::Cleanup {
    const CXXDestructorDecl *Dtor;
    Address Addr;

    CallLocalDtor(const CXXDestructorDecl *D, Address Addr)
      : Dtor(D), Addr(Addr) {}

    void Emit(CodeGenFunction &CGF, Flags flags) override {
      CGF.EmitCXXDestructorCall(Dtor, Dtor_Complete,
                                /*ForVirtualBase=*/false,
                                /*Delegating=*/false, Addr);
    }
  };
} // end anonymous namespace

void CodeGenFunction::PushDestructorCleanup(const CXXDestructorDecl *D,
                                            Address Addr) {
  EHStack.pushCleanup<CallLocalDtor>(NormalAndEHCleanup, D, Addr);
}

void CodeGenFunction::PushDestructorCleanup(QualType T, Address Addr) {
  CXXRecordDecl *ClassDecl = T->getAsCXXRecordDecl();
  if (!ClassDecl) return;
  if (ClassDecl->hasTrivialDestructor()) return;

  const CXXDestructorDecl *D = ClassDecl->getDestructor();
  assert(D && D->isUsed() && "destructor not marked as used!");
  PushDestructorCleanup(D, Addr);
}

void CodeGenFunction::InitializeVTablePointer(const VPtr &Vptr) {
  // Compute the address point.
  llvm::Value *VTableAddressPoint =
      CGM.getCXXABI().getVTableAddressPointInStructor(
          *this, Vptr.VTableClass, Vptr.Base, Vptr.NearestVBase);

  if (!VTableAddressPoint)
    return;

  // Compute where to store the address point.
  llvm::Value *VirtualOffset = nullptr;
  CharUnits NonVirtualOffset = CharUnits::Zero();

  if (CGM.getCXXABI().isVirtualOffsetNeededForVTableField(*this, Vptr)) {
    // We need to use the virtual base offset offset because the virtual base
    // might have a different offset in the most derived class.

    VirtualOffset = CGM.getCXXABI().GetVirtualBaseClassOffset(
        *this, LoadCXXThisAddress(), Vptr.VTableClass, Vptr.NearestVBase);
    NonVirtualOffset = Vptr.OffsetFromNearestVBase;
  } else {
    // We can just use the base offset in the complete class.
    NonVirtualOffset = Vptr.Base.getBaseOffset();
  }

  // Apply the offsets.
  Address VTableField = LoadCXXThisAddress();

  if (!NonVirtualOffset.isZero() || VirtualOffset)
    VTableField = ApplyNonVirtualAndVirtualOffset(
        *this, VTableField, NonVirtualOffset, VirtualOffset, Vptr.VTableClass,
        Vptr.NearestVBase);

  // Finally, store the address point. Use the same LLVM types as the field to
  // support optimization.
  llvm::Type *VTablePtrTy =
      llvm::FunctionType::get(CGM.Int32Ty, /*isVarArg=*/true)
          ->getPointerTo()
          ->getPointerTo();
  VTableField = Builder.CreateBitCast(VTableField, VTablePtrTy->getPointerTo());
  VTableAddressPoint = Builder.CreateBitCast(VTableAddressPoint, VTablePtrTy);

  llvm::StoreInst *Store = Builder.CreateStore(VTableAddressPoint, VTableField);
  CGM.DecorateInstructionWithTBAA(Store, CGM.getTBAAInfoForVTablePtr());
  if (CGM.getCodeGenOpts().OptimizationLevel > 0 &&
      CGM.getCodeGenOpts().StrictVTablePointers)
    CGM.DecorateInstructionWithInvariantGroup(Store, Vptr.VTableClass);
}

CodeGenFunction::VPtrsVector
CodeGenFunction::getVTablePointers(const CXXRecordDecl *VTableClass) {
  CodeGenFunction::VPtrsVector VPtrsResult;
  VisitedVirtualBasesSetTy VBases;
  getVTablePointers(BaseSubobject(VTableClass, CharUnits::Zero()),
                    /*NearestVBase=*/nullptr,
                    /*OffsetFromNearestVBase=*/CharUnits::Zero(),
                    /*BaseIsNonVirtualPrimaryBase=*/false, VTableClass, VBases,
                    VPtrsResult);
  return VPtrsResult;
}

void CodeGenFunction::getVTablePointers(BaseSubobject Base,
                                        const CXXRecordDecl *NearestVBase,
                                        CharUnits OffsetFromNearestVBase,
                                        bool BaseIsNonVirtualPrimaryBase,
                                        const CXXRecordDecl *VTableClass,
                                        VisitedVirtualBasesSetTy &VBases,
                                        VPtrsVector &Vptrs) {
  // If this base is a non-virtual primary base the address point has already
  // been set.
  if (!BaseIsNonVirtualPrimaryBase) {
    // Initialize the vtable pointer for this base.
    VPtr Vptr = {Base, NearestVBase, OffsetFromNearestVBase, VTableClass};
    Vptrs.push_back(Vptr);
  }

  const CXXRecordDecl *RD = Base.getBase();

  // Traverse bases.
  for (const auto &I : RD->bases()) {
    CXXRecordDecl *BaseDecl
      = cast<CXXRecordDecl>(I.getType()->getAs<RecordType>()->getDecl());

    // Ignore classes without a vtable.
    if (!BaseDecl->isDynamicClass())
      continue;

    CharUnits BaseOffset;
    CharUnits BaseOffsetFromNearestVBase;
    bool BaseDeclIsNonVirtualPrimaryBase;

    if (I.isVirtual()) {
      // Check if we've visited this virtual base before.
      if (!VBases.insert(BaseDecl).second)
        continue;

      const ASTRecordLayout &Layout =
        getContext().getASTRecordLayout(VTableClass);

      BaseOffset = Layout.getVBaseClassOffset(BaseDecl);
      BaseOffsetFromNearestVBase = CharUnits::Zero();
      BaseDeclIsNonVirtualPrimaryBase = false;
    } else {
      const ASTRecordLayout &Layout = getContext().getASTRecordLayout(RD);

      BaseOffset = Base.getBaseOffset() + Layout.getBaseClassOffset(BaseDecl);
      BaseOffsetFromNearestVBase =
        OffsetFromNearestVBase + Layout.getBaseClassOffset(BaseDecl);
      BaseDeclIsNonVirtualPrimaryBase = Layout.getPrimaryBase() == BaseDecl;
    }

    getVTablePointers(
        BaseSubobject(BaseDecl, BaseOffset),
        I.isVirtual() ? BaseDecl : NearestVBase, BaseOffsetFromNearestVBase,
        BaseDeclIsNonVirtualPrimaryBase, VTableClass, VBases, Vptrs);
  }
}

void CodeGenFunction::InitializeVTablePointers(const CXXRecordDecl *RD) {
  // Ignore classes without a vtable.
  if (!RD->isDynamicClass())
    return;

  // Initialize the vtable pointers for this class and all of its bases.
  if (CGM.getCXXABI().doStructorsInitializeVPtrs(RD))
    for (const VPtr &Vptr : getVTablePointers(RD))
      InitializeVTablePointer(Vptr);

  if (RD->getNumVBases())
    CGM.getCXXABI().initializeHiddenVirtualInheritanceMembers(*this, RD);
}

llvm::Value *CodeGenFunction::GetVTablePtr(Address This,
                                           llvm::Type *VTableTy,
                                           const CXXRecordDecl *RD) {
  Address VTablePtrSrc = Builder.CreateElementBitCast(This, VTableTy);
  llvm::Instruction *VTable = Builder.CreateLoad(VTablePtrSrc, "vtable");
  CGM.DecorateInstructionWithTBAA(VTable, CGM.getTBAAInfoForVTablePtr());

  if (CGM.getCodeGenOpts().OptimizationLevel > 0 &&
      CGM.getCodeGenOpts().StrictVTablePointers)
    CGM.DecorateInstructionWithInvariantGroup(VTable, RD);

  return VTable;
}

// If a class has a single non-virtual base and does not introduce or override
// virtual member functions or fields, it will have the same layout as its base.
// This function returns the least derived such class.
//
// Casting an instance of a base class to such a derived class is technically
// undefined behavior, but it is a relatively common hack for introducing member
// functions on class instances with specific properties (e.g. llvm::Operator)
// that works under most compilers and should not have security implications, so
// we allow it by default. It can be disabled with -fsanitize=cfi-cast-strict.
static const CXXRecordDecl *
LeastDerivedClassWithSameLayout(const CXXRecordDecl *RD) {
  if (!RD->field_empty())
    return RD;

  if (RD->getNumVBases() != 0)
    return RD;

  if (RD->getNumBases() != 1)
    return RD;

  for (const CXXMethodDecl *MD : RD->methods()) {
    if (MD->isVirtual()) {
      // Virtual member functions are only ok if they are implicit destructors
      // because the implicit destructor will have the same semantics as the
      // base class's destructor if no fields are added.
      if (isa<CXXDestructorDecl>(MD) && MD->isImplicit())
        continue;
      return RD;
    }
  }

  return LeastDerivedClassWithSameLayout(
      RD->bases_begin()->getType()->getAsCXXRecordDecl());
}

void CodeGenFunction::EmitTypeMetadataCodeForVCall(const CXXRecordDecl *RD,
                                                   llvm::Value *VTable,
                                                   SourceLocation Loc) {
  if (CGM.getCodeGenOpts().WholeProgramVTables &&
      CGM.HasHiddenLTOVisibility(RD)) {
    llvm::Metadata *MD =
        CGM.CreateMetadataIdentifierForType(QualType(RD->getTypeForDecl(), 0));
    llvm::Value *TypeId =
        llvm::MetadataAsValue::get(CGM.getLLVMContext(), MD);

    llvm::Value *CastedVTable = Builder.CreateBitCast(VTable, Int8PtrTy);
    llvm::Value *TypeTest =
        Builder.CreateCall(CGM.getIntrinsic(llvm::Intrinsic::type_test),
                           {CastedVTable, TypeId});
    Builder.CreateCall(CGM.getIntrinsic(llvm::Intrinsic::assume), TypeTest);
  }

  if (SanOpts.has(SanitizerKind::CFIVCall))
    EmitVTablePtrCheckForCall(RD, VTable, CodeGenFunction::CFITCK_VCall, Loc);
}

void CodeGenFunction::EmitVTablePtrCheckForCall(const CXXRecordDecl *RD,
                                                llvm::Value *VTable,
                                                CFITypeCheckKind TCK,
                                                SourceLocation Loc) {
  if (!SanOpts.has(SanitizerKind::CFICastStrict))
    RD = LeastDerivedClassWithSameLayout(RD);

  EmitVTablePtrCheck(RD, VTable, TCK, Loc);
}

void CodeGenFunction::EmitVTablePtrCheckForCast(QualType T,
                                                llvm::Value *Derived,
                                                bool MayBeNull,
                                                CFITypeCheckKind TCK,
                                                SourceLocation Loc) {
  if (!getLangOpts().CPlusPlus)
    return;

  auto *ClassTy = T->getAs<RecordType>();
  if (!ClassTy)
    return;

  const CXXRecordDecl *ClassDecl = cast<CXXRecordDecl>(ClassTy->getDecl());

  if (!ClassDecl->isCompleteDefinition() || !ClassDecl->isDynamicClass())
    return;

  if (!SanOpts.has(SanitizerKind::CFICastStrict))
    ClassDecl = LeastDerivedClassWithSameLayout(ClassDecl);

  llvm::BasicBlock *ContBlock = nullptr;

  if (MayBeNull) {
    llvm::Value *DerivedNotNull =
        Builder.CreateIsNotNull(Derived, "cast.nonnull");

    llvm::BasicBlock *CheckBlock = createBasicBlock("cast.check");
    ContBlock = createBasicBlock("cast.cont");

    Builder.CreateCondBr(DerivedNotNull, CheckBlock, ContBlock);

    EmitBlock(CheckBlock);
  }

  llvm::Value *VTable =
    GetVTablePtr(Address(Derived, getPointerAlign()), Int8PtrTy, ClassDecl);

  EmitVTablePtrCheck(ClassDecl, VTable, TCK, Loc);

  if (MayBeNull) {
    Builder.CreateBr(ContBlock);
    EmitBlock(ContBlock);
  }
}

void CodeGenFunction::EmitVTablePtrCheck(const CXXRecordDecl *RD,
                                         llvm::Value *VTable,
                                         CFITypeCheckKind TCK,
                                         SourceLocation Loc) {
  if (!CGM.getCodeGenOpts().SanitizeCfiCrossDso &&
      !CGM.HasHiddenLTOVisibility(RD))
    return;

  std::string TypeName = RD->getQualifiedNameAsString();
  if (getContext().getSanitizerBlacklist().isBlacklistedType(TypeName))
    return;

  SanitizerScope SanScope(this);
  llvm::SanitizerStatKind SSK;
  switch (TCK) {
  case CFITCK_VCall:
    SSK = llvm::SanStat_CFI_VCall;
    break;
  case CFITCK_NVCall:
    SSK = llvm::SanStat_CFI_NVCall;
    break;
  case CFITCK_DerivedCast:
    SSK = llvm::SanStat_CFI_DerivedCast;
    break;
  case CFITCK_UnrelatedCast:
    SSK = llvm::SanStat_CFI_UnrelatedCast;
    break;
  case CFITCK_ICall:
    llvm_unreachable("not expecting CFITCK_ICall");
  }
  EmitSanitizerStatReport(SSK);

  llvm::Metadata *MD =
      CGM.CreateMetadataIdentifierForType(QualType(RD->getTypeForDecl(), 0));
  llvm::Value *TypeId = llvm::MetadataAsValue::get(getLLVMContext(), MD);

  llvm::Value *CastedVTable = Builder.CreateBitCast(VTable, Int8PtrTy);
  llvm::Value *TypeTest = Builder.CreateCall(
      CGM.getIntrinsic(llvm::Intrinsic::type_test), {CastedVTable, TypeId});

  SanitizerMask M;
  switch (TCK) {
  case CFITCK_VCall:
    M = SanitizerKind::CFIVCall;
    break;
  case CFITCK_NVCall:
    M = SanitizerKind::CFINVCall;
    break;
  case CFITCK_DerivedCast:
    M = SanitizerKind::CFIDerivedCast;
    break;
  case CFITCK_UnrelatedCast:
    M = SanitizerKind::CFIUnrelatedCast;
    break;
  case CFITCK_ICall:
    llvm_unreachable("not expecting CFITCK_ICall");
  }

  llvm::Constant *StaticData[] = {
      llvm::ConstantInt::get(Int8Ty, TCK),
      EmitCheckSourceLocation(Loc),
      EmitCheckTypeDescriptor(QualType(RD->getTypeForDecl(), 0)),
  };

  auto CrossDsoTypeId = CGM.CreateCrossDsoCfiTypeId(MD);
  if (CGM.getCodeGenOpts().SanitizeCfiCrossDso && CrossDsoTypeId) {
    EmitCfiSlowPathCheck(M, TypeTest, CrossDsoTypeId, CastedVTable, StaticData);
    return;
  }

  if (CGM.getCodeGenOpts().SanitizeTrap.has(M)) {
    EmitTrapCheck(TypeTest);
    return;
  }

  llvm::Value *AllVtables = llvm::MetadataAsValue::get(
      CGM.getLLVMContext(),
      llvm::MDString::get(CGM.getLLVMContext(), "all-vtables"));
  llvm::Value *ValidVtable = Builder.CreateCall(
      CGM.getIntrinsic(llvm::Intrinsic::type_test), {CastedVTable, AllVtables});
  EmitCheck(std::make_pair(TypeTest, M), "cfi_check_fail", StaticData,
            {CastedVTable, ValidVtable});
}

bool CodeGenFunction::ShouldEmitVTableTypeCheckedLoad(const CXXRecordDecl *RD) {
  if (!CGM.getCodeGenOpts().WholeProgramVTables ||
      !SanOpts.has(SanitizerKind::CFIVCall) ||
      !CGM.getCodeGenOpts().SanitizeTrap.has(SanitizerKind::CFIVCall) ||
      !CGM.HasHiddenLTOVisibility(RD))
    return false;

  std::string TypeName = RD->getQualifiedNameAsString();
  return !getContext().getSanitizerBlacklist().isBlacklistedType(TypeName);
}

llvm::Value *CodeGenFunction::EmitVTableTypeCheckedLoad(
    const CXXRecordDecl *RD, llvm::Value *VTable, uint64_t VTableByteOffset) {
  SanitizerScope SanScope(this);

  EmitSanitizerStatReport(llvm::SanStat_CFI_VCall);

  llvm::Metadata *MD =
      CGM.CreateMetadataIdentifierForType(QualType(RD->getTypeForDecl(), 0));
  llvm::Value *TypeId = llvm::MetadataAsValue::get(CGM.getLLVMContext(), MD);

  llvm::Value *CastedVTable = Builder.CreateBitCast(VTable, Int8PtrTy);
  llvm::Value *CheckedLoad = Builder.CreateCall(
      CGM.getIntrinsic(llvm::Intrinsic::type_checked_load),
      {CastedVTable, llvm::ConstantInt::get(Int32Ty, VTableByteOffset),
       TypeId});
  llvm::Value *CheckResult = Builder.CreateExtractValue(CheckedLoad, 1);

  EmitCheck(std::make_pair(CheckResult, SanitizerKind::CFIVCall),
            "cfi_check_fail", nullptr, nullptr);

  return Builder.CreateBitCast(
      Builder.CreateExtractValue(CheckedLoad, 0),
      cast<llvm::PointerType>(VTable->getType())->getElementType());
}

// FIXME: Ideally Expr::IgnoreParenNoopCasts should do this, but it doesn't do
// quite what we want.
static const Expr *skipNoOpCastsAndParens(const Expr *E) {
  while (true) {
    if (const ParenExpr *PE = dyn_cast<ParenExpr>(E)) {
      E = PE->getSubExpr();
      continue;
    }

    if (const CastExpr *CE = dyn_cast<CastExpr>(E)) {
      if (CE->getCastKind() == CK_NoOp) {
        E = CE->getSubExpr();
        continue;
      }
    }
    if (const UnaryOperator *UO = dyn_cast<UnaryOperator>(E)) {
      if (UO->getOpcode() == UO_Extension) {
        E = UO->getSubExpr();
        continue;
      }
    }
    return E;
  }
}

bool
CodeGenFunction::CanDevirtualizeMemberFunctionCall(const Expr *Base,
                                                   const CXXMethodDecl *MD) {
  // When building with -fapple-kext, all calls must go through the vtable since
  // the kernel linker can do runtime patching of vtables.
  if (getLangOpts().AppleKext)
    return false;

  // If the member function is marked 'final', we know that it can't be
  // overridden and can therefore devirtualize it.
  if (MD->hasAttr<FinalAttr>())
    return true;

  // If the most derived class is marked final, we know that no subclass can
  // override this member function and so we can devirtualize it. For example:
  //
  // struct A { virtual void f(); }
  // struct B final : A { };
  //
  // void f(B *b) {
  //   b->f();
  // }
  //
  if (const CXXRecordDecl *BestDynamicDecl = Base->getBestDynamicClassType()) {
    if (BestDynamicDecl->hasAttr<FinalAttr>())
      return true;

    // There may be a method corresponding to MD in a derived class. If that
    // method is marked final, we can devirtualize it.
    const CXXMethodDecl *DevirtualizedMethod =
        MD->getCorrespondingMethodInClass(BestDynamicDecl);
    if (DevirtualizedMethod->hasAttr<FinalAttr>())
      return true;
  }

  // Similarly, if the class itself is marked 'final' it can't be overridden
  // and we can therefore devirtualize the member function call.
  if (MD->getParent()->hasAttr<FinalAttr>())
    return true;

  Base = skipNoOpCastsAndParens(Base);
  if (const DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Base)) {
    if (const VarDecl *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
      // This is a record decl. We know the type and can devirtualize it.
      return VD->getType()->isRecordType();
    }

    return false;
  }

  // We can devirtualize calls on an object accessed by a class member access
  // expression, since by C++11 [basic.life]p6 we know that it can't refer to
  // a derived class object constructed in the same location.
  if (const MemberExpr *ME = dyn_cast<MemberExpr>(Base))
    if (const ValueDecl *VD = dyn_cast<ValueDecl>(ME->getMemberDecl()))
      return VD->getType()->isRecordType();

  // We can always devirtualize calls on temporary object expressions.
  if (isa<CXXConstructExpr>(Base))
    return true;

  // And calls on bound temporaries.
  if (isa<CXXBindTemporaryExpr>(Base))
    return true;

  // Check if this is a call expr that returns a record type.
  if (const CallExpr *CE = dyn_cast<CallExpr>(Base))
    return CE->getCallReturnType(getContext())->isRecordType();

  // We can't devirtualize the call.
  return false;
}

void CodeGenFunction::EmitForwardingCallToLambda(
                                      const CXXMethodDecl *callOperator,
                                      CallArgList &callArgs) {
  // Get the address of the call operator.
  const CGFunctionInfo &calleeFnInfo =
    CGM.getTypes().arrangeCXXMethodDeclaration(callOperator);
  llvm::Value *callee =
    CGM.GetAddrOfFunction(GlobalDecl(callOperator),
                          CGM.getTypes().GetFunctionType(calleeFnInfo));

  // Prepare the return slot.
  const FunctionProtoType *FPT =
    callOperator->getType()->castAs<FunctionProtoType>();
  QualType resultType = FPT->getReturnType();
  ReturnValueSlot returnSlot;
  if (!resultType->isVoidType() &&
      calleeFnInfo.getReturnInfo().getKind() == ABIArgInfo::Indirect &&
      !hasScalarEvaluationKind(calleeFnInfo.getReturnType()))
    returnSlot = ReturnValueSlot(ReturnValue, resultType.isVolatileQualified());

  // We don't need to separately arrange the call arguments because
  // the call can't be variadic anyway --- it's impossible to forward
  // variadic arguments.

  // Now emit our call.
  RValue RV = EmitCall(calleeFnInfo, callee, returnSlot,
                       callArgs, callOperator);

  // If necessary, copy the returned value into the slot.
  if (!resultType->isVoidType() && returnSlot.isNull())
    EmitReturnOfRValue(RV, resultType);
  else
    EmitBranchThroughCleanup(ReturnBlock);
}

void CodeGenFunction::EmitLambdaBlockInvokeBody() {
  const BlockDecl *BD = BlockInfo->getBlockDecl();
  const VarDecl *variable = BD->capture_begin()->getVariable();
  const CXXRecordDecl *Lambda = variable->getType()->getAsCXXRecordDecl();

  // Start building arguments for forwarding call
  CallArgList CallArgs;

  QualType ThisType = getContext().getPointerType(getContext().getRecordType(Lambda));
  Address ThisPtr = GetAddrOfBlockDecl(variable, false);
  CallArgs.add(RValue::get(ThisPtr.getPointer()), ThisType);

  // Add the rest of the parameters.
  for (auto param : BD->parameters())
    EmitDelegateCallArg(CallArgs, param, param->getLocStart());

  assert(!Lambda->isGenericLambda() &&
            "generic lambda interconversion to block not implemented");
  EmitForwardingCallToLambda(Lambda->getLambdaCallOperator(), CallArgs);
}

void CodeGenFunction::EmitLambdaToBlockPointerBody(FunctionArgList &Args) {
  if (cast<CXXMethodDecl>(CurCodeDecl)->isVariadic()) {
    // FIXME: Making this work correctly is nasty because it requires either
    // cloning the body of the call operator or making the call operator forward.
    CGM.ErrorUnsupported(CurCodeDecl, "lambda conversion to variadic function");
    return;
  }

  EmitFunctionBody(Args, cast<FunctionDecl>(CurGD.getDecl())->getBody());
}

void CodeGenFunction::EmitLambdaDelegatingInvokeBody(const CXXMethodDecl *MD) {
  const CXXRecordDecl *Lambda = MD->getParent();

  // Start building arguments for forwarding call
  CallArgList CallArgs;

  QualType ThisType = getContext().getPointerType(getContext().getRecordType(Lambda));
  llvm::Value *ThisPtr = llvm::UndefValue::get(getTypes().ConvertType(ThisType));
  CallArgs.add(RValue::get(ThisPtr), ThisType);

  // Add the rest of the parameters.
  for (auto Param : MD->parameters())
    EmitDelegateCallArg(CallArgs, Param, Param->getLocStart());

  const CXXMethodDecl *CallOp = Lambda->getLambdaCallOperator();
  // For a generic lambda, find the corresponding call operator specialization
  // to which the call to the static-invoker shall be forwarded.
  if (Lambda->isGenericLambda()) {
    assert(MD->isFunctionTemplateSpecialization());
    const TemplateArgumentList *TAL = MD->getTemplateSpecializationArgs();
    FunctionTemplateDecl *CallOpTemplate = CallOp->getDescribedFunctionTemplate();
    void *InsertPos = nullptr;
    FunctionDecl *CorrespondingCallOpSpecialization =
        CallOpTemplate->findSpecialization(TAL->asArray(), InsertPos);
    assert(CorrespondingCallOpSpecialization);
    CallOp = cast<CXXMethodDecl>(CorrespondingCallOpSpecialization);
  }
  EmitForwardingCallToLambda(CallOp, CallArgs);
}

void CodeGenFunction::EmitLambdaStaticInvokeFunction(const CXXMethodDecl *MD) {
  if (MD->isVariadic()) {
    // FIXME: Making this work correctly is nasty because it requires either
    // cloning the body of the call operator or making the call operator forward.
    CGM.ErrorUnsupported(MD, "lambda conversion to variadic function");
    return;
  }

  EmitLambdaDelegatingInvokeBody(MD);
}
