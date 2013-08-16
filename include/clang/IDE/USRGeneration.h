//===- USRGeneration.h - Routines for USR generation ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_IDE_USRGENERATION_H
#define LLVM_CLANG_IDE_USRGENERATION_H

#include "clang/Basic/LLVM.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
  class Decl;

namespace ide {

static inline StringRef getUSRSpacePrefix() {
  return "c:";
}

/// \brief Generate a USR for a Decl, including the prefix.
/// \returns true if the results should be ignored, false otherwise.
bool generateUSRForDecl(const Decl *D, SmallVectorImpl<char> &Buf);

/// \brief Generate a USR fragment for an Objective-C class.
void generateUSRForObjCClass(StringRef Cls, raw_ostream &OS);

/// \brief Generate a USR fragment for an Objective-C class category.
void generateUSRForObjCCategory(StringRef Cls, StringRef Cat, raw_ostream &OS);

/// \brief Generate a USR fragment for an Objective-C instance variable.  The
/// complete USR can be created by concatenating the USR for the
/// encompassing class with this USR fragment.
void generateUSRForObjCIvar(StringRef Ivar, raw_ostream &OS);

/// \brief Generate a USR fragment for an Objective-C method.
void generateUSRForObjCMethod(StringRef Sel, bool IsInstanceMethod,
                              raw_ostream &OS);

/// \brief Generate a USR fragment for an Objective-C property.
void generateUSRForObjCProperty(StringRef Prop, raw_ostream &OS);

/// \brief Generate a USR fragment for an Objective-C protocol.
void generateUSRForObjCProtocol(StringRef Prot, raw_ostream &OS);

} // namespace ide
} // namespace clang

#endif // LLVM_CLANG_IDE_USRGENERATION_H

