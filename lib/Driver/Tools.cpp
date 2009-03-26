//===--- Tools.cpp - Tools Implementations ------------------------------*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Tools.h"

#include "clang/Driver/Action.h"
#include "clang/Driver/Arg.h"
#include "clang/Driver/ArgList.h"
#include "clang/Driver/Driver.h" // FIXME: Remove?
#include "clang/Driver/DriverDiagnostic.h" // FIXME: Remove?
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Job.h"
#include "clang/Driver/HostInfo.h"
#include "clang/Driver/Option.h"
#include "clang/Driver/ToolChain.h"
#include "clang/Driver/Util.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include "InputInfo.h"
#include "ToolChains.h"

using namespace clang::driver;
using namespace clang::driver::tools;

void Clang::ConstructJob(Compilation &C, const JobAction &JA,
                         Job &Dest,
                         const InputInfo &Output,
                         const InputInfoList &Inputs,
                         const ArgList &Args,
                         const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  if (isa<AnalyzeJobAction>(JA)) {
    assert(JA.getType() == types::TY_Plist && "Invalid output type.");
    CmdArgs.push_back("-analyze");
  } else if (isa<PreprocessJobAction>(JA)) {
    CmdArgs.push_back("-E");
  } else if (isa<PrecompileJobAction>(JA)) {
    // No special option needed, driven by -x.
    //
    // FIXME: Don't drive this by -x, that is gross.
  } else {
    assert(isa<CompileJobAction>(JA) && "Invalid action for clang tool.");
  
    if (JA.getType() == types::TY_Nothing) {
      CmdArgs.push_back("-fsyntax-only");
    } else if (JA.getType() == types::TY_LLVMAsm) {
      CmdArgs.push_back("-emit-llvm");
    } else if (JA.getType() == types::TY_LLVMBC) {
      CmdArgs.push_back("-emit-llvm-bc");
    } else if (JA.getType() == types::TY_PP_Asm) {
      CmdArgs.push_back("-S");
    }
  }

  // The make clang go fast button.
  CmdArgs.push_back("-disable-free");

  if (isa<AnalyzeJobAction>(JA)) {
    // Add default argument set.
    //
    // FIXME: Move into clang?
    CmdArgs.push_back("-warn-dead-stores");
    CmdArgs.push_back("-checker-cfref");
    CmdArgs.push_back("-analyzer-eagerly-assume");
    CmdArgs.push_back("-warn-objc-methodsigs");
    // Do not enable the missing -dealloc check.
    // '-warn-objc-missing-dealloc',
    CmdArgs.push_back("-warn-objc-unused-ivars");
    
    CmdArgs.push_back("-analyzer-output=plist");

    // Add -Xanalyzer arguments when running as analyzer.
    Args.AddAllArgValues(CmdArgs, options::OPT_Xanalyzer);
  } else {
    // Perform argument translation for LLVM backend. This
    // takes some care in reconciling with llvm-gcc. The
    // issue is that llvm-gcc translates these options based on
    // the values in cc1, whereas we are processing based on
    // the driver arguments.
    //
    // FIXME: This is currently broken for -f flags when -fno
    // variants are present.
    
    // This comes from the default translation the driver + cc1
    // would do to enable flag_pic.
    // 
    // FIXME: Centralize this code.
    bool PICEnabled = (Args.hasArg(options::OPT_fPIC) ||
                       Args.hasArg(options::OPT_fpic) ||
                       Args.hasArg(options::OPT_fPIE) ||
                       Args.hasArg(options::OPT_fpie));
    bool PICDisabled = (Args.hasArg(options::OPT_mkernel) ||
                        Args.hasArg(options::OPT_static));
    const char *Model = getToolChain().GetForcedPicModel();
    if (!Model) {
      if (Args.hasArg(options::OPT_mdynamic_no_pic))
        Model = "dynamic-no-pic";
      else if (PICDisabled)
        Model = "static";
      else if (PICEnabled)
        Model = "pic";
      else
        Model = getToolChain().GetDefaultRelocationModel();
    }
    CmdArgs.push_back("--relocation-model");
    CmdArgs.push_back(Model);

    if (Args.hasArg(options::OPT_ftime_report))
      CmdArgs.push_back("--time-passes");
    // FIXME: Set --enable-unsafe-fp-math.
    if (!Args.hasArg(options::OPT_fomit_frame_pointer))
      CmdArgs.push_back("--disable-fp-elim");
    if (!Args.hasFlag(options::OPT_fzero_initialized_in_bss,
                      options::OPT_fno_zero_initialized_in_bss,
                      true))
      CmdArgs.push_back("--nozero-initialized-in-bss");
    if (Args.hasArg(options::OPT_dA) || Args.hasArg(options::OPT_fverbose_asm))
      CmdArgs.push_back("--asm-verbose");
    if (Args.hasArg(options::OPT_fdebug_pass_structure))
      CmdArgs.push_back("--debug-pass=Structure");
    if (Args.hasArg(options::OPT_fdebug_pass_arguments))
      CmdArgs.push_back("--debug-pass=Arguments");
    // FIXME: set --inline-threshhold=50 if (optimize_size || optimize
    // < 3)
    if (Args.hasFlag(options::OPT_funwind_tables,
                      options::OPT_fno_unwind_tables,
                      getToolChain().IsUnwindTablesDefault()))
      CmdArgs.push_back("--unwind-tables=1");
    else
      CmdArgs.push_back("--unwind-tables=0");
    if (!Args.hasFlag(options::OPT_mred_zone,
                       options::OPT_mno_red_zone,
                       true))
      CmdArgs.push_back("--disable-red-zone");
    if (Args.hasFlag(options::OPT_msoft_float,
                      options::OPT_mno_soft_float,
                      false))
      CmdArgs.push_back("--soft-float");
        
    // FIXME: Need target hooks.
    if (memcmp(getToolChain().getPlatform().c_str(), "darwin", 6) == 0) {
      if (getToolChain().getArchName() == "x86_64")
        CmdArgs.push_back("--mcpu=core2");
      else if (getToolChain().getArchName() == "i386")
        CmdArgs.push_back("--mcpu=yonah");
    }
    
    // FIXME: Ignores ordering. Also, we need to find a realistic
    // solution for this.
    static const struct { 
      options::ID Pos, Neg; 
      const char *Name; 
    } FeatureOptions[] = {
      { options::OPT_mmmx, options::OPT_mno_mmx, "mmx" },
      { options::OPT_msse, options::OPT_mno_sse, "sse" },
      { options::OPT_msse2, options::OPT_mno_sse2, "sse2" },
      { options::OPT_msse3, options::OPT_mno_sse3, "sse3" },
      { options::OPT_mssse3, options::OPT_mno_ssse3, "ssse3" },
      { options::OPT_msse41, options::OPT_mno_sse41, "sse41" },
      { options::OPT_msse42, options::OPT_mno_sse42, "sse42" },
      { options::OPT_msse4a, options::OPT_mno_sse4a, "sse4a" },
      { options::OPT_m3dnow, options::OPT_mno_3dnow, "3dnow" },
      { options::OPT_m3dnowa, options::OPT_mno_3dnowa, "3dnowa" }
    };
    const unsigned NumFeatureOptions = 
      sizeof(FeatureOptions)/sizeof(FeatureOptions[0]);

    // FIXME: Avoid std::string
    std::string Attrs;
    for (unsigned i=0; i < NumFeatureOptions; ++i) {
      if (Args.hasArg(FeatureOptions[i].Pos)) {
        if (!Attrs.empty())
          Attrs += ',';
        Attrs += '+';
        Attrs += FeatureOptions[i].Name;
      } else if (Args.hasArg(FeatureOptions[i].Neg)) {
        if (!Attrs.empty())
          Attrs += ',';
        Attrs += '-';
        Attrs += FeatureOptions[i].Name;
      }
    }
    if (!Attrs.empty()) {
      CmdArgs.push_back("--mattr");
      CmdArgs.push_back(Args.MakeArgString(Attrs.c_str()));
    }

    if (Args.hasFlag(options::OPT_fmath_errno,
                      options::OPT_fno_math_errno,
                      getToolChain().IsMathErrnoDefault()))
      CmdArgs.push_back("--fmath-errno=1");
    else
      CmdArgs.push_back("--fmath-errno=0");

    if (Arg *A = Args.getLastArg(options::OPT_flimited_precision_EQ)) {
      CmdArgs.push_back("--limit-float-precision");
      CmdArgs.push_back(A->getValue(Args));
    }
    
    // FIXME: Add --stack-protector-buffer-size=<xxx> on
    // -fstack-protect.

    Args.AddLastArg(CmdArgs, options::OPT_MD);
    Args.AddLastArg(CmdArgs, options::OPT_MMD);
    Args.AddAllArgs(CmdArgs, options::OPT_MF);
    Args.AddLastArg(CmdArgs, options::OPT_MP);
    Args.AddAllArgs(CmdArgs, options::OPT_MT);

    Arg *Unsupported = Args.getLastArg(options::OPT_M);
    if (!Unsupported) 
      Unsupported = Args.getLastArg(options::OPT_MM);
    if (!Unsupported) 
      Unsupported = Args.getLastArg(options::OPT_MG);
    if (!Unsupported) 
      Unsupported = Args.getLastArg(options::OPT_MQ);
    if (Unsupported) {
      const Driver &D = getToolChain().getHost().getDriver();
      D.Diag(clang::diag::err_drv_unsupported_opt) 
        << Unsupported->getOption().getName();
    }
  }

  Args.AddAllArgs(CmdArgs, options::OPT_v);
  Args.AddAllArgs(CmdArgs, options::OPT_D, options::OPT_U);
  Args.AddAllArgs(CmdArgs, options::OPT_I_Group, options::OPT_F);
  Args.AddLastArg(CmdArgs, options::OPT_P);
  Args.AddAllArgs(CmdArgs, options::OPT_mmacosx_version_min_EQ);

  // Special case debug options to only pass -g to clang. This is
  // wrong.
  if (Args.hasArg(options::OPT_g_Group))
    CmdArgs.push_back("-g");

  Args.AddLastArg(CmdArgs, options::OPT_nostdinc);

  // FIXME: Clang isn't going to accept just anything here.
  // FIXME: Use iterator.

  // Add -i* options, and automatically translate to -include-pth for
  // transparent PCH support. It's wonky, but we include looking for
  // .gch so we can support seamless replacement into a build system
  // already set up to be generating .gch files.
  for (ArgList::const_iterator 
         it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    const Arg *A = *it;
    if (!A->getOption().matches(options::OPT_i_Group)) 
      continue;

    if (A->getOption().matches(options::OPT_include)) {
      bool FoundPTH = false;
      llvm::sys::Path P(A->getValue(Args));
      P.appendSuffix("pth");
      if (P.exists()) {
        FoundPTH = true;
      } else {
        P.eraseSuffix();
        P.appendSuffix("gch");
        if (P.exists())
          FoundPTH = true;
      }

      if (FoundPTH) {
        A->claim();
        CmdArgs.push_back("-include-pth");
        CmdArgs.push_back(Args.MakeArgString(P.c_str()));
        continue;
      }
    }

    // Not translated, render as usual.
    A->claim();
    A->render(Args, CmdArgs);
  }

  // Manually translate -O to -O1 and -O4 to -O3; let clang reject
  // others.
  if (Arg *A = Args.getLastArg(options::OPT_O_Group)) {
    if (A->getOption().getId() == options::OPT_O4) 
      CmdArgs.push_back("-O3");
    else if (A->getValue(Args)[0] == '\0')
      CmdArgs.push_back("-O1");
    else
      A->render(Args, CmdArgs);
  }

  Args.AddAllArgs(CmdArgs, options::OPT_clang_W_Group, 
                  options::OPT_pedantic_Group);
  Args.AddLastArg(CmdArgs, options::OPT_w);
  Args.AddAllArgs(CmdArgs, options::OPT_std_EQ, options::OPT_ansi, 
                  options::OPT_trigraphs);
  
  if (Arg *A = Args.getLastArg(options::OPT_ftemplate_depth_)) {
    CmdArgs.push_back("-ftemplate-depth");
    CmdArgs.push_back(A->getValue(Args));
  }

  Args.AddAllArgs(CmdArgs, options::OPT_clang_f_Group);

  Args.AddLastArg(CmdArgs, options::OPT_dM);

  Args.AddAllArgValues(CmdArgs, options::OPT_Xclang);

  // FIXME: Always pass the full triple once we aren't concerned with
  // ccc compat.
  CmdArgs.push_back("-arch");
  CmdArgs.push_back(getToolChain().getArchName().c_str());

  if (Output.isPipe()) {
    CmdArgs.push_back("-o");
    CmdArgs.push_back("-");
  } else if (Output.isFilename()) {
    CmdArgs.push_back("-o");
    CmdArgs.push_back(Output.getFilename());
  } else {
    assert(Output.isNothing() && "Invalid output.");
  }

  for (InputInfoList::const_iterator
         it = Inputs.begin(), ie = Inputs.end(); it != ie; ++it) {
    const InputInfo &II = *it;
    CmdArgs.push_back("-x");
    CmdArgs.push_back(types::getTypeName(II.getType()));
    if (II.isPipe())
      CmdArgs.push_back("-");
    else if (II.isFilename())
      CmdArgs.push_back(II.getFilename());
    else
      II.getInputArg().renderAsInput(Args, CmdArgs);
  }
      
  const char *Exec = 
    Args.MakeArgString(getToolChain().GetProgramPath(C, "clang-cc").c_str());
  Dest.addCommand(new Command(Exec, CmdArgs));

  // Claim some arguments which clang doesn't support, but we don't
  // care to warn the user about.
  
  // FIXME: Use iterator.
  for (ArgList::const_iterator 
         it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    const Arg *A = *it;
    if (A->getOption().matches(options::OPT_clang_ignored_W_Group) ||
        A->getOption().matches(options::OPT_clang_ignored_f_Group))
      A->claim();
  }
}

