//===--- DiagnosticIDs.cpp - Diagnostic IDs Handling ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Diagnostic IDs-related interfaces.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTDiagnostic.h"
#include "clang/Analysis/AnalysisDiagnostic.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Lex/LexDiagnostic.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Sema/SemaDiagnostic.h"

#include <map>
using namespace clang;

//===----------------------------------------------------------------------===//
// Builtin Diagnostic information
//===----------------------------------------------------------------------===//

namespace {

// Diagnostic classes.
enum {
  CLASS_NOTE       = 0x01,
  CLASS_WARNING    = 0x02,
  CLASS_EXTENSION  = 0x03,
  CLASS_ERROR      = 0x04
};

struct StaticDiagInfoRec {
  unsigned short DiagID;
  unsigned Mapping : 3;
  unsigned Class : 3;
  bool SFINAE : 1;
  unsigned Category : 5;
  
  const char *Description;
  const char *OptionGroup;

  bool operator<(const StaticDiagInfoRec &RHS) const {
    return DiagID < RHS.DiagID;
  }
};

}

static const StaticDiagInfoRec StaticDiagInfo[] = {
#define DIAG(ENUM,CLASS,DEFAULT_MAPPING,DESC,GROUP,SFINAE, CATEGORY)    \
  { diag::ENUM, DEFAULT_MAPPING, CLASS, SFINAE, CATEGORY, DESC, GROUP },
#include "clang/Basic/DiagnosticCommonKinds.inc"
#include "clang/Basic/DiagnosticDriverKinds.inc"
#include "clang/Basic/DiagnosticFrontendKinds.inc"
#include "clang/Basic/DiagnosticLexKinds.inc"
#include "clang/Basic/DiagnosticParseKinds.inc"
#include "clang/Basic/DiagnosticASTKinds.inc"
#include "clang/Basic/DiagnosticSemaKinds.inc"
#include "clang/Basic/DiagnosticAnalysisKinds.inc"
  { 0, 0, 0, 0, 0, 0, 0}
};
#undef DIAG

/// GetDiagInfo - Return the StaticDiagInfoRec entry for the specified DiagID,
/// or null if the ID is invalid.
static const StaticDiagInfoRec *GetDiagInfo(unsigned DiagID) {
  unsigned NumDiagEntries = sizeof(StaticDiagInfo)/sizeof(StaticDiagInfo[0])-1;

  // If assertions are enabled, verify that the StaticDiagInfo array is sorted.
#ifndef NDEBUG
  static bool IsFirst = true;
  if (IsFirst) {
    for (unsigned i = 1; i != NumDiagEntries; ++i) {
      assert(StaticDiagInfo[i-1].DiagID != StaticDiagInfo[i].DiagID &&
             "Diag ID conflict, the enums at the start of clang::diag (in "
             "DiagnosticIDs.h) probably need to be increased");

      assert(StaticDiagInfo[i-1] < StaticDiagInfo[i] &&
             "Improperly sorted diag info");
    }
    IsFirst = false;
  }
#endif

  // Search the diagnostic table with a binary search.
  StaticDiagInfoRec Find = { DiagID, 0, 0, 0, 0, 0, 0 };

  const StaticDiagInfoRec *Found =
    std::lower_bound(StaticDiagInfo, StaticDiagInfo + NumDiagEntries, Find);
  if (Found == StaticDiagInfo + NumDiagEntries ||
      Found->DiagID != DiagID)
    return 0;

  return Found;
}

static unsigned GetDefaultDiagMapping(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->Mapping;
  return diag::MAP_FATAL;
}

/// getWarningOptionForDiag - Return the lowest-level warning option that
/// enables the specified diagnostic.  If there is no -Wfoo flag that controls
/// the diagnostic, this returns null.
const char *DiagnosticIDs::getWarningOptionForDiag(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->OptionGroup;
  return 0;
}

/// getWarningOptionForDiag - Return the category number that a specified
/// DiagID belongs to, or 0 if no category.
unsigned DiagnosticIDs::getCategoryNumberForDiag(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->Category;
  return 0;
}

