//== Store.h - Interface for maps from Locations to Values ------*- C++ -*--==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defined the types Store and StoreManager.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_STORE_H
#define LLVM_CLANG_ANALYSIS_STORE_H

#include "clang/Analysis/PathSensitive/RValues.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include <iosfwd>

namespace clang {
  
typedef const void* Store;

class GRState;  
class GRStateManager;
class LiveVariables;
class Stmt;
class Expr;
class ObjCIvarDecl;
class MemRegion;
class MemRegionManager;

class StoreManager {
public:
  typedef llvm::SmallSet<SymbolID, 20>      LiveSymbolsTy;
  typedef llvm::DenseSet<SymbolID>          DeadSymbolsTy;
  
  virtual ~StoreManager() {}
  virtual SVal GetSVal(Store St, Loc LV, QualType T = QualType()) = 0;
  virtual Store SetSVal(Store St, Loc LV, SVal V) = 0;
  virtual Store Remove(Store St, Loc LV) = 0;
  virtual Store getInitialStore() = 0;
  virtual MemRegionManager& getRegionManager() = 0;

  virtual SVal getLValueVar(const GRState* St, const VarDecl* VD) = 0;  
  
  virtual SVal getLValueIvar(const GRState* St, const ObjCIvarDecl* D,
                                SVal Base)=0;
  
  virtual SVal getLValueField(const GRState* St, const FieldDecl* D,
                                 SVal Base) = 0;
  
  virtual SVal getLValueElement(const GRState* St, 
                                   SVal Base, SVal Offset) = 0;
  

  virtual Store
  RemoveDeadBindings(Store store, Stmt* Loc, const LiveVariables& Live,
                     llvm::SmallVectorImpl<const MemRegion*>& RegionRoots,
                     LiveSymbolsTy& LSymbols, DeadSymbolsTy& DSymbols) = 0;

  virtual Store AddDecl(Store store,
                        const VarDecl* VD, Expr* Ex, 
                        SVal InitVal = UndefinedVal(), 
                        unsigned Count = 0) = 0;

  virtual void print(Store store, std::ostream& Out,
                     const char* nl, const char *sep) = 0;
      
  class BindingsHandler {
  public:    
    virtual ~BindingsHandler();
    virtual bool HandleBinding(StoreManager& SMgr, Store store,
                               MemRegion* R, SVal val) = 0;
  };
  
  /// iterBindings - Iterate over the bindings in the Store.
  virtual void iterBindings(Store store, BindingsHandler& f) = 0;  
};
  
StoreManager* CreateBasicStoreManager(GRStateManager& StMgr);
  
} // end clang namespace

#endif
