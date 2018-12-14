//===--- CodeGenOptions.h ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the CodeGenOptions interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_CODEGENOPTIONS_H
#define LLVM_CLANG_BASIC_CODEGENOPTIONS_H

#include "clang/Basic/DebugInfoOptions.h"
#include "clang/Basic/Sanitizers.h"
#include "clang/Basic/XRayInstr.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Regex.h"
#include "llvm/Target/TargetOptions.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace clang {

/// Bitfields of CodeGenOptions, split out from CodeGenOptions to ensure
/// that this large collection of bitfields is a trivial class type.
class CodeGenOptionsBase {
public:
#define CODEGENOPT(Name, Bits, Default) unsigned Name : Bits;
#define ENUM_CODEGENOPT(Name, Type, Bits, Default)
#include "clang/Basic/CodeGenOptions.def"

protected:
#define CODEGENOPT(Name, Bits, Default)
#define ENUM_CODEGENOPT(Name, Type, Bits, Default) unsigned Name : Bits;
#include "clang/Basic/CodeGenOptions.def"
};

/// CodeGenOptions - Track various options which control how the code
/// is optimized and passed to the backend.
class CodeGenOptions : public CodeGenOptionsBase {
public:
  enum InliningMethod {
    NormalInlining,     // Use the standard function inlining pass.
    OnlyHintInlining,   // Inline only (implicitly) hinted functions.
    OnlyAlwaysInlining  // Only run the always inlining pass.
  };

  enum VectorLibrary {
    NoLibrary,  // Don't use any vector library.
    Accelerate, // Use the Accelerate framework.
    SVML        // Intel short vector math library.
  };


  enum ObjCDispatchMethodKind {
    Legacy = 0,
    NonLegacy = 1,
    Mixed = 2
  };

  enum TLSModel {
    GeneralDynamicTLSModel,
    LocalDynamicTLSModel,
    InitialExecTLSModel,
    LocalExecTLSModel
  };

  enum DwarfFissionKind { NoFission, SplitFileFission, SingleFileFission };

  /// Clang versions with different platform ABI conformance.
  enum class ClangABI {
    /// Attempt to be ABI-compatible with code generated by Clang 3.8.x
    /// (SVN r257626). This causes <1 x long long> to be passed in an
    /// integer register instead of an SSE register on x64_64.
    Ver3_8,

    /// Attempt to be ABI-compatible with code generated by Clang 4.0.x
    /// (SVN r291814). This causes move operations to be ignored when
    /// determining whether a class type can be passed or returned directly.
    Ver4,

    /// Conform to the underlying platform's C and C++ ABIs as closely
    /// as we can.
    Latest
  };

  enum StructReturnConventionKind {
    SRCK_Default,  // No special option was passed.
    SRCK_OnStack,  // Small structs on the stack (-fpcc-struct-return).
    SRCK_InRegs    // Small structs in registers (-freg-struct-return).
  };

  enum ProfileInstrKind {
    ProfileNone,       // Profile instrumentation is turned off.
    ProfileClangInstr, // Clang instrumentation to generate execution counts
                       // to use with PGO.
    ProfileIRInstr,    // IR level PGO instrumentation in LLVM.
  };

  enum EmbedBitcodeKind {
    Embed_Off,      // No embedded bitcode.
    Embed_All,      // Embed both bitcode and commandline in the output.
    Embed_Bitcode,  // Embed just the bitcode in the output.
    Embed_Marker    // Embed a marker as a placeholder for bitcode.
  };

  enum SignReturnAddressScope {
    None,    // No signing for any function
    NonLeaf, // Sign the return address of functions that spill LR
    All      // Sign the return address of all functions
  };

  enum SignReturnAddressKeyValue { AKey, BKey };

  /// The code model to use (-mcmodel).
  std::string CodeModel;

  /// The filename with path we use for coverage data files. The runtime
  /// allows further manipulation with the GCOV_PREFIX and GCOV_PREFIX_STRIP
  /// environment variables.
  std::string CoverageDataFile;

  /// The filename with path we use for coverage notes files.
  std::string CoverageNotesFile;

  /// Regexes separated by a semi-colon to filter the files to instrument.
  std::string ProfileFilterFiles;

  /// Regexes separated by a semi-colon to filter the files to not instrument.
  std::string ProfileExcludeFiles;

  /// The version string to put into coverage files.
  char CoverageVersion[4];

  /// Enable additional debugging information.
  std::string DebugPass;

  /// The string to embed in debug information as the current working directory.
  std::string DebugCompilationDir;

  /// The string to embed in the debug information for the compile unit, if
  /// non-empty.
  std::string DwarfDebugFlags;

  /// The string containing the commandline for the llvm.commandline metadata,
  /// if non-empty.
  std::string RecordCommandLine;

  std::map<std::string, std::string> DebugPrefixMap;

  /// The ABI to use for passing floating point arguments.
  std::string FloatABI;

  /// The floating-point denormal mode to use.
  std::string FPDenormalMode;

  /// The float precision limit to use, if non-empty.
  std::string LimitFloatPrecision;

  struct BitcodeFileToLink {
    /// The filename of the bitcode file to link in.
    std::string Filename;
    /// If true, we set attributes functions in the bitcode library according to
    /// our CodeGenOptions, much as we set attrs on functions that we generate
    /// ourselves.
    bool PropagateAttrs = false;
    /// If true, we use LLVM module internalizer.
    bool Internalize = false;
    /// Bitwise combination of llvm::Linker::Flags, passed to the LLVM linker.
    unsigned LinkFlags = 0;
  };