void gcc::Common::ConstructJob(Compilation &C, const JobAction &JA,
                               Job &Dest,
                               const InputInfo &Output,
                               const InputInfoList &Inputs,
                               const ArgList &Args,
                               const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  for (ArgList::const_iterator 
         it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    Arg *A = *it;
    if (A->getOption().hasForwardToGCC()) {
      // It is unfortunate that we have to claim here, as this means
      // we will basically never report anything interesting for
      // platforms using a generic gcc.
      A->claim();
      A->render(Args, CmdArgs);
    }
  }
  
  RenderExtraToolArgs(CmdArgs);

  // If using a driver driver, force the arch.
  if (getToolChain().getHost().useDriverDriver()) {
    CmdArgs.push_back("-arch");
    CmdArgs.push_back(getToolChain().getArchName().c_str());
  }

  if (Output.isPipe()) {
    CmdArgs.push_back("-o");
    CmdArgs.push_back("-");
  } else if (Output.isFilename()) {
    CmdArgs.push_back("-o");
    CmdArgs.push_back(Output.getFilename());
  } else {
    assert(Output.isNothing() && "Unexpected output");
    CmdArgs.push_back("-fsyntax-only");
  }


  // Only pass -x if gcc will understand it; otherwise hope gcc
  // understands the suffix correctly. The main use case this would go
  // wrong in is for linker inputs if they happened to have an odd
  // suffix; really the only way to get this to happen is a command
  // like '-x foobar a.c' which will treat a.c like a linker input.
  //
  // FIXME: For the linker case specifically, can we safely convert
  // inputs into '-Wl,' options?
  for (InputInfoList::const_iterator
         it = Inputs.begin(), ie = Inputs.end(); it != ie; ++it) {
    const InputInfo &II = *it;
    if (types::canTypeBeUserSpecified(II.getType())) {
      CmdArgs.push_back("-x");
      CmdArgs.push_back(types::getTypeName(II.getType()));
    }

    if (II.isPipe())
      CmdArgs.push_back("-");
    else if (II.isFilename())
      CmdArgs.push_back(II.getFilename());
    else
      // Don't render as input, we need gcc to do the translations.
      II.getInputArg().render(Args, CmdArgs);
  }

  const char *Exec = 
    Args.MakeArgString(getToolChain().GetProgramPath(C, "gcc").c_str());
  Dest.addCommand(new Command(Exec, CmdArgs));
}