/// getCategoryNameFromID - Given a category ID, return the name of the
/// category, an empty string if CategoryID is zero, or null if CategoryID is
/// invalid.
const char *DiagnosticIDs::getCategoryNameFromID(unsigned CategoryID) {
  // Second the table of options, sorted by name for fast binary lookup.
  static const char *CategoryNameTable[] = {
#define GET_CATEGORY_TABLE
#define CATEGORY(X) X,
#include "clang/Basic/DiagnosticGroups.inc"
#undef GET_CATEGORY_TABLE
    "<<END>>"
  };
  static const size_t CategoryNameTableSize =
    sizeof(CategoryNameTable) / sizeof(CategoryNameTable[0])-1;
  
  if (CategoryID >= CategoryNameTableSize) return 0;
  return CategoryNameTable[CategoryID];
}



DiagnosticIDs::SFINAEResponse 
DiagnosticIDs::getDiagnosticSFINAEResponse(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID)) {
    if (!Info->SFINAE)
      return SFINAE_Report;

    if (Info->Class == CLASS_ERROR)
      return SFINAE_SubstitutionFailure;
    
    // Suppress notes, warnings, and extensions;
    return SFINAE_Suppress;
  }
  
  return SFINAE_Report;
}

/// getDiagClass - Return the class field of the diagnostic.
///
static unsigned getBuiltinDiagClass(unsigned DiagID) {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->Class;
  return ~0U;
}

//===----------------------------------------------------------------------===//
// Custom Diagnostic information
//===----------------------------------------------------------------------===//

namespace clang {
  namespace diag {
    class CustomDiagInfo {
      typedef std::pair<DiagnosticIDs::Level, std::string> DiagDesc;
      std::vector<DiagDesc> DiagInfo;
      std::map<DiagDesc, unsigned> DiagIDs;
    public:

      /// getDescription - Return the description of the specified custom
      /// diagnostic.
      const char *getDescription(unsigned DiagID) const {
        assert(this && DiagID-DIAG_UPPER_LIMIT < DiagInfo.size() &&
               "Invalid diagnosic ID");
        return DiagInfo[DiagID-DIAG_UPPER_LIMIT].second.c_str();
      }

      /// getLevel - Return the level of the specified custom diagnostic.
      DiagnosticIDs::Level getLevel(unsigned DiagID) const {
        assert(this && DiagID-DIAG_UPPER_LIMIT < DiagInfo.size() &&
               "Invalid diagnosic ID");
        return DiagInfo[DiagID-DIAG_UPPER_LIMIT].first;
      }

      unsigned getOrCreateDiagID(DiagnosticIDs::Level L, llvm::StringRef Message,
                                 DiagnosticIDs &Diags) {
        DiagDesc D(L, Message);
        // Check to see if it already exists.
        std::map<DiagDesc, unsigned>::iterator I = DiagIDs.lower_bound(D);
        if (I != DiagIDs.end() && I->first == D)
          return I->second;

        // If not, assign a new ID.
        unsigned ID = DiagInfo.size()+DIAG_UPPER_LIMIT;
        DiagIDs.insert(std::make_pair(D, ID));
        DiagInfo.push_back(D);
        return ID;
      }
    };

  } // end diag namespace
} // end clang namespace


//===----------------------------------------------------------------------===//
// Common Diagnostic implementation
//===----------------------------------------------------------------------===//

DiagnosticIDs::DiagnosticIDs() {
  CustomDiagInfo = 0;
}

DiagnosticIDs::~DiagnosticIDs() {
  delete CustomDiagInfo;
}

/// getCustomDiagID - Return an ID for a diagnostic with the specified message
/// and level.  If this is the first request for this diagnosic, it is
/// registered and created, otherwise the existing ID is returned.
unsigned DiagnosticIDs::getCustomDiagID(Level L, llvm::StringRef Message) {
  if (CustomDiagInfo == 0)
    CustomDiagInfo = new diag::CustomDiagInfo();
  return CustomDiagInfo->getOrCreateDiagID(L, Message, *this);
}


/// isBuiltinWarningOrExtension - Return true if the unmapped diagnostic
/// level of the specified diagnostic ID is a Warning or Extension.
/// This only works on builtin diagnostics, not custom ones, and is not legal to
/// call on NOTEs.
bool DiagnosticIDs::isBuiltinWarningOrExtension(unsigned DiagID) {
  return DiagID < diag::DIAG_UPPER_LIMIT &&
         getBuiltinDiagClass(DiagID) != CLASS_ERROR;
}

/// \brief Determine whether the given built-in diagnostic ID is a
/// Note.
bool DiagnosticIDs::isBuiltinNote(unsigned DiagID) {
  return DiagID < diag::DIAG_UPPER_LIMIT &&
    getBuiltinDiagClass(DiagID) == CLASS_NOTE;
}

