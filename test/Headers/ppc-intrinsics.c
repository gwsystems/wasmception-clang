// NOTE: Assertions have been autogenerated by utils/update_cc_test_checks.py
// REQUIRES: powerpc-registered-target
// expected-no-diagnostics

// Don't include mm_malloc.h, it's system specific.
#define _MM_MALLOC_H_INCLUDED

// RUN: %clang -S -emit-llvm -DNO_WARN_X86_INTRINSICS -mcpu=pwr8 -target powerpc64-unknown-linux-gnu %s -Xclang -verify
// RUN: %clang -S -emit-llvm -DNO_WARN_X86_INTRINSICS -mcpu=pwr8 -target powerpc64-unknown-linux-gnu %s -Xclang -verify -x c++

// Since mm_malloc.h references system native stdlib.h, doing cross-compile
// testing may cause unexpected problems. This would affect xmmintrin.h and
// other following intrinsics headers. If there's need to test them using
// cross-compile, please add -ffreestanding to compiler options, like
// test/CodeGen/ppc-xmmintrin.c.

// RUN: not %clang -S -emit-llvm -target powerpc64-unknown-linux-gnu -mcpu=pwr8 %s -o /dev/null 2>&1 | FileCheck %s -check-prefix=CHECK-ERROR

#include <mmintrin.h>

// Altivec must be enabled.
#include <xmmintrin.h>
#include <emmintrin.h>
#include <pmmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

// CHECK-ERROR: {{[0-9]+}}:{{[0-9]+}}: error: "Please read comment above. Use -DNO_WARN_X86_INTRINSICS to disable this error."