void gcc::Preprocess::RenderExtraToolArgs(ArgStringList &CmdArgs) const {
  CmdArgs.push_back("-E");
}

void gcc::Precompile::RenderExtraToolArgs(ArgStringList &CmdArgs) const {
  // The type is good enough.
}

void gcc::Compile::RenderExtraToolArgs(ArgStringList &CmdArgs) const {
  CmdArgs.push_back("-S");
}

void gcc::Assemble::RenderExtraToolArgs(ArgStringList &CmdArgs) const {
  CmdArgs.push_back("-c");
}

void gcc::Link::RenderExtraToolArgs(ArgStringList &CmdArgs) const {
  // The types are (hopefully) good enough.
}

void darwin::Assemble::ConstructJob(Compilation &C, const JobAction &JA,
                                    Job &Dest, const InputInfo &Output, 
                                    const InputInfoList &Inputs, 
                                    const ArgList &Args, 
                                    const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  assert(Inputs.size() == 1 && "Unexpected number of inputs.");
  const InputInfo &Input = Inputs[0];

  // Bit of a hack, this is only used for original inputs.
  if (Input.isFilename() &&
      strcmp(Input.getFilename(), Input.getBaseInput()) == 0 &&
      Args.hasArg(options::OPT_g_Group))
    CmdArgs.push_back("--gstabs");
  
  // Derived from asm spec.
  CmdArgs.push_back("-arch");
  CmdArgs.push_back(getToolChain().getArchName().c_str());

  CmdArgs.push_back("-force_cpusubtype_ALL");
  if ((Args.hasArg(options::OPT_mkernel) ||
       Args.hasArg(options::OPT_static) ||
       Args.hasArg(options::OPT_fapple_kext)) &&
      !Args.hasArg(options::OPT_dynamic))
      CmdArgs.push_back("-static");
  
  Args.AddAllArgValues(CmdArgs, options::OPT_Wa_COMMA,
                       options::OPT_Xassembler);

  assert(Output.isFilename() && "Unexpected lipo output.");
  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  if (Input.isPipe()) {
    CmdArgs.push_back("-");
  } else {
    assert(Input.isFilename() && "Invalid input.");
    CmdArgs.push_back(Input.getFilename());
  }

  // asm_final spec is empty.

  const char *Exec = 
    Args.MakeArgString(getToolChain().GetProgramPath(C, "as").c_str());
  Dest.addCommand(new Command(Exec, CmdArgs));
}

