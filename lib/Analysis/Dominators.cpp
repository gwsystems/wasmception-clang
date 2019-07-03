//===- Dominators.cpp - Implementation of dominators tree for Clang CFG ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/Analyses/Dominators.h"

using namespace clang;

template <>
void clang::CFGDominatorTreeImpl</*IsPostDom=*/true>::anchor() {}

template <>
void clang::CFGDominatorTreeImpl</*IsPostDom=*/false>::anchor() {}
