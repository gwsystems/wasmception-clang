//= CFGRecStmtDeclVisitor - Recursive visitor of CFG stmts/decls -*- C++ --*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by Ted Kremenek and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the template class CFGRecStmtDeclVisitor, which extends
// CFGRecStmtVisitor by implementing (typed) visitation of decls.
//
// FIXME: This may not be fully complete.  We currently explore only subtypes
//        of ScopedDecl.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_CFG_REC_STMT_DECL_VISITOR_H
#define LLVM_CLANG_ANALYSIS_CFG_REC_STMT_DECL_VISITOR_H

#include "clang/Analysis/CFGRecStmtVisitor.h"
#include "clang/AST/Decl.h"

#define DISPATCH_CASE(CASE,CLASS) \
case Decl::CASE: \
static_cast<ImplClass*>(this)->Visit##CLASS(static_cast<CLASS*>(D));\
break;

#define DEFAULT_DISPATCH(CLASS) void Visit##CLASS(CLASS* D) {}

namespace clang {
template <typename ImplClass>
class CFGRecStmtDeclVisitor : public CFGRecStmtVisitor<ImplClass> {
public:  

  void VisitDeclRefExpr(DeclRefExpr* DR) {
    static_cast<ImplClass*>(this)->VisitDeclChain(DR->getDecl());
  }
  
  void VisitDeclStmt(DeclStmt* DS) { 
    static_cast<ImplClass*>(this)->VisitDeclChain(DS->getDecl()); 
  }
  
  void VisitDeclChain(ScopedDecl* D) {
    for (; D != NULL; D = D->getNextDeclarator())
      static_cast<ImplClass*>(this)->VisitScopedDecl(D);
  }
  
  void VisitScopedDecl(ScopedDecl* D) {
    switch (D->getKind()) {
        DISPATCH_CASE(Function,FunctionDecl)
        DISPATCH_CASE(BlockVariable,BlockVarDecl) // FIXME:Refine. VisitVarDecl?
        DISPATCH_CASE(FileVariable,FileVarDecl)   // FIXME: (same)
        DISPATCH_CASE(ParmVariable,ParmVarDecl)       // FIXME: (same)
        DISPATCH_CASE(EnumConstant,EnumConstantDecl)
        DISPATCH_CASE(Typedef,TypedefDecl)
        DISPATCH_CASE(Struct,RecordDecl)    // FIXME: Refine.  VisitStructDecl?
        DISPATCH_CASE(Union,RecordDecl)     // FIXME: Refine.
        DISPATCH_CASE(Class,RecordDecl)     // FIXME: Refine. 
        DISPATCH_CASE(Enum,EnumDecl)
        DISPATCH_CASE(ObjcInterface,ObjcInterfaceDecl)
        DISPATCH_CASE(ObjcClass,ObjcClassDecl)
        DISPATCH_CASE(ObjcProtocol,ObjcProtocolDecl)
        DISPATCH_CASE(ObjcCategory,ObjcCategoryDecl)
      default:
        assert(false && "Subtype of ScopedDecl not handled.");
    }
  }
  
  DEFAULT_DISPATCH(FunctionDecl)
  DEFAULT_DISPATCH(BlockVarDecl)
  DEFAULT_DISPATCH(FileVarDecl)
  DEFAULT_DISPATCH(ParmVarDecl)
  DEFAULT_DISPATCH(EnumConstantDecl)
  DEFAULT_DISPATCH(TypedefDecl)
  DEFAULT_DISPATCH(RecordDecl)
  DEFAULT_DISPATCH(EnumDecl)
  DEFAULT_DISPATCH(ObjcInterfaceDecl)
  DEFAULT_DISPATCH(ObjcClassDecl)
  DEFAULT_DISPATCH(ObjcMethodDecl)
  DEFAULT_DISPATCH(ObjcProtocolDecl)
  DEFAULT_DISPATCH(ObjcCategoryDecl)
};

} // end namespace clang

#undef DISPATCH_CASE
#undef DEFAULT_DISPATCH
#endif
