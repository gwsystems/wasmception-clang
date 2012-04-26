// These tests are generated by running utils/generate_arch_predefine_tests.sh
// to observe GCC's behavior (or some other system compiler's behavior).
//
// Begin X86/GCC/Linux tests ----------------
//
// RUN: %clang -march=i386 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_I386_M32
// CHECK_I386_M32: #define __i386 1
// CHECK_I386_M32: #define __i386__ 1
// CHECK_I386_M32: #define __tune_i386__ 1
// CHECK_I386_M32: #define i386 1
// RUN: %clang -march=i386 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_I386_M64
// CHECK_I386_M64: error:
//
// RUN: %clang -march=i486 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_I486_M32
// CHECK_I486_M32: #define __i386 1
// CHECK_I486_M32: #define __i386__ 1
// CHECK_I486_M32: #define __i486 1
// CHECK_I486_M32: #define __i486__ 1
// CHECK_I486_M32: #define __tune_i486__ 1
// CHECK_I486_M32: #define i386 1
// RUN: %clang -march=i486 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_I486_M64
// CHECK_I486_M64: error:
//
// RUN: %clang -march=i586 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_I586_M32
// CHECK_I586_M32: #define __i386 1
// CHECK_I586_M32: #define __i386__ 1
// CHECK_I586_M32: #define __i586 1
// CHECK_I586_M32: #define __i586__ 1
// CHECK_I586_M32: #define __pentium 1
// CHECK_I586_M32: #define __pentium__ 1
// CHECK_I586_M32: #define __tune_i586__ 1
// CHECK_I586_M32: #define __tune_pentium__ 1
// CHECK_I586_M32: #define i386 1
// RUN: %clang -march=i586 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_I586_M64
// CHECK_I586_M64: error:
//
// RUN: %clang -march=pentium -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM_M32
// CHECK_PENTIUM_M32: #define __i386 1
// CHECK_PENTIUM_M32: #define __i386__ 1
// CHECK_PENTIUM_M32: #define __i586 1
// CHECK_PENTIUM_M32: #define __i586__ 1
// CHECK_PENTIUM_M32: #define __pentium 1
// CHECK_PENTIUM_M32: #define __pentium__ 1
// CHECK_PENTIUM_M32: #define __tune_i586__ 1
// CHECK_PENTIUM_M32: #define __tune_pentium__ 1
// CHECK_PENTIUM_M32: #define i386 1
// RUN: %clang -march=pentium -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM_M64
// CHECK_PENTIUM_M64: error:
//
// RUN: %clang -march=pentium-mmx -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM_MMX_M32
// CHECK_PENTIUM_MMX_M32: #define __MMX__ 1
// CHECK_PENTIUM_MMX_M32: #define __i386 1
// CHECK_PENTIUM_MMX_M32: #define __i386__ 1
// CHECK_PENTIUM_MMX_M32: #define __i586 1
// CHECK_PENTIUM_MMX_M32: #define __i586__ 1
// CHECK_PENTIUM_MMX_M32: #define __pentium 1
// CHECK_PENTIUM_MMX_M32: #define __pentium__ 1
// CHECK_PENTIUM_MMX_M32: #define __pentium_mmx__ 1
// CHECK_PENTIUM_MMX_M32: #define __tune_i586__ 1
// CHECK_PENTIUM_MMX_M32: #define __tune_pentium__ 1
// CHECK_PENTIUM_MMX_M32: #define __tune_pentium_mmx__ 1
// CHECK_PENTIUM_MMX_M32: #define i386 1
// RUN: %clang -march=pentium-mmx -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM_MMX_M64
// CHECK_PENTIUM_MMX_M64: error:
//
// RUN: %clang -march=winchip-c6 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_WINCHIP_C6_M32
// CHECK_WINCHIP_C6_M32: #define __MMX__ 1
// CHECK_WINCHIP_C6_M32: #define __i386 1
// CHECK_WINCHIP_C6_M32: #define __i386__ 1
// CHECK_WINCHIP_C6_M32: #define __i486 1
// CHECK_WINCHIP_C6_M32: #define __i486__ 1
// CHECK_WINCHIP_C6_M32: #define __tune_i486__ 1
// CHECK_WINCHIP_C6_M32: #define i386 1
// RUN: %clang -march=winchip-c6 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_WINCHIP_C6_M64
// CHECK_WINCHIP_C6_M64: error:
//
// RUN: %clang -march=winchip2 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_WINCHIP2_M32
// CHECK_WINCHIP2_M32: #define __3dNOW__ 1
// CHECK_WINCHIP2_M32: #define __MMX__ 1
// CHECK_WINCHIP2_M32: #define __i386 1
// CHECK_WINCHIP2_M32: #define __i386__ 1
// CHECK_WINCHIP2_M32: #define __i486 1
// CHECK_WINCHIP2_M32: #define __i486__ 1
// CHECK_WINCHIP2_M32: #define __tune_i486__ 1
// CHECK_WINCHIP2_M32: #define i386 1
// RUN: %clang -march=winchip2 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_WINCHIP2_M64
// CHECK_WINCHIP2_M64: error:
//
// RUN: %clang -march=c3 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_C3_M32
// CHECK_C3_M32: #define __3dNOW__ 1
// CHECK_C3_M32: #define __MMX__ 1
// CHECK_C3_M32: #define __i386 1
// CHECK_C3_M32: #define __i386__ 1
// CHECK_C3_M32: #define __i486 1
// CHECK_C3_M32: #define __i486__ 1
// CHECK_C3_M32: #define __tune_i486__ 1
// CHECK_C3_M32: #define i386 1
// RUN: %clang -march=c3 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_C3_M64
// CHECK_C3_M64: error:
//
// RUN: %clang -march=c3-2 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_C3_2_M32
// CHECK_C3_2_M32: #define __MMX__ 1
// CHECK_C3_2_M32: #define __SSE__ 1
// CHECK_C3_2_M32: #define __i386 1
// CHECK_C3_2_M32: #define __i386__ 1
// CHECK_C3_2_M32: #define __i686 1
// CHECK_C3_2_M32: #define __i686__ 1
// CHECK_C3_2_M32: #define __pentiumpro 1
// CHECK_C3_2_M32: #define __pentiumpro__ 1
// CHECK_C3_2_M32: #define __tune_i686__ 1
// CHECK_C3_2_M32: #define __tune_pentium2__ 1
// CHECK_C3_2_M32: #define __tune_pentiumpro__ 1
// CHECK_C3_2_M32: #define i386 1
// RUN: %clang -march=c3-2 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_C3_2_M64
// CHECK_C3_2_M64: error:
//
// RUN: %clang -march=i686 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_I686_M32
// CHECK_I686_M32: #define __i386 1
// CHECK_I686_M32: #define __i386__ 1
// CHECK_I686_M32: #define __i686 1
// CHECK_I686_M32: #define __i686__ 1
// CHECK_I686_M32: #define __pentiumpro 1
// CHECK_I686_M32: #define __pentiumpro__ 1
// CHECK_I686_M32: #define i386 1
// RUN: %clang -march=i686 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_I686_M64
// CHECK_I686_M64: error:
//
// RUN: %clang -march=pentiumpro -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUMPRO_M32
// CHECK_PENTIUMPRO_M32: #define __i386 1
// CHECK_PENTIUMPRO_M32: #define __i386__ 1
// CHECK_PENTIUMPRO_M32: #define __i686 1
// CHECK_PENTIUMPRO_M32: #define __i686__ 1
// CHECK_PENTIUMPRO_M32: #define __pentiumpro 1
// CHECK_PENTIUMPRO_M32: #define __pentiumpro__ 1
// CHECK_PENTIUMPRO_M32: #define __tune_i686__ 1
// CHECK_PENTIUMPRO_M32: #define __tune_pentiumpro__ 1
// CHECK_PENTIUMPRO_M32: #define i386 1
// RUN: %clang -march=pentiumpro -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUMPRO_M64
// CHECK_PENTIUMPRO_M64: error:
//
// RUN: %clang -march=pentium2 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM2_M32
// CHECK_PENTIUM2_M32: #define __MMX__ 1
// CHECK_PENTIUM2_M32: #define __i386 1
// CHECK_PENTIUM2_M32: #define __i386__ 1
// CHECK_PENTIUM2_M32: #define __i686 1
// CHECK_PENTIUM2_M32: #define __i686__ 1
// CHECK_PENTIUM2_M32: #define __pentiumpro 1
// CHECK_PENTIUM2_M32: #define __pentiumpro__ 1
// CHECK_PENTIUM2_M32: #define __tune_i686__ 1
// CHECK_PENTIUM2_M32: #define __tune_pentium2__ 1
// CHECK_PENTIUM2_M32: #define __tune_pentiumpro__ 1
// CHECK_PENTIUM2_M32: #define i386 1
// RUN: %clang -march=pentium2 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM2_M64
// CHECK_PENTIUM2_M64: error:
//
// RUN: %clang -march=pentium3 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM3_M32
// CHECK_PENTIUM3_M32: #define __MMX__ 1
// CHECK_PENTIUM3_M32: #define __SSE__ 1
// CHECK_PENTIUM3_M32: #define __i386 1
// CHECK_PENTIUM3_M32: #define __i386__ 1
// CHECK_PENTIUM3_M32: #define __i686 1
// CHECK_PENTIUM3_M32: #define __i686__ 1
// CHECK_PENTIUM3_M32: #define __pentiumpro 1
// CHECK_PENTIUM3_M32: #define __pentiumpro__ 1
// CHECK_PENTIUM3_M32: #define __tune_i686__ 1
// CHECK_PENTIUM3_M32: #define __tune_pentium2__ 1
// CHECK_PENTIUM3_M32: #define __tune_pentium3__ 1
// CHECK_PENTIUM3_M32: #define __tune_pentiumpro__ 1
// CHECK_PENTIUM3_M32: #define i386 1
// RUN: %clang -march=pentium3 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM3_M64
// CHECK_PENTIUM3_M64: error:
//
// RUN: %clang -march=pentium3m -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM3M_M32
// CHECK_PENTIUM3M_M32: #define __MMX__ 1
// CHECK_PENTIUM3M_M32: #define __SSE__ 1
// CHECK_PENTIUM3M_M32: #define __i386 1
// CHECK_PENTIUM3M_M32: #define __i386__ 1
// CHECK_PENTIUM3M_M32: #define __i686 1
// CHECK_PENTIUM3M_M32: #define __i686__ 1
// CHECK_PENTIUM3M_M32: #define __pentiumpro 1
// CHECK_PENTIUM3M_M32: #define __pentiumpro__ 1
// CHECK_PENTIUM3M_M32: #define __tune_i686__ 1
// CHECK_PENTIUM3M_M32: #define __tune_pentiumpro__ 1
// CHECK_PENTIUM3M_M32: #define i386 1
// RUN: %clang -march=pentium3m -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM3M_M64
// CHECK_PENTIUM3M_M64: error:
//
// RUN: %clang -march=pentium-m -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM_M_M32
// CHECK_PENTIUM_M_M32: #define __MMX__ 1
// CHECK_PENTIUM_M_M32: #define __SSE2__ 1
// CHECK_PENTIUM_M_M32: #define __SSE__ 1
// CHECK_PENTIUM_M_M32: #define __i386 1
// CHECK_PENTIUM_M_M32: #define __i386__ 1
// CHECK_PENTIUM_M_M32: #define __i686 1
// CHECK_PENTIUM_M_M32: #define __i686__ 1
// CHECK_PENTIUM_M_M32: #define __pentiumpro 1
// CHECK_PENTIUM_M_M32: #define __pentiumpro__ 1
// CHECK_PENTIUM_M_M32: #define __tune_i686__ 1
// CHECK_PENTIUM_M_M32: #define __tune_pentiumpro__ 1
// CHECK_PENTIUM_M_M32: #define i386 1
// RUN: %clang -march=pentium-m -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM_M_M64
// CHECK_PENTIUM_M_M64: error:
//
// RUN: %clang -march=pentium4 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM4_M32
// CHECK_PENTIUM4_M32: #define __MMX__ 1
// CHECK_PENTIUM4_M32: #define __SSE2__ 1
// CHECK_PENTIUM4_M32: #define __SSE__ 1
// CHECK_PENTIUM4_M32: #define __i386 1
// CHECK_PENTIUM4_M32: #define __i386__ 1
// CHECK_PENTIUM4_M32: #define __pentium4 1
// CHECK_PENTIUM4_M32: #define __pentium4__ 1
// CHECK_PENTIUM4_M32: #define __tune_pentium4__ 1
// CHECK_PENTIUM4_M32: #define i386 1
// RUN: %clang -march=pentium4 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM4_M64
// CHECK_PENTIUM4_M64: error:
//
// RUN: %clang -march=pentium4m -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM4M_M32
// CHECK_PENTIUM4M_M32: #define __MMX__ 1
// CHECK_PENTIUM4M_M32: #define __SSE2__ 1
// CHECK_PENTIUM4M_M32: #define __SSE__ 1
// CHECK_PENTIUM4M_M32: #define __i386 1
// CHECK_PENTIUM4M_M32: #define __i386__ 1
// CHECK_PENTIUM4M_M32: #define __pentium4 1
// CHECK_PENTIUM4M_M32: #define __pentium4__ 1
// CHECK_PENTIUM4M_M32: #define __tune_pentium4__ 1
// CHECK_PENTIUM4M_M32: #define i386 1
// RUN: %clang -march=pentium4m -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PENTIUM4M_M64
// CHECK_PENTIUM4M_M64: error:
//
// RUN: %clang -march=prescott -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PRESCOTT_M32
// CHECK_PRESCOTT_M32: #define __MMX__ 1
// CHECK_PRESCOTT_M32: #define __SSE2__ 1
// CHECK_PRESCOTT_M32: #define __SSE3__ 1
// CHECK_PRESCOTT_M32: #define __SSE__ 1
// CHECK_PRESCOTT_M32: #define __i386 1
// CHECK_PRESCOTT_M32: #define __i386__ 1
// CHECK_PRESCOTT_M32: #define __nocona 1
// CHECK_PRESCOTT_M32: #define __nocona__ 1
// CHECK_PRESCOTT_M32: #define __tune_nocona__ 1
// CHECK_PRESCOTT_M32: #define i386 1
// RUN: %clang -march=prescott -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_PRESCOTT_M64
// CHECK_PRESCOTT_M64: error:
//
// RUN: %clang -march=nocona -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_NOCONA_M32
// CHECK_NOCONA_M32: #define __MMX__ 1
// CHECK_NOCONA_M32: #define __SSE2__ 1
// CHECK_NOCONA_M32: #define __SSE3__ 1
// CHECK_NOCONA_M32: #define __SSE__ 1
// CHECK_NOCONA_M32: #define __i386 1
// CHECK_NOCONA_M32: #define __i386__ 1
// CHECK_NOCONA_M32: #define __nocona 1
// CHECK_NOCONA_M32: #define __nocona__ 1
// CHECK_NOCONA_M32: #define __tune_nocona__ 1
// CHECK_NOCONA_M32: #define i386 1
// RUN: %clang -march=nocona -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_NOCONA_M64
// CHECK_NOCONA_M64: #define __MMX__ 1
// CHECK_NOCONA_M64: #define __SSE2_MATH__ 1
// CHECK_NOCONA_M64: #define __SSE2__ 1
// CHECK_NOCONA_M64: #define __SSE3__ 1
// CHECK_NOCONA_M64: #define __SSE_MATH__ 1
// CHECK_NOCONA_M64: #define __SSE__ 1
// CHECK_NOCONA_M64: #define __amd64 1
// CHECK_NOCONA_M64: #define __amd64__ 1
// CHECK_NOCONA_M64: #define __nocona 1
// CHECK_NOCONA_M64: #define __nocona__ 1
// CHECK_NOCONA_M64: #define __tune_nocona__ 1
// CHECK_NOCONA_M64: #define __x86_64 1
// CHECK_NOCONA_M64: #define __x86_64__ 1
//
// RUN: %clang -march=core2 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_CORE2_M32
// CHECK_CORE2_M32: #define __MMX__ 1
// CHECK_CORE2_M32: #define __SSE2__ 1
// CHECK_CORE2_M32: #define __SSE3__ 1
// CHECK_CORE2_M32: #define __SSE__ 1
// CHECK_CORE2_M32: #define __SSSE3__ 1
// CHECK_CORE2_M32: #define __core2 1
// CHECK_CORE2_M32: #define __core2__ 1
// CHECK_CORE2_M32: #define __i386 1
// CHECK_CORE2_M32: #define __i386__ 1
// CHECK_CORE2_M32: #define __tune_core2__ 1
// CHECK_CORE2_M32: #define i386 1
// RUN: %clang -march=core2 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_CORE2_M64
// CHECK_CORE2_M64: #define __MMX__ 1
// CHECK_CORE2_M64: #define __SSE2_MATH__ 1
// CHECK_CORE2_M64: #define __SSE2__ 1
// CHECK_CORE2_M64: #define __SSE3__ 1
// CHECK_CORE2_M64: #define __SSE_MATH__ 1
// CHECK_CORE2_M64: #define __SSE__ 1
// CHECK_CORE2_M64: #define __SSSE3__ 1
// CHECK_CORE2_M64: #define __amd64 1
// CHECK_CORE2_M64: #define __amd64__ 1
// CHECK_CORE2_M64: #define __core2 1
// CHECK_CORE2_M64: #define __core2__ 1
// CHECK_CORE2_M64: #define __tune_core2__ 1
// CHECK_CORE2_M64: #define __x86_64 1
// CHECK_CORE2_M64: #define __x86_64__ 1
//
// RUN: %clang -march=corei7 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_COREI7_M32
// CHECK_COREI7_M32: #define __MMX__ 1
// CHECK_COREI7_M32: #define __SSE2__ 1
// CHECK_COREI7_M32: #define __SSE3__ 1
// CHECK_COREI7_M32: #define __SSE4_1__ 1
// CHECK_COREI7_M32: #define __SSE4_2__ 1
// CHECK_COREI7_M32: #define __SSE__ 1
// CHECK_COREI7_M32: #define __SSSE3__ 1
// CHECK_COREI7_M32: #define __corei7 1
// CHECK_COREI7_M32: #define __corei7__ 1
// CHECK_COREI7_M32: #define __i386 1
// CHECK_COREI7_M32: #define __i386__ 1
// CHECK_COREI7_M32: #define __tune_corei7__ 1
// CHECK_COREI7_M32: #define i386 1
// RUN: %clang -march=corei7 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_COREI7_M64
// CHECK_COREI7_M64: #define __MMX__ 1
// CHECK_COREI7_M64: #define __SSE2_MATH__ 1
// CHECK_COREI7_M64: #define __SSE2__ 1
// CHECK_COREI7_M64: #define __SSE3__ 1
// CHECK_COREI7_M64: #define __SSE4_1__ 1
// CHECK_COREI7_M64: #define __SSE4_2__ 1
// CHECK_COREI7_M64: #define __SSE_MATH__ 1
// CHECK_COREI7_M64: #define __SSE__ 1
// CHECK_COREI7_M64: #define __SSSE3__ 1
// CHECK_COREI7_M64: #define __amd64 1
// CHECK_COREI7_M64: #define __amd64__ 1
// CHECK_COREI7_M64: #define __corei7 1
// CHECK_COREI7_M64: #define __corei7__ 1
// CHECK_COREI7_M64: #define __tune_corei7__ 1
// CHECK_COREI7_M64: #define __x86_64 1
// CHECK_COREI7_M64: #define __x86_64__ 1
//
// RUN: %clang -march=corei7-avx -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_COREI7_AVX_M32
// CHECK_COREI7_AVX_M32: #define __AES__ 1
// CHECK_COREI7_AVX_M32: #define __AVX__ 1
// CHECK_COREI7_AVX_M32: #define __MMX__ 1
// CHECK_COREI7_AVX_M32: #define __SSE2__ 1
// CHECK_COREI7_AVX_M32: #define __SSE3__ 1
// CHECK_COREI7_AVX_M32: #define __SSE4_1__ 1
// CHECK_COREI7_AVX_M32: #define __SSE4_2__ 1
// CHECK_COREI7_AVX_M32: #define __SSE__ 1
// CHECK_COREI7_AVX_M32: #define __SSSE3__ 1
// CHECK_COREI7_AVX_M32: #define __corei7 1
// CHECK_COREI7_AVX_M32: #define __corei7__ 1
// CHECK_COREI7_AVX_M32: #define __i386 1
// CHECK_COREI7_AVX_M32: #define __i386__ 1
// CHECK_COREI7_AVX_M32: #define __tune_corei7__ 1
// CHECK_COREI7_AVX_M32: #define i386 1
// RUN: %clang -march=corei7-avx -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_COREI7_AVX_M64
// CHECK_COREI7_AVX_M64: #define __AES__ 1
// CHECK_COREI7_AVX_M64: #define __AVX__ 1
// CHECK_COREI7_AVX_M64: #define __MMX__ 1
// CHECK_COREI7_AVX_M64: #define __SSE2_MATH__ 1
// CHECK_COREI7_AVX_M64: #define __SSE2__ 1
// CHECK_COREI7_AVX_M64: #define __SSE3__ 1
// CHECK_COREI7_AVX_M64: #define __SSE4_1__ 1
// CHECK_COREI7_AVX_M64: #define __SSE4_2__ 1
// CHECK_COREI7_AVX_M64: #define __SSE_MATH__ 1
// CHECK_COREI7_AVX_M64: #define __SSE__ 1
// CHECK_COREI7_AVX_M64: #define __SSSE3__ 1
// CHECK_COREI7_AVX_M64: #define __amd64 1
// CHECK_COREI7_AVX_M64: #define __amd64__ 1
// CHECK_COREI7_AVX_M64: #define __corei7 1
// CHECK_COREI7_AVX_M64: #define __corei7__ 1
// CHECK_COREI7_AVX_M64: #define __tune_corei7__ 1
// CHECK_COREI7_AVX_M64: #define __x86_64 1
// CHECK_COREI7_AVX_M64: #define __x86_64__ 1
//
// RUN: %clang -march=core-avx-i -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_CORE_AVX_I_M32
// CHECK_CORE_AVX_I_M32: #define __AES__ 1
// CHECK_CORE_AVX_I_M32: #define __AVX__ 1
// CHECK_CORE_AVX_I_M32: #define __MMX__ 1
// CHECK_CORE_AVX_I_M32: #define __SSE2__ 1
// CHECK_CORE_AVX_I_M32: #define __SSE3__ 1
// CHECK_CORE_AVX_I_M32: #define __SSE4_1__ 1
// CHECK_CORE_AVX_I_M32: #define __SSE4_2__ 1
// CHECK_CORE_AVX_I_M32: #define __SSE__ 1
// CHECK_CORE_AVX_I_M32: #define __SSSE3__ 1
// CHECK_CORE_AVX_I_M32: #define __corei7 1
// CHECK_CORE_AVX_I_M32: #define __corei7__ 1
// CHECK_CORE_AVX_I_M32: #define __i386 1
// CHECK_CORE_AVX_I_M32: #define __i386__ 1
// CHECK_CORE_AVX_I_M32: #define __tune_corei7__ 1
// CHECK_CORE_AVX_I_M32: #define i386 1
// RUN: %clang -march=core-avx-i -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_CORE_AVX_I_M64
// CHECK_CORE_AVX_I_M64: #define __AES__ 1
// CHECK_CORE_AVX_I_M64: #define __AVX__ 1
// CHECK_CORE_AVX_I_M64: #define __MMX__ 1
// CHECK_CORE_AVX_I_M64: #define __SSE2_MATH__ 1
// CHECK_CORE_AVX_I_M64: #define __SSE2__ 1
// CHECK_CORE_AVX_I_M64: #define __SSE3__ 1
// CHECK_CORE_AVX_I_M64: #define __SSE4_1__ 1
// CHECK_CORE_AVX_I_M64: #define __SSE4_2__ 1
// CHECK_CORE_AVX_I_M64: #define __SSE_MATH__ 1
// CHECK_CORE_AVX_I_M64: #define __SSE__ 1
// CHECK_CORE_AVX_I_M64: #define __SSSE3__ 1
// CHECK_CORE_AVX_I_M64: #define __amd64 1
// CHECK_CORE_AVX_I_M64: #define __amd64__ 1
// CHECK_CORE_AVX_I_M64: #define __corei7 1
// CHECK_CORE_AVX_I_M64: #define __corei7__ 1
// CHECK_CORE_AVX_I_M64: #define __tune_corei7__ 1
// CHECK_CORE_AVX_I_M64: #define __x86_64 1
// CHECK_CORE_AVX_I_M64: #define __x86_64__ 1
//
// RUN: %clang -march=atom -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATOM_M32
// CHECK_ATOM_M32: #define __MMX__ 1
// CHECK_ATOM_M32: #define __SSE2__ 1
// CHECK_ATOM_M32: #define __SSE3__ 1
// CHECK_ATOM_M32: #define __SSE__ 1
// CHECK_ATOM_M32: #define __SSSE3__ 1
// CHECK_ATOM_M32: #define __atom 1
// CHECK_ATOM_M32: #define __atom__ 1
// CHECK_ATOM_M32: #define __i386 1
// CHECK_ATOM_M32: #define __i386__ 1
// CHECK_ATOM_M32: #define __tune_atom__ 1
// CHECK_ATOM_M32: #define i386 1
// RUN: %clang -march=atom -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATOM_M64
// CHECK_ATOM_M64: #define __MMX__ 1
// CHECK_ATOM_M64: #define __SSE2_MATH__ 1
// CHECK_ATOM_M64: #define __SSE2__ 1
// CHECK_ATOM_M64: #define __SSE3__ 1
// CHECK_ATOM_M64: #define __SSE_MATH__ 1
// CHECK_ATOM_M64: #define __SSE__ 1
// CHECK_ATOM_M64: #define __SSSE3__ 1
// CHECK_ATOM_M64: #define __amd64 1
// CHECK_ATOM_M64: #define __amd64__ 1
// CHECK_ATOM_M64: #define __atom 1
// CHECK_ATOM_M64: #define __atom__ 1
// CHECK_ATOM_M64: #define __tune_atom__ 1
// CHECK_ATOM_M64: #define __x86_64 1
// CHECK_ATOM_M64: #define __x86_64__ 1
//
// RUN: %clang -march=geode -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_GEODE_M32
// CHECK_GEODE_M32: #define __3dNOW_A__ 1
// CHECK_GEODE_M32: #define __3dNOW__ 1
// CHECK_GEODE_M32: #define __MMX__ 1
// CHECK_GEODE_M32: #define __geode 1
// CHECK_GEODE_M32: #define __geode__ 1
// CHECK_GEODE_M32: #define __i386 1
// CHECK_GEODE_M32: #define __i386__ 1
// CHECK_GEODE_M32: #define __tune_geode__ 1
// CHECK_GEODE_M32: #define i386 1
// RUN: %clang -march=geode -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_GEODE_M64
// CHECK_GEODE_M64: error:
//
// RUN: %clang -march=k6 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K6_M32
// CHECK_K6_M32: #define __MMX__ 1
// CHECK_K6_M32: #define __i386 1
// CHECK_K6_M32: #define __i386__ 1
// CHECK_K6_M32: #define __k6 1
// CHECK_K6_M32: #define __k6__ 1
// CHECK_K6_M32: #define __tune_k6__ 1
// CHECK_K6_M32: #define i386 1
// RUN: %clang -march=k6 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K6_M64
// CHECK_K6_M64: error:
//
// RUN: %clang -march=k6-2 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K6_2_M32
// CHECK_K6_2_M32: #define __3dNOW__ 1
// CHECK_K6_2_M32: #define __MMX__ 1
// CHECK_K6_2_M32: #define __i386 1
// CHECK_K6_2_M32: #define __i386__ 1
// CHECK_K6_2_M32: #define __k6 1
// CHECK_K6_2_M32: #define __k6_2__ 1
// CHECK_K6_2_M32: #define __k6__ 1
// CHECK_K6_2_M32: #define __tune_k6_2__ 1
// CHECK_K6_2_M32: #define __tune_k6__ 1
// CHECK_K6_2_M32: #define i386 1
// RUN: %clang -march=k6-2 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K6_2_M64
// CHECK_K6_2_M64: error:
//
// RUN: %clang -march=k6-3 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K6_3_M32
// CHECK_K6_3_M32: #define __3dNOW__ 1
// CHECK_K6_3_M32: #define __MMX__ 1
// CHECK_K6_3_M32: #define __i386 1
// CHECK_K6_3_M32: #define __i386__ 1
// CHECK_K6_3_M32: #define __k6 1
// CHECK_K6_3_M32: #define __k6_3__ 1
// CHECK_K6_3_M32: #define __k6__ 1
// CHECK_K6_3_M32: #define __tune_k6_3__ 1
// CHECK_K6_3_M32: #define __tune_k6__ 1
// CHECK_K6_3_M32: #define i386 1
// RUN: %clang -march=k6-3 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K6_3_M64
// CHECK_K6_3_M64: error:
//
// RUN: %clang -march=athlon -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_M32
// CHECK_ATHLON_M32: #define __3dNOW_A__ 1
// CHECK_ATHLON_M32: #define __3dNOW__ 1
// CHECK_ATHLON_M32: #define __MMX__ 1
// CHECK_ATHLON_M32: #define __athlon 1
// CHECK_ATHLON_M32: #define __athlon__ 1
// CHECK_ATHLON_M32: #define __i386 1
// CHECK_ATHLON_M32: #define __i386__ 1
// CHECK_ATHLON_M32: #define __tune_athlon__ 1
// CHECK_ATHLON_M32: #define i386 1
// RUN: %clang -march=athlon -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_M64
// CHECK_ATHLON_M64: error:
//
// RUN: %clang -march=athlon-tbird -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_TBIRD_M32
// CHECK_ATHLON_TBIRD_M32: #define __3dNOW_A__ 1
// CHECK_ATHLON_TBIRD_M32: #define __3dNOW__ 1
// CHECK_ATHLON_TBIRD_M32: #define __MMX__ 1
// CHECK_ATHLON_TBIRD_M32: #define __athlon 1
// CHECK_ATHLON_TBIRD_M32: #define __athlon__ 1
// CHECK_ATHLON_TBIRD_M32: #define __i386 1
// CHECK_ATHLON_TBIRD_M32: #define __i386__ 1
// CHECK_ATHLON_TBIRD_M32: #define __tune_athlon__ 1
// CHECK_ATHLON_TBIRD_M32: #define i386 1
// RUN: %clang -march=athlon-tbird -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_TBIRD_M64
// CHECK_ATHLON_TBIRD_M64: error:
//
// RUN: %clang -march=athlon-4 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_4_M32
// CHECK_ATHLON_4_M32: #define __3dNOW_A__ 1
// CHECK_ATHLON_4_M32: #define __3dNOW__ 1
// CHECK_ATHLON_4_M32: #define __MMX__ 1
// CHECK_ATHLON_4_M32: #define __SSE__ 1
// CHECK_ATHLON_4_M32: #define __athlon 1
// CHECK_ATHLON_4_M32: #define __athlon__ 1
// CHECK_ATHLON_4_M32: #define __athlon_sse__ 1
// CHECK_ATHLON_4_M32: #define __i386 1
// CHECK_ATHLON_4_M32: #define __i386__ 1
// CHECK_ATHLON_4_M32: #define __tune_athlon__ 1
// CHECK_ATHLON_4_M32: #define __tune_athlon_sse__ 1
// CHECK_ATHLON_4_M32: #define i386 1
// RUN: %clang -march=athlon-4 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_4_M64
// CHECK_ATHLON_4_M64: error:
//
// RUN: %clang -march=athlon-xp -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_XP_M32
// CHECK_ATHLON_XP_M32: #define __3dNOW_A__ 1
// CHECK_ATHLON_XP_M32: #define __3dNOW__ 1
// CHECK_ATHLON_XP_M32: #define __MMX__ 1
// CHECK_ATHLON_XP_M32: #define __SSE__ 1
// CHECK_ATHLON_XP_M32: #define __athlon 1
// CHECK_ATHLON_XP_M32: #define __athlon__ 1
// CHECK_ATHLON_XP_M32: #define __athlon_sse__ 1
// CHECK_ATHLON_XP_M32: #define __i386 1
// CHECK_ATHLON_XP_M32: #define __i386__ 1
// CHECK_ATHLON_XP_M32: #define __tune_athlon__ 1
// CHECK_ATHLON_XP_M32: #define __tune_athlon_sse__ 1
// CHECK_ATHLON_XP_M32: #define i386 1
// RUN: %clang -march=athlon-xp -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_XP_M64
// CHECK_ATHLON_XP_M64: error:
//
// RUN: %clang -march=athlon-mp -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_MP_M32
// CHECK_ATHLON_MP_M32: #define __3dNOW_A__ 1
// CHECK_ATHLON_MP_M32: #define __3dNOW__ 1
// CHECK_ATHLON_MP_M32: #define __MMX__ 1
// CHECK_ATHLON_MP_M32: #define __SSE__ 1
// CHECK_ATHLON_MP_M32: #define __athlon 1
// CHECK_ATHLON_MP_M32: #define __athlon__ 1
// CHECK_ATHLON_MP_M32: #define __athlon_sse__ 1
// CHECK_ATHLON_MP_M32: #define __i386 1
// CHECK_ATHLON_MP_M32: #define __i386__ 1
// CHECK_ATHLON_MP_M32: #define __tune_athlon__ 1
// CHECK_ATHLON_MP_M32: #define __tune_athlon_sse__ 1
// CHECK_ATHLON_MP_M32: #define i386 1
// RUN: %clang -march=athlon-mp -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_MP_M64
// CHECK_ATHLON_MP_M64: error:
//
// RUN: %clang -march=x86-64 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_X86_64_M32
// CHECK_X86_64_M32: #define __MMX__ 1
// CHECK_X86_64_M32: #define __SSE2__ 1
// CHECK_X86_64_M32: #define __SSE__ 1
// CHECK_X86_64_M32: #define __i386 1
// CHECK_X86_64_M32: #define __i386__ 1
// CHECK_X86_64_M32: #define __k8 1
// CHECK_X86_64_M32: #define __k8__ 1
// CHECK_X86_64_M32: #define i386 1
// RUN: %clang -march=x86-64 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_X86_64_M64
// CHECK_X86_64_M64: #define __MMX__ 1
// CHECK_X86_64_M64: #define __SSE2_MATH__ 1
// CHECK_X86_64_M64: #define __SSE2__ 1
// CHECK_X86_64_M64: #define __SSE_MATH__ 1
// CHECK_X86_64_M64: #define __SSE__ 1
// CHECK_X86_64_M64: #define __amd64 1
// CHECK_X86_64_M64: #define __amd64__ 1
// CHECK_X86_64_M64: #define __k8 1
// CHECK_X86_64_M64: #define __k8__ 1
// CHECK_X86_64_M64: #define __x86_64 1
// CHECK_X86_64_M64: #define __x86_64__ 1
//
// RUN: %clang -march=k8 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K8_M32
// CHECK_K8_M32: #define __3dNOW_A__ 1
// CHECK_K8_M32: #define __3dNOW__ 1
// CHECK_K8_M32: #define __MMX__ 1
// CHECK_K8_M32: #define __SSE2__ 1
// CHECK_K8_M32: #define __SSE__ 1
// CHECK_K8_M32: #define __i386 1
// CHECK_K8_M32: #define __i386__ 1
// CHECK_K8_M32: #define __k8 1
// CHECK_K8_M32: #define __k8__ 1
// CHECK_K8_M32: #define __tune_k8__ 1
// CHECK_K8_M32: #define i386 1
// RUN: %clang -march=k8 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K8_M64
// CHECK_K8_M64: #define __3dNOW_A__ 1
// CHECK_K8_M64: #define __3dNOW__ 1
// CHECK_K8_M64: #define __MMX__ 1
// CHECK_K8_M64: #define __SSE2_MATH__ 1
// CHECK_K8_M64: #define __SSE2__ 1
// CHECK_K8_M64: #define __SSE_MATH__ 1
// CHECK_K8_M64: #define __SSE__ 1
// CHECK_K8_M64: #define __amd64 1
// CHECK_K8_M64: #define __amd64__ 1
// CHECK_K8_M64: #define __k8 1
// CHECK_K8_M64: #define __k8__ 1
// CHECK_K8_M64: #define __tune_k8__ 1
// CHECK_K8_M64: #define __x86_64 1
// CHECK_K8_M64: #define __x86_64__ 1
//
// RUN: %clang -march=k8-sse3 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K8_SSE3_M32
// CHECK_K8_SSE3_M32: #define __3dNOW_A__ 1
// CHECK_K8_SSE3_M32: #define __3dNOW__ 1
// CHECK_K8_SSE3_M32: #define __MMX__ 1
// CHECK_K8_SSE3_M32: #define __SSE2__ 1
// CHECK_K8_SSE3_M32: #define __SSE3__ 1
// CHECK_K8_SSE3_M32: #define __SSE__ 1
// CHECK_K8_SSE3_M32: #define __i386 1
// CHECK_K8_SSE3_M32: #define __i386__ 1
// CHECK_K8_SSE3_M32: #define __k8 1
// CHECK_K8_SSE3_M32: #define __k8__ 1
// CHECK_K8_SSE3_M32: #define __tune_k8__ 1
// CHECK_K8_SSE3_M32: #define i386 1
// RUN: %clang -march=k8-sse3 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_K8_SSE3_M64
// CHECK_K8_SSE3_M64: #define __3dNOW_A__ 1
// CHECK_K8_SSE3_M64: #define __3dNOW__ 1
// CHECK_K8_SSE3_M64: #define __MMX__ 1
// CHECK_K8_SSE3_M64: #define __SSE2_MATH__ 1
// CHECK_K8_SSE3_M64: #define __SSE2__ 1
// CHECK_K8_SSE3_M64: #define __SSE3__ 1
// CHECK_K8_SSE3_M64: #define __SSE_MATH__ 1
// CHECK_K8_SSE3_M64: #define __SSE__ 1
// CHECK_K8_SSE3_M64: #define __amd64 1
// CHECK_K8_SSE3_M64: #define __amd64__ 1
// CHECK_K8_SSE3_M64: #define __k8 1
// CHECK_K8_SSE3_M64: #define __k8__ 1
// CHECK_K8_SSE3_M64: #define __tune_k8__ 1
// CHECK_K8_SSE3_M64: #define __x86_64 1
// CHECK_K8_SSE3_M64: #define __x86_64__ 1
//
// RUN: %clang -march=opteron -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_OPTERON_M32
// CHECK_OPTERON_M32: #define __3dNOW_A__ 1
// CHECK_OPTERON_M32: #define __3dNOW__ 1
// CHECK_OPTERON_M32: #define __MMX__ 1
// CHECK_OPTERON_M32: #define __SSE2__ 1
// CHECK_OPTERON_M32: #define __SSE__ 1
// CHECK_OPTERON_M32: #define __i386 1
// CHECK_OPTERON_M32: #define __i386__ 1
// CHECK_OPTERON_M32: #define __k8 1
// CHECK_OPTERON_M32: #define __k8__ 1
// CHECK_OPTERON_M32: #define __tune_k8__ 1
// CHECK_OPTERON_M32: #define i386 1
// RUN: %clang -march=opteron -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_OPTERON_M64
// CHECK_OPTERON_M64: #define __3dNOW_A__ 1
// CHECK_OPTERON_M64: #define __3dNOW__ 1
// CHECK_OPTERON_M64: #define __MMX__ 1
// CHECK_OPTERON_M64: #define __SSE2_MATH__ 1
// CHECK_OPTERON_M64: #define __SSE2__ 1
// CHECK_OPTERON_M64: #define __SSE_MATH__ 1
// CHECK_OPTERON_M64: #define __SSE__ 1
// CHECK_OPTERON_M64: #define __amd64 1
// CHECK_OPTERON_M64: #define __amd64__ 1
// CHECK_OPTERON_M64: #define __k8 1
// CHECK_OPTERON_M64: #define __k8__ 1
// CHECK_OPTERON_M64: #define __tune_k8__ 1
// CHECK_OPTERON_M64: #define __x86_64 1
// CHECK_OPTERON_M64: #define __x86_64__ 1
//
// RUN: %clang -march=opteron-sse3 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_OPTERON_SSE3_M32
// CHECK_OPTERON_SSE3_M32: #define __3dNOW_A__ 1
// CHECK_OPTERON_SSE3_M32: #define __3dNOW__ 1
// CHECK_OPTERON_SSE3_M32: #define __MMX__ 1
// CHECK_OPTERON_SSE3_M32: #define __SSE2__ 1
// CHECK_OPTERON_SSE3_M32: #define __SSE3__ 1
// CHECK_OPTERON_SSE3_M32: #define __SSE__ 1
// CHECK_OPTERON_SSE3_M32: #define __i386 1
// CHECK_OPTERON_SSE3_M32: #define __i386__ 1
// CHECK_OPTERON_SSE3_M32: #define __k8 1
// CHECK_OPTERON_SSE3_M32: #define __k8__ 1
// CHECK_OPTERON_SSE3_M32: #define __tune_k8__ 1
// CHECK_OPTERON_SSE3_M32: #define i386 1
// RUN: %clang -march=opteron-sse3 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_OPTERON_SSE3_M64
// CHECK_OPTERON_SSE3_M64: #define __3dNOW_A__ 1
// CHECK_OPTERON_SSE3_M64: #define __3dNOW__ 1
// CHECK_OPTERON_SSE3_M64: #define __MMX__ 1
// CHECK_OPTERON_SSE3_M64: #define __SSE2_MATH__ 1
// CHECK_OPTERON_SSE3_M64: #define __SSE2__ 1
// CHECK_OPTERON_SSE3_M64: #define __SSE3__ 1
// CHECK_OPTERON_SSE3_M64: #define __SSE_MATH__ 1
// CHECK_OPTERON_SSE3_M64: #define __SSE__ 1
// CHECK_OPTERON_SSE3_M64: #define __amd64 1
// CHECK_OPTERON_SSE3_M64: #define __amd64__ 1
// CHECK_OPTERON_SSE3_M64: #define __k8 1
// CHECK_OPTERON_SSE3_M64: #define __k8__ 1
// CHECK_OPTERON_SSE3_M64: #define __tune_k8__ 1
// CHECK_OPTERON_SSE3_M64: #define __x86_64 1
// CHECK_OPTERON_SSE3_M64: #define __x86_64__ 1
//
// RUN: %clang -march=athlon64 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON64_M32
// CHECK_ATHLON64_M32: #define __3dNOW_A__ 1
// CHECK_ATHLON64_M32: #define __3dNOW__ 1
// CHECK_ATHLON64_M32: #define __MMX__ 1
// CHECK_ATHLON64_M32: #define __SSE2__ 1
// CHECK_ATHLON64_M32: #define __SSE__ 1
// CHECK_ATHLON64_M32: #define __i386 1
// CHECK_ATHLON64_M32: #define __i386__ 1
// CHECK_ATHLON64_M32: #define __k8 1
// CHECK_ATHLON64_M32: #define __k8__ 1
// CHECK_ATHLON64_M32: #define __tune_k8__ 1
// CHECK_ATHLON64_M32: #define i386 1
// RUN: %clang -march=athlon64 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON64_M64
// CHECK_ATHLON64_M64: #define __3dNOW_A__ 1
// CHECK_ATHLON64_M64: #define __3dNOW__ 1
// CHECK_ATHLON64_M64: #define __MMX__ 1
// CHECK_ATHLON64_M64: #define __SSE2_MATH__ 1
// CHECK_ATHLON64_M64: #define __SSE2__ 1
// CHECK_ATHLON64_M64: #define __SSE_MATH__ 1
// CHECK_ATHLON64_M64: #define __SSE__ 1
// CHECK_ATHLON64_M64: #define __amd64 1
// CHECK_ATHLON64_M64: #define __amd64__ 1
// CHECK_ATHLON64_M64: #define __k8 1
// CHECK_ATHLON64_M64: #define __k8__ 1
// CHECK_ATHLON64_M64: #define __tune_k8__ 1
// CHECK_ATHLON64_M64: #define __x86_64 1
// CHECK_ATHLON64_M64: #define __x86_64__ 1
//
// RUN: %clang -march=athlon64-sse3 -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON64_SSE3_M32
// CHECK_ATHLON64_SSE3_M32: #define __3dNOW_A__ 1
// CHECK_ATHLON64_SSE3_M32: #define __3dNOW__ 1
// CHECK_ATHLON64_SSE3_M32: #define __MMX__ 1
// CHECK_ATHLON64_SSE3_M32: #define __SSE2__ 1
// CHECK_ATHLON64_SSE3_M32: #define __SSE3__ 1
// CHECK_ATHLON64_SSE3_M32: #define __SSE__ 1
// CHECK_ATHLON64_SSE3_M32: #define __i386 1
// CHECK_ATHLON64_SSE3_M32: #define __i386__ 1
// CHECK_ATHLON64_SSE3_M32: #define __k8 1
// CHECK_ATHLON64_SSE3_M32: #define __k8__ 1
// CHECK_ATHLON64_SSE3_M32: #define __tune_k8__ 1
// CHECK_ATHLON64_SSE3_M32: #define i386 1
// RUN: %clang -march=athlon64-sse3 -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON64_SSE3_M64
// CHECK_ATHLON64_SSE3_M64: #define __3dNOW_A__ 1
// CHECK_ATHLON64_SSE3_M64: #define __3dNOW__ 1
// CHECK_ATHLON64_SSE3_M64: #define __MMX__ 1
// CHECK_ATHLON64_SSE3_M64: #define __SSE2_MATH__ 1
// CHECK_ATHLON64_SSE3_M64: #define __SSE2__ 1
// CHECK_ATHLON64_SSE3_M64: #define __SSE3__ 1
// CHECK_ATHLON64_SSE3_M64: #define __SSE_MATH__ 1
// CHECK_ATHLON64_SSE3_M64: #define __SSE__ 1
// CHECK_ATHLON64_SSE3_M64: #define __amd64 1
// CHECK_ATHLON64_SSE3_M64: #define __amd64__ 1
// CHECK_ATHLON64_SSE3_M64: #define __k8 1
// CHECK_ATHLON64_SSE3_M64: #define __k8__ 1
// CHECK_ATHLON64_SSE3_M64: #define __tune_k8__ 1
// CHECK_ATHLON64_SSE3_M64: #define __x86_64 1
// CHECK_ATHLON64_SSE3_M64: #define __x86_64__ 1
//
// RUN: %clang -march=athlon-fx -m32 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_FX_M32
// CHECK_ATHLON_FX_M32: #define __3dNOW_A__ 1
// CHECK_ATHLON_FX_M32: #define __3dNOW__ 1
// CHECK_ATHLON_FX_M32: #define __MMX__ 1
// CHECK_ATHLON_FX_M32: #define __SSE2__ 1
// CHECK_ATHLON_FX_M32: #define __SSE__ 1
// CHECK_ATHLON_FX_M32: #define __i386 1
// CHECK_ATHLON_FX_M32: #define __i386__ 1
// CHECK_ATHLON_FX_M32: #define __k8 1
// CHECK_ATHLON_FX_M32: #define __k8__ 1
// CHECK_ATHLON_FX_M32: #define __tune_k8__ 1
// CHECK_ATHLON_FX_M32: #define i386 1
// RUN: %clang -march=athlon-fx -m64 -E -dM %s -o - 2>&1 \
// RUN:   | FileCheck %s -check-prefix=CHECK_ATHLON_FX_M64
// CHECK_ATHLON_FX_M64: #define __3dNOW_A__ 1
// CHECK_ATHLON_FX_M64: #define __3dNOW__ 1
// CHECK_ATHLON_FX_M64: #define __MMX__ 1
// CHECK_ATHLON_FX_M64: #define __SSE2_MATH__ 1
// CHECK_ATHLON_FX_M64: #define __SSE2__ 1
// CHECK_ATHLON_FX_M64: #define __SSE_MATH__ 1
// CHECK_ATHLON_FX_M64: #define __SSE__ 1
// CHECK_ATHLON_FX_M64: #define __amd64 1
// CHECK_ATHLON_FX_M64: #define __amd64__ 1
// CHECK_ATHLON_FX_M64: #define __k8 1
// CHECK_ATHLON_FX_M64: #define __k8__ 1
// CHECK_ATHLON_FX_M64: #define __tune_k8__ 1
// CHECK_ATHLON_FX_M64: #define __x86_64 1
// CHECK_ATHLON_FX_M64: #define __x86_64__ 1
//
// End X86/GCC/Linux tests ------------------