  /// The files specified here are linked in to the module before optimizations.
  std::vector<BitcodeFileToLink> LinkBitcodeFiles;

  /// The user provided name for the "main file", if non-empty. This is useful
  /// in situations where the input file name does not match the original input
  /// file, for example with -save-temps.
  std::string MainFileName;

  /// The name for the split debug info file that we'll break out. This is used
  /// in the backend for setting the name in the skeleton cu.
  std::string SplitDwarfFile;

  /// The name of the relocation model to use.
  llvm::Reloc::Model RelocationModel;

  /// The thread model to use
  std::string ThreadModel;

  /// If not an empty string, trap intrinsics are lowered to calls to this
  /// function instead of to trap instructions.
  std::string TrapFuncName;

  /// A list of dependent libraries.
  std::vector<std::string> DependentLibraries;

  /// A list of linker options to embed in the object file.
  std::vector<std::string> LinkerOptions;

  /// Name of the profile file to use as output for -fprofile-instr-generate
  /// and -fprofile-generate.
  std::string InstrProfileOutput;

  /// Name of the profile file to use with -fprofile-sample-use.
  std::string SampleProfileFile;

  /// Name of the profile file to use as input for -fprofile-instr-use
  std::string ProfileInstrumentUsePath;

  /// Name of the profile remapping file to apply to the profile data supplied
  /// by -fprofile-sample-use or -fprofile-instr-use.
  std::string ProfileRemappingFile;

  /// Name of the function summary index file to use for ThinLTO function
  /// importing.
  std::string ThinLTOIndexFile;

  /// Name of a file that can optionally be written with minimized bitcode
  /// to be used as input for the ThinLTO thin link step, which only needs
  /// the summary and module symbol table (and not, e.g. any debug metadata).
  std::string ThinLinkBitcodeFile;

  /// Prefix to use for -save-temps output.
  std::string SaveTempsFilePrefix;

  /// Name of file passed with -fcuda-include-gpubinary option to forward to
  /// CUDA runtime back-end for incorporating them into host-side object file.
  std::string CudaGpuBinaryFileName;

  /// The name of the file to which the backend should save YAML optimization
  /// records.
  std::string OptRecordFile;

  /// Regular expression to select optimizations for which we should enable
  /// optimization remarks. Transformation passes whose name matches this
  /// expression (and support this feature), will emit a diagnostic
  /// whenever they perform a transformation. This is enabled by the
  /// -Rpass=regexp flag.
  std::shared_ptr<llvm::Regex> OptimizationRemarkPattern;

  /// Regular expression to select optimizations for which we should enable
  /// missed optimization remarks. Transformation passes whose name matches this
  /// expression (and support this feature), will emit a diagnostic
  /// whenever they tried but failed to perform a transformation. This is
  /// enabled by the -Rpass-missed=regexp flag.
  std::shared_ptr<llvm::Regex> OptimizationRemarkMissedPattern;

  /// Regular expression to select optimizations for which we should enable
  /// optimization analyses. Transformation passes whose name matches this
  /// expression (and support this feature), will emit a diagnostic
  /// whenever they want to explain why they decided to apply or not apply
  /// a given transformation. This is enabled by the -Rpass-analysis=regexp
  /// flag.
  std::shared_ptr<llvm::Regex> OptimizationRemarkAnalysisPattern;

  /// Set of files defining the rules for the symbol rewriting.
  std::vector<std::string> RewriteMapFiles;

  /// Set of sanitizer checks that are non-fatal (i.e. execution should be
  /// continued when possible).
  SanitizerSet SanitizeRecover;

  /// Set of sanitizer checks that trap rather than diagnose.
  SanitizerSet SanitizeTrap;

  /// List of backend command-line options for -fembed-bitcode.
  std::vector<uint8_t> CmdArgs;

  /// A list of all -fno-builtin-* function names (e.g., memset).
  std::vector<std::string> NoBuiltinFuncs;

  std::vector<std::string> Reciprocals;

  /// The preferred width for auto-vectorization transforms. This is intended to
  /// override default transforms based on the width of the architected vector
  /// registers.
  std::string PreferVectorWidth;

  /// Set of XRay instrumentation kinds to emit.
  XRayInstrSet XRayInstrumentationBundle;

public:
  // Define accessors/mutators for code generation options of enumeration type.
#define CODEGENOPT(Name, Bits, Default)
#define ENUM_CODEGENOPT(Name, Type, Bits, Default) \
  Type get##Name() const { return static_cast<Type>(Name); } \
  void set##Name(Type Value) { Name = static_cast<unsigned>(Value); }
#include "clang/Basic/CodeGenOptions.def"

  CodeGenOptions();

  /// Is this a libc/libm function that is no longer recognized as a
  /// builtin because a -fno-builtin-* option has been specified?
  bool isNoBuiltinFunc(const char *Name) const;

  const std::vector<std::string> &getNoBuiltinFuncs() const {
    return NoBuiltinFuncs;
  }

  /// Check if Clang profile instrumenation is on.
  bool hasProfileClangInstr() const {
    return getProfileInstr() == ProfileClangInstr;
  }

  /// Check if IR level profile instrumentation is on.
  bool hasProfileIRInstr() const {
    return getProfileInstr() == ProfileIRInstr;
  }

  /// Check if Clang profile use is on.
  bool hasProfileClangUse() const {
    return getProfileUse() == ProfileClangInstr;
  }

  /// Check if IR level profile use is on.
  bool hasProfileIRUse() const {
    return getProfileUse() == ProfileIRInstr;
  }

};

}  // end namespace clang

#endif