/// isBuiltinExtensionDiag - Determine whether the given built-in diagnostic
/// ID is for an extension of some sort.  This also returns EnabledByDefault,
/// which is set to indicate whether the diagnostic is ignored by default (in
/// which case -pedantic enables it) or treated as a warning/error by default.
///
bool DiagnosticIDs::isBuiltinExtensionDiag(unsigned DiagID,
                                        bool &EnabledByDefault) {
  if (DiagID >= diag::DIAG_UPPER_LIMIT ||
      getBuiltinDiagClass(DiagID) != CLASS_EXTENSION)
    return false;
  
  EnabledByDefault = GetDefaultDiagMapping(DiagID) != diag::MAP_IGNORE;
  return true;
}

/// getDescription - Given a diagnostic ID, return a description of the
/// issue.
const char *DiagnosticIDs::getDescription(unsigned DiagID) const {
  if (const StaticDiagInfoRec *Info = GetDiagInfo(DiagID))
    return Info->Description;
  return CustomDiagInfo->getDescription(DiagID);
}

/// getDiagnosticLevel - Based on the way the client configured the Diagnostic
/// object, classify the specified diagnostic ID into a Level, consumable by
/// the DiagnosticClient.
DiagnosticIDs::Level
DiagnosticIDs::getDiagnosticLevel(unsigned DiagID, SourceLocation Loc,
                                  const Diagnostic &Diag) const {
  // Handle custom diagnostics, which cannot be mapped.
  if (DiagID >= diag::DIAG_UPPER_LIMIT)
    return CustomDiagInfo->getLevel(DiagID);

  unsigned DiagClass = getBuiltinDiagClass(DiagID);
  assert(DiagClass != CLASS_NOTE && "Cannot get diagnostic level of a note!");
  return getDiagnosticLevel(DiagID, DiagClass, Loc, Diag);
}

/// \brief Based on the way the client configured the Diagnostic
/// object, classify the specified diagnostic ID into a Level, consumable by
/// the DiagnosticClient.
///
/// \param Loc The source location we are interested in finding out the
/// diagnostic state. Can be null in order to query the latest state.
DiagnosticIDs::Level
DiagnosticIDs::getDiagnosticLevel(unsigned DiagID, unsigned DiagClass,
                                  SourceLocation Loc,
                                  const Diagnostic &Diag) const {
  // Specific non-error diagnostics may be mapped to various levels from ignored
  // to error.  Errors can only be mapped to fatal.
  DiagnosticIDs::Level Result = DiagnosticIDs::Fatal;

  Diagnostic::DiagStatePointsTy::iterator
    Pos = Diag.GetDiagStatePointForLoc(Loc);
  Diagnostic::DiagState *State = Pos->State;

  // Get the mapping information, if unset, compute it lazily.
  unsigned MappingInfo = Diag.getDiagnosticMappingInfo((diag::kind)DiagID,
                                                       State);
  if (MappingInfo == 0) {
    MappingInfo = GetDefaultDiagMapping(DiagID);
    Diag.setDiagnosticMappingInternal(DiagID, MappingInfo, State, false);
  }

  switch (MappingInfo & 7) {
  default: assert(0 && "Unknown mapping!");
  case diag::MAP_IGNORE:
    // Ignore this, unless this is an extension diagnostic and we're mapping
    // them onto warnings or errors.
    if (!isBuiltinExtensionDiag(DiagID) ||  // Not an extension
        Diag.ExtBehavior == Diagnostic::Ext_Ignore || // Ext ignored
        (MappingInfo & 8) != 0)             // User explicitly mapped it.
      return DiagnosticIDs::Ignored;
    Result = DiagnosticIDs::Warning;
    if (Diag.ExtBehavior == Diagnostic::Ext_Error) Result = DiagnosticIDs::Error;
    if (Result == DiagnosticIDs::Error && Diag.ErrorsAsFatal)
      Result = DiagnosticIDs::Fatal;
    break;
  case diag::MAP_ERROR:
    Result = DiagnosticIDs::Error;
    if (Diag.ErrorsAsFatal)
      Result = DiagnosticIDs::Fatal;
    break;
  case diag::MAP_FATAL:
    Result = DiagnosticIDs::Fatal;
    break;
  case diag::MAP_WARNING:
    // If warnings are globally mapped to ignore or error, do it.
    if (Diag.IgnoreAllWarnings)
      return DiagnosticIDs::Ignored;

    Result = DiagnosticIDs::Warning;

    // If this is an extension diagnostic and we're in -pedantic-error mode, and
    // if the user didn't explicitly map it, upgrade to an error.
    if (Diag.ExtBehavior == Diagnostic::Ext_Error &&
        (MappingInfo & 8) == 0 &&
        isBuiltinExtensionDiag(DiagID))
      Result = DiagnosticIDs::Error;

    if (Diag.WarningsAsErrors)
      Result = DiagnosticIDs::Error;
    if (Result == DiagnosticIDs::Error && Diag.ErrorsAsFatal)
      Result = DiagnosticIDs::Fatal;
    break;

  case diag::MAP_WARNING_NO_WERROR:
    // Diagnostics specified with -Wno-error=foo should be set to warnings, but
    // not be adjusted by -Werror or -pedantic-errors.
    Result = DiagnosticIDs::Warning;

    // If warnings are globally mapped to ignore or error, do it.
    if (Diag.IgnoreAllWarnings)
      return DiagnosticIDs::Ignored;

    break;

  case diag::MAP_ERROR_NO_WFATAL:
    // Diagnostics specified as -Wno-fatal-error=foo should be errors, but
    // unaffected by -Wfatal-errors.
    Result = DiagnosticIDs::Error;
    break;
  }

  // Okay, we're about to return this as a "diagnostic to emit" one last check:
  // if this is any sort of extension warning, and if we're in an __extension__
  // block, silence it.
  if (Diag.AllExtensionsSilenced && isBuiltinExtensionDiag(DiagID))
    return DiagnosticIDs::Ignored;

  return Result;
}