static const char *MakeFormattedString(const ArgList &Args,
                                       const llvm::format_object_base &Fmt) {
  std::string Str;
  llvm::raw_string_ostream(Str) << Fmt;
  return Args.MakeArgString(Str.c_str());
}

/// Helper routine for seeing if we should use dsymutil; this is a
/// gcc compatible hack, we should remove it and use the input
/// type information.
static bool isSourceSuffix(const char *Str) {
  // match: 'C', 'CPP', 'c', 'cc', 'cp', 'c++', 'cpp', 'cxx', 'm',
  // 'mm'.
  switch (strlen(Str)) {
  default:
    return false;
  case 1:
    return (memcmp(Str, "C", 1) == 0 ||
            memcmp(Str, "c", 1) == 0 ||
            memcmp(Str, "m", 1) == 0);
  case 2:
    return (memcmp(Str, "cc", 2) == 0 ||
            memcmp(Str, "cp", 2) == 0 ||
            memcmp(Str, "mm", 2) == 0);
  case 3:
    return (memcmp(Str, "CPP", 3) == 0 ||
            memcmp(Str, "c++", 3) == 0 ||
            memcmp(Str, "cpp", 3) == 0 ||
            memcmp(Str, "cxx", 3) == 0);
  }
}

static bool isMacosxVersionLT(unsigned (&A)[3], unsigned (&B)[3]) {
  for (unsigned i=0; i < 3; ++i) {
    if (A[i] > B[i]) return false;
    if (A[i] < B[i]) return true;
  }
  return false;
}

static bool isMacosxVersionLT(unsigned (&A)[3], 
                              unsigned V0, unsigned V1=0, unsigned V2=0) {
  unsigned B[3] = { V0, V1, V2 };
  return isMacosxVersionLT(A, B);
}

