//===--- CheckerManager.cpp - Static Analyzer Checker Manager -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines the Static Analyzer Checker Manager.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/CheckerProvider.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/Analysis/ProgramPoint.h"
#include "clang/AST/DeclBase.h"

using namespace clang;
using namespace ento;

//===----------------------------------------------------------------------===//
// Functions for running checkers for AST traversing..
//===----------------------------------------------------------------------===//

void CheckerManager::runCheckersOnASTDecl(const Decl *D, AnalysisManager& mgr,
                                          BugReporter &BR) {
  assert(D);

  unsigned DeclKind = D->getKind();
  CachedDeclCheckers *checkers = 0;
  CachedDeclCheckersMapTy::iterator CCI = CachedDeclCheckersMap.find(DeclKind);
  if (CCI != CachedDeclCheckersMap.end()) {
    checkers = &(CCI->second);
  } else {
    // Find the checkers that should run for this Decl and cache them.
    checkers = &CachedDeclCheckersMap[DeclKind];
    for (unsigned i = 0, e = DeclCheckers.size(); i != e; ++i) {
      DeclCheckerInfo &info = DeclCheckers[i];
      if (info.IsForDeclFn(D))
        checkers->push_back(info.CheckFn);
    }
  }

  assert(checkers);
  for (CachedDeclCheckers::iterator
         I = checkers->begin(), E = checkers->end(); I != E; ++I)
    (*I)(D, mgr, BR);
}

void CheckerManager::runCheckersOnASTBody(const Decl *D, AnalysisManager& mgr,
                                          BugReporter &BR) {
  assert(D && D->hasBody());

  for (unsigned i = 0, e = BodyCheckers.size(); i != e; ++i)
    BodyCheckers[i](D, mgr, BR);
}

//===----------------------------------------------------------------------===//
// Functions for running checkers for path-sensitive checking.
//===----------------------------------------------------------------------===//

template <typename CHECK_CTX>
static void expandGraphWithCheckers(CHECK_CTX checkCtx,
                                    ExplodedNodeSet &Dst,
                                    const ExplodedNodeSet &Src) {

  typename CHECK_CTX::CheckersTy::const_iterator
      I = checkCtx.checkers_begin(), E = checkCtx.checkers_end();
  if (I == E) {
    Dst.insert(Src);
    return;
  }

  ExplodedNodeSet Tmp1, Tmp2;
  const ExplodedNodeSet *PrevSet = &Src;

  for (; I != E; ++I) {
    ExplodedNodeSet *CurrSet = 0;
    if (I+1 == E)
      CurrSet = &Dst;
    else {
      CurrSet = (PrevSet == &Tmp1) ? &Tmp2 : &Tmp1;
      CurrSet->clear();
    }

    for (ExplodedNodeSet::iterator NI = PrevSet->begin(), NE = PrevSet->end();
         NI != NE; ++NI)
      checkCtx.runChecker(*I, *CurrSet, *NI);

    // Update which NodeSet is the current one.
    PrevSet = CurrSet;
  }
}

namespace {
  struct CheckStmtContext {
    typedef llvm::SmallVectorImpl<CheckerManager::CheckStmtFunc> CheckersTy;
    bool IsPreVisit;
    const CheckersTy &Checkers;
    const Stmt *S;
    ExprEngine &Eng;

    CheckersTy::const_iterator checkers_begin() { return Checkers.begin(); }
    CheckersTy::const_iterator checkers_end() { return Checkers.end(); }

    CheckStmtContext(bool isPreVisit, const CheckersTy &checkers,
                     const Stmt *s, ExprEngine &eng)
      : IsPreVisit(isPreVisit), Checkers(checkers), S(s), Eng(eng) { }

    void runChecker(CheckerManager::CheckStmtFunc checkFn,
                    ExplodedNodeSet &Dst, ExplodedNode *Pred) {
      // FIXME: Remove respondsToCallback from CheckerContext;
      CheckerContext C(Dst, Eng.getBuilder(), Eng, Pred, checkFn.Checker,
                       IsPreVisit ? ProgramPoint::PreStmtKind :
                                    ProgramPoint::PostStmtKind, 0, S);
      checkFn(S, C);
    }
  };
}

/// \brief Run checkers for visiting Stmts.
void CheckerManager::runCheckersForStmt(bool isPreVisit,
                                        ExplodedNodeSet &Dst,
                                        const ExplodedNodeSet &Src,
                                        const Stmt *S,
                                        ExprEngine &Eng) {
  CheckStmtContext C(isPreVisit, *getCachedStmtCheckersFor(S, isPreVisit),
                     S, Eng);
  expandGraphWithCheckers(C, Dst, Src);
}