struct WarningOption {
  const char  *Name;
  const short *Members;
  const short *SubGroups;
};

#define GET_DIAG_ARRAYS
#include "clang/Basic/DiagnosticGroups.inc"
#undef GET_DIAG_ARRAYS

// Second the table of options, sorted by name for fast binary lookup.
static const WarningOption OptionTable[] = {
#define GET_DIAG_TABLE
#include "clang/Basic/DiagnosticGroups.inc"
#undef GET_DIAG_TABLE
};
static const size_t OptionTableSize =
sizeof(OptionTable) / sizeof(OptionTable[0]);

static bool WarningOptionCompare(const WarningOption &LHS,
                                 const WarningOption &RHS) {
  return strcmp(LHS.Name, RHS.Name) < 0;
}

static void MapGroupMembers(const WarningOption *Group, diag::Mapping Mapping,
                            SourceLocation Loc, Diagnostic &Diag) {
  // Option exists, poke all the members of its diagnostic set.
  if (const short *Member = Group->Members) {
    for (; *Member != -1; ++Member)
      Diag.setDiagnosticMapping(*Member, Mapping, Loc);
  }

  // Enable/disable all subgroups along with this one.
  if (const short *SubGroups = Group->SubGroups) {
    for (; *SubGroups != (short)-1; ++SubGroups)
      MapGroupMembers(&OptionTable[(short)*SubGroups], Mapping, Loc, Diag);
  }
}

/// setDiagnosticGroupMapping - Change an entire diagnostic group (e.g.
/// "unknown-pragmas" to have the specified mapping.  This returns true and
/// ignores the request if "Group" was unknown, false otherwise.
bool DiagnosticIDs::setDiagnosticGroupMapping(const char *Group,
                                           diag::Mapping Map,
                                           SourceLocation Loc,
                                           Diagnostic &Diag) const {
  assert((Loc.isValid() ||
          Diag.DiagStatePoints.empty() ||
          Diag.DiagStatePoints.back().Loc.isInvalid()) &&
         "Loc should be invalid only when the mapping comes from command-line");
  assert((Loc.isInvalid() || Diag.DiagStatePoints.empty() ||
          Diag.DiagStatePoints.back().Loc.isInvalid() ||
          !Diag.SourceMgr->isBeforeInTranslationUnit(Loc,
                                            Diag.DiagStatePoints.back().Loc)) &&
         "Source location of new mapping is before the previous one!");

  WarningOption Key = { Group, 0, 0 };
  const WarningOption *Found =
  std::lower_bound(OptionTable, OptionTable + OptionTableSize, Key,
                   WarningOptionCompare);
  if (Found == OptionTable + OptionTableSize ||
      strcmp(Found->Name, Group) != 0)
    return true;  // Option not found.

  MapGroupMembers(Found, Map, Loc, Diag);
  return false;
}

