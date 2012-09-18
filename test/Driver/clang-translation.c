// RUN: %clang -target i386-unknown-unknown -### -S -O0 -Os %s -o %t.s -fverbose-asm -funwind-tables -fvisibility=hidden 2> %t.log
// RUN: grep '"-triple" "i386-unknown-unknown"' %t.log
// RUN: grep '"-S"' %t.log
// RUN: grep '"-disable-free"' %t.log
// RUN: grep '"-mrelocation-model" "static"' %t.log
// RUN: grep '"-mdisable-fp-elim"' %t.log
// RUN: grep '"-munwind-tables"' %t.log
// RUN: grep '"-Os"' %t.log
// RUN: grep '"-o" .*clang-translation.*' %t.log
// RUN: grep '"-masm-verbose"' %t.log
// RUN: grep '"-fvisibility" "hidden"' %t.log
// RUN: %clang -target i386-apple-darwin9 -### -S %s -o %t.s 2> %t.log
// RUN: grep '"-target-cpu" "yonah"' %t.log
// RUN: %clang -target x86_64-apple-darwin9 -### -S %s -o %t.s 2> %t.log
// RUN: grep '"-target-cpu" "core2"' %t.log

// RUN: %clang -target x86_64-apple-darwin10 -### -S %s 2> %t.log \
// RUN:   -arch armv7
// RUN: FileCheck -check-prefix=ARMV7_DEFAULT %s < %t.log
// ARMV7_DEFAULT: clang
// ARMV7_DEFAULT: "-cc1"
// ARMV7_DEFAULT-NOT: "-msoft-float"
// ARMV7_DEFAULT: "-mfloat-abi" "soft"
// ARMV7_DEFAULT-NOT: "-msoft-float"
// ARMV7_DEFAULT: "-x" "c"

// RUN: %clang -target x86_64-apple-darwin10 -### -S %s 2> %t.log \
// RUN:   -arch armv7 -msoft-float
// RUN: FileCheck -check-prefix=ARMV7_SOFTFLOAT %s < %t.log
// ARMV7_SOFTFLOAT: clang
// ARMV7_SOFTFLOAT: "-cc1"
// ARMV7_SOFTFLOAT: "-msoft-float"
// ARMV7_SOFTFLOAT: "-mfloat-abi" "soft"
// ARMV7_SOFTFLOAT: "-target-feature"
// ARMV7_SOFTFLOAT: "-neon"
// ARMV7_SOFTFLOAT: "-x" "c"

// RUN: %clang -target x86_64-apple-darwin10 -### -S %s 2> %t.log \
// RUN:   -arch armv7 -mhard-float
// RUN: FileCheck -check-prefix=ARMV7_HARDFLOAT %s < %t.log
// ARMV7_HARDFLOAT: clang
// ARMV7_HARDFLOAT: "-cc1"
// ARMV7_HARDFLOAT-NOT: "-msoft-float"
// ARMV7_HARDFLOAT: "-mfloat-abi" "hard"
// ARMV7_HARDFLOAT-NOT: "-msoft-float"
// ARMV7_HARDFLOAT: "-x" "c"

// RUN: %clang -target arm-linux -### -S %s 2> %t.log \
// RUN:   -march=armv5e
// RUN: FileCheck -check-prefix=ARMV5E %s < %t.log
// ARMV5E: clang
// ARMV5E: "-cc1"
// ARMV5E: "-target-cpu" "arm1022e"

// RUN: %clang -ccc-clang-archs powerpc64 \
// RUN:   -target powerpc64-unknown-linux-gnu -### -S %s 2> %t.log \
// RUN:   -mcpu=G5
// RUN: FileCheck -check-prefix=PPCG5 %s < %t.log
// PPCG5: clang
// PPCG5: "-cc1"
// PPCG5: "-target-cpu" "g5"

// RUN: %clang -ccc-clang-archs powerpc64 \
// RUN:   -target powerpc64-unknown-linux-gnu -### -S %s 2> %t.log      \
// RUN:   -mcpu=power7
// RUN: FileCheck -check-prefix=PPCPWR7 %s < %t.log
// PPCPWR7: clang
// PPCPWR7: "-cc1"
// PPCPWR7: "-target-cpu" "pwr7"

// RUN: %clang -ccc-clang-archs powerpc64 \
// RUN:   -target powerpc64-unknown-linux-gnu -### -S %s 2> %t.log
// RUN: FileCheck -check-prefix=PPC64NS %s < %t.log
// PPC64NS: clang
// PPC64NS: "-cc1"
// PPC64NS: "-target-cpu" "ppc64"

// RUN: %clang -ccc-clang-archs powerpc \
// RUN:   -target powerpc-fsl-linux -### -S %s 2> %t.log \
// RUN:   -mcpu=e500mc
// RUN: FileCheck -check-prefix=PPCE500MC %s < %t.log
// PPCE500MC: clang
// PPCE500MC: "-cc1"
// PPCE500MC: "-target-cpu" "e500mc"

// RUN: %clang -ccc-clang-archs powerpc64 \
// RUN:   -target powerpc64-fsl-linux -### -S %s 2> %t.log \
// RUN:   -mcpu=e5500
// RUN: FileCheck -check-prefix=PPCE5500 %s < %t.log
// PPCE5500: clang
// PPCE5500: "-cc1"
// PPCE5500: "-target-cpu" "e5500"