namespace {
  struct CheckObjCMessageContext {
    typedef std::vector<CheckerManager::CheckObjCMessageFunc> CheckersTy;
    bool IsPreVisit;
    const CheckersTy &Checkers;
    const ObjCMessage &Msg;
    ExprEngine &Eng;

    CheckersTy::const_iterator checkers_begin() { return Checkers.begin(); }
    CheckersTy::const_iterator checkers_end() { return Checkers.end(); }

    CheckObjCMessageContext(bool isPreVisit, const CheckersTy &checkers,
                            const ObjCMessage &msg, ExprEngine &eng)
      : IsPreVisit(isPreVisit), Checkers(checkers), Msg(msg), Eng(eng) { }

    void runChecker(CheckerManager::CheckObjCMessageFunc checkFn,
                    ExplodedNodeSet &Dst, ExplodedNode *Pred) {
      CheckerContext C(Dst, Eng.getBuilder(), Eng, Pred, checkFn.Checker,
                       IsPreVisit ? ProgramPoint::PreStmtKind :
                                    ProgramPoint::PostStmtKind, 0,
                       Msg.getOriginExpr());
      checkFn(Msg, C);
    }
  };
}

/// \brief Run checkers for visiting obj-c messages.
void CheckerManager::runCheckersForObjCMessage(bool isPreVisit,
                                               ExplodedNodeSet &Dst,
                                               const ExplodedNodeSet &Src,
                                               const ObjCMessage &msg,
                                               ExprEngine &Eng) {
  CheckObjCMessageContext C(isPreVisit,
                            isPreVisit ? PreObjCMessageCheckers
                                       : PostObjCMessageCheckers,
                            msg, Eng);
  expandGraphWithCheckers(C, Dst, Src);
}

namespace {
  struct CheckLocationContext {
    typedef std::vector<CheckerManager::CheckLocationFunc> CheckersTy;
    const CheckersTy &Checkers;
    SVal Loc;
    bool IsLoad;
    const Stmt *S;
    const GRState *State;
    ExprEngine &Eng;

    CheckersTy::const_iterator checkers_begin() { return Checkers.begin(); }
    CheckersTy::const_iterator checkers_end() { return Checkers.end(); }

    CheckLocationContext(const CheckersTy &checkers,
                         SVal loc, bool isLoad, const Stmt *s,
                         const GRState *state, ExprEngine &eng)
      : Checkers(checkers), Loc(loc), IsLoad(isLoad), S(s),
        State(state), Eng(eng) { }

    void runChecker(CheckerManager::CheckLocationFunc checkFn,
                    ExplodedNodeSet &Dst, ExplodedNode *Pred) {
      CheckerContext C(Dst, Eng.getBuilder(), Eng, Pred, checkFn.Checker,
                       IsLoad ? ProgramPoint::PreLoadKind :
                       ProgramPoint::PreStoreKind, 0, S, State);
      checkFn(Loc, IsLoad, C);
    }
  };
}

/// \brief Run checkers for load/store of a location.
void CheckerManager::runCheckersForLocation(ExplodedNodeSet &Dst,
                                            const ExplodedNodeSet &Src,
                                            SVal location, bool isLoad,
                                            const Stmt *S,
                                            const GRState *state,
                                            ExprEngine &Eng) {
  CheckLocationContext C(LocationCheckers, location, isLoad, S, state, Eng);
  expandGraphWithCheckers(C, Dst, Src);
}

void CheckerManager::runCheckersForEndAnalysis(ExplodedGraph &G,
                                               BugReporter &BR,
                                               ExprEngine &Eng) {
  for (unsigned i = 0, e = EndAnalysisCheckers.size(); i != e; ++i)
    EndAnalysisCheckers[i](G, BR, Eng);
}