/// ProcessDiag - This is the method used to report a diagnostic that is
/// finally fully formed.
bool DiagnosticIDs::ProcessDiag(Diagnostic &Diag) const {
  DiagnosticInfo Info(&Diag);

  if (Diag.SuppressAllDiagnostics)
    return false;

  assert(Diag.getClient() && "DiagnosticClient not set!");

  // Figure out the diagnostic level of this message.
  DiagnosticIDs::Level DiagLevel;
  unsigned DiagID = Info.getID();

  // ShouldEmitInSystemHeader - True if this diagnostic should be produced even
  // in a system header.
  bool ShouldEmitInSystemHeader;

  if (DiagID >= diag::DIAG_UPPER_LIMIT) {
    // Handle custom diagnostics, which cannot be mapped.
    DiagLevel = CustomDiagInfo->getLevel(DiagID);

    // Custom diagnostics always are emitted in system headers.
    ShouldEmitInSystemHeader = true;
  } else {
    // Get the class of the diagnostic.  If this is a NOTE, map it onto whatever
    // the diagnostic level was for the previous diagnostic so that it is
    // filtered the same as the previous diagnostic.
    unsigned DiagClass = getBuiltinDiagClass(DiagID);
    if (DiagClass == CLASS_NOTE) {
      DiagLevel = DiagnosticIDs::Note;
      ShouldEmitInSystemHeader = false;  // extra consideration is needed
    } else {
      // If this is not an error and we are in a system header, we ignore it.
      // Check the original Diag ID here, because we also want to ignore
      // extensions and warnings in -Werror and -pedantic-errors modes, which
      // *map* warnings/extensions to errors.
      ShouldEmitInSystemHeader = DiagClass == CLASS_ERROR;

      DiagLevel = getDiagnosticLevel(DiagID, DiagClass, Info.getLocation(),
                                     Diag);
    }
  }

  if (DiagLevel != DiagnosticIDs::Note) {
    // Record that a fatal error occurred only when we see a second
    // non-note diagnostic. This allows notes to be attached to the
    // fatal error, but suppresses any diagnostics that follow those
    // notes.
    if (Diag.LastDiagLevel == DiagnosticIDs::Fatal)
      Diag.FatalErrorOccurred = true;

    Diag.LastDiagLevel = DiagLevel;
  }

  // If a fatal error has already been emitted, silence all subsequent
  // diagnostics.
  if (Diag.FatalErrorOccurred) {
    if (DiagLevel >= DiagnosticIDs::Error &&
        Diag.Client->IncludeInDiagnosticCounts()) {
      ++Diag.NumErrors;
      ++Diag.NumErrorsSuppressed;
    }

    return false;
  }

  // If the client doesn't care about this message, don't issue it.  If this is
  // a note and the last real diagnostic was ignored, ignore it too.
  if (DiagLevel == DiagnosticIDs::Ignored ||
      (DiagLevel == DiagnosticIDs::Note &&
       Diag.LastDiagLevel == DiagnosticIDs::Ignored))
    return false;

  // If this diagnostic is in a system header and is not a clang error, suppress
  // it.
  if (Diag.SuppressSystemWarnings && !ShouldEmitInSystemHeader &&
      Info.getLocation().isValid() &&
      Diag.getSourceManager().isInSystemHeader(
          Diag.getSourceManager().getInstantiationLoc(Info.getLocation())) &&
      (DiagLevel != DiagnosticIDs::Note ||
       Diag.LastDiagLevel == DiagnosticIDs::Ignored)) {
    Diag.LastDiagLevel = DiagnosticIDs::Ignored;
    return false;
  }

  if (DiagLevel >= DiagnosticIDs::Error) {
    if (Diag.Client->IncludeInDiagnosticCounts()) {
      Diag.ErrorOccurred = true;
      ++Diag.NumErrors;
    }

    // If we've emitted a lot of errors, emit a fatal error after it to stop a
    // flood of bogus errors.
    if (Diag.ErrorLimit && Diag.NumErrors >= Diag.ErrorLimit &&
        DiagLevel == DiagnosticIDs::Error)
      Diag.SetDelayedDiagnostic(diag::fatal_too_many_errors);
  }

  // Finally, report it.
  Diag.Client->HandleDiagnostic((Diagnostic::Level)DiagLevel, Info);
  if (Diag.Client->IncludeInDiagnosticCounts()) {
    if (DiagLevel == DiagnosticIDs::Warning)
      ++Diag.NumWarnings;
  }

  Diag.CurDiagID = ~0U;

  return true;
}