static bool isMacosxVersionGTE(unsigned(&A)[3], 
                              unsigned V0, unsigned V1=0, unsigned V2=0) {
  return !isMacosxVersionLT(A, V0, V1, V2);
}

const toolchains::Darwin_X86 &darwin::Link::getDarwinToolChain() const {
  return reinterpret_cast<const toolchains::Darwin_X86&>(getToolChain());
}

void darwin::Link::AddDarwinArch(const ArgList &Args, 
                                 ArgStringList &CmdArgs) const {
  // Derived from darwin_arch spec.
  CmdArgs.push_back("-arch");
  CmdArgs.push_back(getToolChain().getArchName().c_str());
}

void darwin::Link::AddDarwinSubArch(const ArgList &Args, 
                                    ArgStringList &CmdArgs) const {
  // Derived from darwin_subarch spec, not sure what the distinction
  // exists for but at least for this chain it is the same.
  AddDarwinArch(Args, CmdArgs);
}

void darwin::Link::AddLinkArgs(const ArgList &Args, 
                               ArgStringList &CmdArgs) const {
  const Driver &D = getToolChain().getHost().getDriver();

  // Derived from the "link" spec.
  Args.AddAllArgs(CmdArgs, options::OPT_static);
  if (!Args.hasArg(options::OPT_static))
    CmdArgs.push_back("-dynamic");
  if (Args.hasArg(options::OPT_fgnu_runtime)) {
    // FIXME: gcc replaces -lobjc in forward args with -lobjc-gnu
    // here. How do we wish to handle such things?
  }
    
  if (!Args.hasArg(options::OPT_dynamiclib)) {
    if (Args.hasArg(options::OPT_force__cpusubtype__ALL)) {
      AddDarwinArch(Args, CmdArgs);
      CmdArgs.push_back("-force_cpusubtype_ALL");
    } else
      AddDarwinSubArch(Args, CmdArgs);

    Args.AddLastArg(CmdArgs, options::OPT_bundle);
    Args.AddAllArgs(CmdArgs, options::OPT_bundle__loader);
    Args.AddAllArgs(CmdArgs, options::OPT_client__name);

    Arg *A;
    if ((A = Args.getLastArg(options::OPT_compatibility__version)) ||
        (A = Args.getLastArg(options::OPT_current__version)) ||
        (A = Args.getLastArg(options::OPT_install__name)))
      D.Diag(clang::diag::err_drv_argument_only_allowed_with)
        << A->getAsString(Args) << "-dynamiclib";

    Args.AddLastArg(CmdArgs, options::OPT_force__flat__namespace);
    Args.AddLastArg(CmdArgs, options::OPT_keep__private__externs);
    Args.AddLastArg(CmdArgs, options::OPT_private__bundle);
  } else {
    CmdArgs.push_back("-dylib");

    Arg *A;
    if ((A = Args.getLastArg(options::OPT_bundle)) ||
        (A = Args.getLastArg(options::OPT_bundle__loader)) ||
        (A = Args.getLastArg(options::OPT_client__name)) ||
        (A = Args.getLastArg(options::OPT_force__flat__namespace)) ||
        (A = Args.getLastArg(options::OPT_keep__private__externs)) ||
        (A = Args.getLastArg(options::OPT_private__bundle)))
      D.Diag(clang::diag::err_drv_argument_not_allowed_with)
        << A->getAsString(Args) << "-dynamiclib";
    
    Args.AddAllArgsTranslated(CmdArgs, options::OPT_compatibility__version,
                              "-dylib_compatibility_version");
    Args.AddAllArgsTranslated(CmdArgs, options::OPT_current__version,
                              "-dylib_current_version");

    if (Args.hasArg(options::OPT_force__cpusubtype__ALL)) {
      AddDarwinArch(Args, CmdArgs);
          // NOTE: We don't add -force_cpusubtype_ALL on this path. Ok.
    } else
      AddDarwinSubArch(Args, CmdArgs);

    Args.AddAllArgsTranslated(CmdArgs, options::OPT_install__name,
                              "-dylib_install_name");
  }

  Args.AddLastArg(CmdArgs, options::OPT_all__load);
  Args.AddAllArgs(CmdArgs, options::OPT_allowable__client);
  Args.AddLastArg(CmdArgs, options::OPT_bind__at__load);
  Args.AddLastArg(CmdArgs, options::OPT_dead__strip);
  Args.AddLastArg(CmdArgs, options::OPT_no__dead__strip__inits__and__terms);
  Args.AddAllArgs(CmdArgs, options::OPT_dylib__file);
  Args.AddLastArg(CmdArgs, options::OPT_dynamic);
  Args.AddAllArgs(CmdArgs, options::OPT_exported__symbols__list);
  Args.AddLastArg(CmdArgs, options::OPT_flat__namespace);
  Args.AddAllArgs(CmdArgs, options::OPT_headerpad__max__install__names);
  Args.AddAllArgs(CmdArgs, options::OPT_image__base);
  Args.AddAllArgs(CmdArgs, options::OPT_init);

  if (!Args.hasArg(options::OPT_mmacosx_version_min_EQ)) {
    if (!Args.hasArg(options::OPT_miphoneos_version_min_EQ)) {
        // FIXME: I don't understand what is going on here. This is
        // supposed to come from darwin_ld_minversion, but gcc doesn't
        // seem to be following that; it must be getting overridden
        // somewhere.
        CmdArgs.push_back("-macosx_version_min");
        CmdArgs.push_back(getDarwinToolChain().getMacosxVersionStr());
      }
  } else {
    // Adding all arguments doesn't make sense here but this is what
    // gcc does.
    Args.AddAllArgsTranslated(CmdArgs, options::OPT_mmacosx_version_min_EQ,
                              "-macosx_version_min");
  }

  Args.AddAllArgsTranslated(CmdArgs, options::OPT_miphoneos_version_min_EQ,
                            "-iphoneos_version_min");
  Args.AddLastArg(CmdArgs, options::OPT_nomultidefs);
  Args.AddLastArg(CmdArgs, options::OPT_multi__module);
  Args.AddLastArg(CmdArgs, options::OPT_single__module);
  Args.AddAllArgs(CmdArgs, options::OPT_multiply__defined);
  Args.AddAllArgs(CmdArgs, options::OPT_multiply__defined__unused);
  
  if (Args.hasArg(options::OPT_fpie))
    CmdArgs.push_back("-pie");

  Args.AddLastArg(CmdArgs, options::OPT_prebind);
  Args.AddLastArg(CmdArgs, options::OPT_noprebind);
  Args.AddLastArg(CmdArgs, options::OPT_nofixprebinding);
  Args.AddLastArg(CmdArgs, options::OPT_prebind__all__twolevel__modules);
  Args.AddLastArg(CmdArgs, options::OPT_read__only__relocs);
  Args.AddAllArgs(CmdArgs, options::OPT_sectcreate);
  Args.AddAllArgs(CmdArgs, options::OPT_sectorder);
  Args.AddAllArgs(CmdArgs, options::OPT_seg1addr);
  Args.AddAllArgs(CmdArgs, options::OPT_segprot);
  Args.AddAllArgs(CmdArgs, options::OPT_segaddr);
  Args.AddAllArgs(CmdArgs, options::OPT_segs__read__only__addr);
  Args.AddAllArgs(CmdArgs, options::OPT_segs__read__write__addr);
  Args.AddAllArgs(CmdArgs, options::OPT_seg__addr__table);
  Args.AddAllArgs(CmdArgs, options::OPT_seg__addr__table__filename);
  Args.AddAllArgs(CmdArgs, options::OPT_sub__library);
  Args.AddAllArgs(CmdArgs, options::OPT_sub__umbrella);
  Args.AddAllArgsTranslated(CmdArgs, options::OPT_isysroot, "-syslibroot");
  Args.AddLastArg(CmdArgs, options::OPT_twolevel__namespace);
  Args.AddLastArg(CmdArgs, options::OPT_twolevel__namespace__hints);
  Args.AddAllArgs(CmdArgs, options::OPT_umbrella);
  Args.AddAllArgs(CmdArgs, options::OPT_undefined);
  Args.AddAllArgs(CmdArgs, options::OPT_unexported__symbols__list);
  Args.AddAllArgs(CmdArgs, options::OPT_weak__reference__mismatches);

  if (!Args.hasArg(options::OPT_weak__reference__mismatches)) {
    CmdArgs.push_back("-weak_reference_mismatches");
    CmdArgs.push_back("non-weak");
  }

  Args.AddLastArg(CmdArgs, options::OPT_X_Flag);
  Args.AddAllArgs(CmdArgs, options::OPT_y);
  Args.AddLastArg(CmdArgs, options::OPT_w);
  Args.AddAllArgs(CmdArgs, options::OPT_pagezero__size);
  Args.AddAllArgs(CmdArgs, options::OPT_segs__read__);
  Args.AddLastArg(CmdArgs, options::OPT_seglinkedit);
  Args.AddLastArg(CmdArgs, options::OPT_noseglinkedit);
  Args.AddAllArgs(CmdArgs, options::OPT_sectalign);
  Args.AddAllArgs(CmdArgs, options::OPT_sectobjectsymbols);
  Args.AddAllArgs(CmdArgs, options::OPT_segcreate);
  Args.AddLastArg(CmdArgs, options::OPT_whyload);
  Args.AddLastArg(CmdArgs, options::OPT_whatsloaded);
  Args.AddAllArgs(CmdArgs, options::OPT_dylinker__install__name);
  Args.AddLastArg(CmdArgs, options::OPT_dylinker);
  Args.AddLastArg(CmdArgs, options::OPT_Mach);
}