/// \brief Run checkers for evaluating a call.
/// Only one checker will evaluate the call.
void CheckerManager::runCheckersForEvalCall(ExplodedNodeSet &Dst,
                                            const ExplodedNodeSet &Src,
                                            const CallExpr *CE,
                                            ExprEngine &Eng,
                                            GraphExpander *defaultEval) {
  if (EvalCallCheckers.empty() && defaultEval == 0) {
    Dst.insert(Src);
    return;
  }

  for (ExplodedNodeSet::iterator
         NI = Src.begin(), NE = Src.end(); NI != NE; ++NI) {

    ExplodedNode *Pred = *NI;
    bool anyEvaluated = false;
    for (std::vector<EvalCallFunc>::iterator
           EI = EvalCallCheckers.begin(), EE = EvalCallCheckers.end();
         EI != EE; ++EI) {
      ExplodedNodeSet checkDst;
      CheckerContext C(checkDst, Eng.getBuilder(), Eng, Pred, EI->Checker,
                       ProgramPoint::PostStmtKind, 0, CE);
      bool evaluated = (*EI)(CE, C);
      assert(!(evaluated && anyEvaluated)
             && "There are more than one checkers evaluating the call");
      if (evaluated) {
        anyEvaluated = true;
        Dst.insert(checkDst);
#ifdef NDEBUG
        break; // on release don't check that no other checker also evals.
#endif
      }
    }
    
    if (!anyEvaluated) {
      if (defaultEval)
        defaultEval->expandGraph(Dst, Pred);
      else
        Dst.insert(Pred);
    }
  }
}

void CheckerManager::registerCheckersToEngine(ExprEngine &eng) {
  for (unsigned i = 0, e = Funcs.size(); i != e; ++i)
    Funcs[i](eng);
}

//===----------------------------------------------------------------------===//
// Internal registration functions for AST traversing.
//===----------------------------------------------------------------------===//

void CheckerManager::_registerForDecl(CheckDeclFunc checkfn,
                                      HandlesDeclFunc isForDeclFn) {
  DeclCheckerInfo info = { checkfn, isForDeclFn };
  DeclCheckers.push_back(info);
}

void CheckerManager::_registerForBody(CheckDeclFunc checkfn) {
  BodyCheckers.push_back(checkfn);
}

//===----------------------------------------------------------------------===//
// Internal registration functions for path-sensitive checking.
//===----------------------------------------------------------------------===//

void CheckerManager::_registerForPreStmt(CheckStmtFunc checkfn,
                                         HandlesStmtFunc isForStmtFn) {
  StmtCheckerInfo info = { checkfn, isForStmtFn, /*IsPreVisit*/true };
  StmtCheckers.push_back(info);
}
void CheckerManager::_registerForPostStmt(CheckStmtFunc checkfn,
                                          HandlesStmtFunc isForStmtFn) {
  StmtCheckerInfo info = { checkfn, isForStmtFn, /*IsPreVisit*/false };
  StmtCheckers.push_back(info);
}

void CheckerManager::_registerForPreObjCMessage(CheckObjCMessageFunc checkfn) {
  PreObjCMessageCheckers.push_back(checkfn);
}
void CheckerManager::_registerForPostObjCMessage(CheckObjCMessageFunc checkfn) {
  PostObjCMessageCheckers.push_back(checkfn);
}

void CheckerManager::_registerForLocation(CheckLocationFunc checkfn) {
  LocationCheckers.push_back(checkfn);
}

void CheckerManager::_registerForEndAnalysis(CheckEndAnalysisFunc checkfn) {
  EndAnalysisCheckers.push_back(checkfn);
}

void CheckerManager::_registerForEvalCall(EvalCallFunc checkfn) {
  EvalCallCheckers.push_back(checkfn);
}

//===----------------------------------------------------------------------===//
// Implementation details.
//===----------------------------------------------------------------------===//

CheckerManager::CachedStmtCheckers *
CheckerManager::getCachedStmtCheckersFor(const Stmt *S, bool isPreVisit) {
  assert(S);

  CachedStmtCheckersKey key(S->getStmtClass(), isPreVisit);
  CachedStmtCheckers *checkers = 0;
  CachedStmtCheckersMapTy::iterator CCI = CachedStmtCheckersMap.find(key);
  if (CCI != CachedStmtCheckersMap.end()) {
    checkers = &(CCI->second);
  } else {
    // Find the checkers that should run for this Stmt and cache them.
    checkers = &CachedStmtCheckersMap[key];
    for (unsigned i = 0, e = StmtCheckers.size(); i != e; ++i) {
      StmtCheckerInfo &info = StmtCheckers[i];
      if (info.IsPreVisit == isPreVisit && info.IsForStmtFn(S))
        checkers->push_back(info.CheckFn);
    }
  }

  assert(checkers);
  return checkers;
}

CheckerManager::~CheckerManager() {
  for (unsigned i = 0, e = CheckerDtors.size(); i != e; ++i)
    CheckerDtors[i]();
}

// Anchor for the vtable.
CheckerProvider::~CheckerProvider() { }

// Anchor for the vtable.
GraphExpander::~GraphExpander() { }
