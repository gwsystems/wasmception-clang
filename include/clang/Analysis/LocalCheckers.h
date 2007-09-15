//==- LocalCheckers.h - Intra-Procedural+Flow-Sensitive Checkers -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file was developed by Ted Kremenek and is distributed under
// the University of Illinois Open Source License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the interface to call a set of intra-procedural (local)
//  checkers that use flow/path-sensitive analyses to find bugs.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_LOCALCHECKERS_H
#define LLVM_CLANG_ANALYSIS_LOCALCHECKERS_H

namespace clang {

class CFG;
class LiveVariables;
class Diagnostic;
  
void CheckDeadStores(CFG& cfg, LiveVariables& L,
                     ASTContext &Ctx, Diagnostic &Diags);
void CheckDeadStores(CFG& cfg, ASTContext &Ctx, Diagnostic &Diags);  

} // end namespace clang

#endif