void darwin::Link::ConstructJob(Compilation &C, const JobAction &JA,
                                Job &Dest, const InputInfo &Output, 
                                const InputInfoList &Inputs, 
                                const ArgList &Args, 
                                const char *LinkingOutput) const {
  assert(Output.getType() == types::TY_Image && "Invalid linker output type.");
  // The logic here is derived from gcc's behavior; most of which
  // comes from specs (starting with link_command). Consult gcc for
  // more information.

  // FIXME: The spec references -fdump= which seems to have
  // disappeared?

  ArgStringList CmdArgs;

  // I'm not sure why this particular decomposition exists in gcc, but
  // we follow suite for ease of comparison.
  AddLinkArgs(Args, CmdArgs);

  // FIXME: gcc has %{x} in here. How could this ever happen?  Cruft?
  Args.AddAllArgs(CmdArgs, options::OPT_d_Flag);
  Args.AddAllArgs(CmdArgs, options::OPT_s);
  Args.AddAllArgs(CmdArgs, options::OPT_t);
  Args.AddAllArgs(CmdArgs, options::OPT_Z_Flag);
  Args.AddAllArgs(CmdArgs, options::OPT_u_Group);
  Args.AddAllArgs(CmdArgs, options::OPT_A);
  Args.AddLastArg(CmdArgs, options::OPT_e);
  Args.AddAllArgs(CmdArgs, options::OPT_m_Separate);
  Args.AddAllArgs(CmdArgs, options::OPT_r);

  // FIXME: This is just being pedantically bug compatible, gcc
  // doesn't *mean* to forward this, it just does (yay for pattern
  // matching). It doesn't work, of course.
  Args.AddAllArgs(CmdArgs, options::OPT_object);

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  unsigned MacosxVersion[3];
  if (Arg *A = Args.getLastArg(options::OPT_mmacosx_version_min_EQ)) {
    bool HadExtra;
    if (!Driver::GetReleaseVersion(A->getValue(Args), MacosxVersion[0], 
                                   MacosxVersion[1], MacosxVersion[2],
                                   HadExtra) ||
        HadExtra) {
      const Driver &D = getToolChain().getHost().getDriver();
      D.Diag(clang::diag::err_drv_invalid_version_number)
        << A->getAsString(Args);
    }
  } else {
    getDarwinToolChain().getMacosxVersion(MacosxVersion);
  }
    
  if (!Args.hasArg(options::OPT_A) &&
      !Args.hasArg(options::OPT_nostdlib) &&
      !Args.hasArg(options::OPT_nostartfiles)) {
    // Derived from startfile spec.
    if (Args.hasArg(options::OPT_dynamiclib)) {
        // Derived from darwin_dylib1 spec.
      if (Args.hasArg(options::OPT_miphoneos_version_min_EQ) ||
          isMacosxVersionLT(MacosxVersion, 10, 5))
        CmdArgs.push_back("-ldylib1.o");
      else
        CmdArgs.push_back("-ldylib1.10.5.o");
    } else {
      if (Args.hasArg(options::OPT_bundle)) {
        if (!Args.hasArg(options::OPT_static))
          CmdArgs.push_back("-lbundle1.o");
      } else {
        if (Args.hasArg(options::OPT_pg)) {
          if (Args.hasArg(options::OPT_static) ||
              Args.hasArg(options::OPT_object) ||
              Args.hasArg(options::OPT_preload)) {
            CmdArgs.push_back("-lgcrt0.o");
          } else {
            CmdArgs.push_back("-lgcrt1.o");
                
            // darwin_crt2 spec is empty.
          } 
        } else {
          if (Args.hasArg(options::OPT_static) ||
              Args.hasArg(options::OPT_object) ||
              Args.hasArg(options::OPT_preload)) {
            CmdArgs.push_back("-lcrt0.o");
          } else {
            // Derived from darwin_crt1 spec.
            if (Args.hasArg(options::OPT_miphoneos_version_min_EQ) ||
                isMacosxVersionLT(MacosxVersion, 10, 5)) {
              CmdArgs.push_back("-lcrt1.o");
            } else {
              CmdArgs.push_back("-lcrt1.10.5.o");
              
              // darwin_crt2 spec is empty.
            }
          }
        }
      }
    }

    if (Args.hasArg(options::OPT_shared_libgcc) &&
        !Args.hasArg(options::OPT_miphoneos_version_min_EQ) &&
        isMacosxVersionLT(MacosxVersion, 10, 5)) {
      const char *Str = getToolChain().GetFilePath(C, "crt3.o").c_str();
      CmdArgs.push_back(Args.MakeArgString(Str));
    }
  }

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  
  if (Args.hasArg(options::OPT_fopenmp))
    // This is more complicated in gcc...
    CmdArgs.push_back("-lgomp");

  // FIXME: Derive these correctly.
  const char *TCDir = getDarwinToolChain().getToolChainDir().c_str();
  if (getToolChain().getArchName() == "x86_64") {
    CmdArgs.push_back(MakeFormattedString(Args,
                              llvm::format("-L/usr/lib/gcc/%s/x86_64", TCDir)));
    // Intentionally duplicated for (temporary) gcc bug compatibility.
    CmdArgs.push_back(MakeFormattedString(Args,
                              llvm::format("-L/usr/lib/gcc/%s/x86_64", TCDir)));
  }
  CmdArgs.push_back(MakeFormattedString(Args, 
                                        llvm::format("-L/usr/lib/%s", TCDir)));
  CmdArgs.push_back(MakeFormattedString(Args, 
                                     llvm::format("-L/usr/lib/gcc/%s", TCDir)));
  // Intentionally duplicated for (temporary) gcc bug compatibility.
  CmdArgs.push_back(MakeFormattedString(Args, 
                                     llvm::format("-L/usr/lib/gcc/%s", TCDir)));
  CmdArgs.push_back(MakeFormattedString(Args, 
                  llvm::format("-L/usr/lib/gcc/%s/../../../%s", TCDir, TCDir)));
  CmdArgs.push_back(MakeFormattedString(Args,
                            llvm::format("-L/usr/lib/gcc/%s/../../..", TCDir)));
  
  for (InputInfoList::const_iterator
         it = Inputs.begin(), ie = Inputs.end(); it != ie; ++it) {
    const InputInfo &II = *it;
    if (II.isFilename())
      CmdArgs.push_back(II.getFilename());
    else 
      II.getInputArg().renderAsInput(Args, CmdArgs);
  }

  if (LinkingOutput) {
    CmdArgs.push_back("-arch_multiple");
    CmdArgs.push_back("-final_output");
    CmdArgs.push_back(LinkingOutput);
  }

  if (Args.hasArg(options::OPT_fprofile_arcs) ||
      Args.hasArg(options::OPT_fprofile_generate) ||
      Args.hasArg(options::OPT_fcreate_profile) ||
      Args.hasArg(options::OPT_coverage))
    CmdArgs.push_back("-lgcov");
  
  if (Args.hasArg(options::OPT_fnested_functions))
    CmdArgs.push_back("-allow_stack_execute");
  
  if (!Args.hasArg(options::OPT_nostdlib) &&
      !Args.hasArg(options::OPT_nodefaultlibs)) {
    // link_ssp spec is empty.

    // Derived from libgcc spec.
    if (Args.hasArg(options::OPT_static)) {
      CmdArgs.push_back("-lgcc_static");
    } else if (Args.hasArg(options::OPT_static_libgcc)) {
      CmdArgs.push_back("-lgcc_eh");
      CmdArgs.push_back("-lgcc");
    } else if (Args.hasArg(options::OPT_miphoneos_version_min_EQ)) {
      // Derived from darwin_iphoneos_libgcc spec.
      CmdArgs.push_back("-lgcc_s.10.5");
      CmdArgs.push_back("-lgcc");
    } else if (Args.hasArg(options::OPT_shared_libgcc) ||
               Args.hasArg(options::OPT_fexceptions) ||
               Args.hasArg(options::OPT_fgnu_runtime)) {
      if (isMacosxVersionLT(MacosxVersion, 10, 5))
        CmdArgs.push_back("-lgcc_s.10.4");
      else
        CmdArgs.push_back("-lgcc_s.10.5");
      CmdArgs.push_back("-lgcc");
    } else {
      if (isMacosxVersionLT(MacosxVersion, 10, 5) &&
          isMacosxVersionGTE(MacosxVersion, 10, 3, 9))
        CmdArgs.push_back("-lgcc_s.10.4");
      if (isMacosxVersionGTE(MacosxVersion, 10, 5))
        CmdArgs.push_back("-lgcc_s.10.5");
      CmdArgs.push_back("-lgcc");
    }

    // Derived from lib spec.
    if (!Args.hasArg(options::OPT_static))
      CmdArgs.push_back("-lSystem");
  }

  if (!Args.hasArg(options::OPT_A) &&
      !Args.hasArg(options::OPT_nostdlib) &&
      !Args.hasArg(options::OPT_nostartfiles)) {
    // endfile_spec is empty.
  }

  Args.AddAllArgs(CmdArgs, options::OPT_T_Group);
  Args.AddAllArgs(CmdArgs, options::OPT_F);

  const char *Exec = 
    Args.MakeArgString(getToolChain().GetProgramPath(C, "collect2").c_str());
  Dest.addCommand(new Command(Exec, CmdArgs));  

  if (Args.getLastArg(options::OPT_g_Group) &&
      !Args.getLastArg(options::OPT_gstabs) &&
      !Args.getLastArg(options::OPT_g0)) {
    // FIXME: This is gross, but matches gcc. The test only considers
    // the suffix (not the -x type), and then only of the first
    // input. Awesome.
    const char *Suffix = strchr(Inputs[0].getBaseInput(), '.');
    if (Suffix && isSourceSuffix(Suffix + 1)) {
      const char *Exec = 
       Args.MakeArgString(getToolChain().GetProgramPath(C, "dsymutil").c_str());
      ArgStringList CmdArgs;
      CmdArgs.push_back(Output.getFilename());
      C.getJobs().addCommand(new Command(Exec, CmdArgs));
    }
  }
}

void darwin::Lipo::ConstructJob(Compilation &C, const JobAction &JA,
                                Job &Dest, const InputInfo &Output, 
                                const InputInfoList &Inputs, 
                                const ArgList &Args, 
                                const char *LinkingOutput) const {
  ArgStringList CmdArgs;

  CmdArgs.push_back("-create");
  assert(Output.isFilename() && "Unexpected lipo output.");

  CmdArgs.push_back("-output");
  CmdArgs.push_back(Output.getFilename());

  for (InputInfoList::const_iterator
         it = Inputs.begin(), ie = Inputs.end(); it != ie; ++it) {
    const InputInfo &II = *it;
    assert(II.isFilename() && "Unexpected lipo input.");
    CmdArgs.push_back(II.getFilename());
  }
  const char *Exec = 
    Args.MakeArgString(getToolChain().GetProgramPath(C, "lipo").c_str());
  Dest.addCommand(new Command(Exec, CmdArgs));
}
