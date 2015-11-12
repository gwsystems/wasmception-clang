// RUN: %clang -### %s 2>&1 | FileCheck %s -check-prefix=NO-MSBITFIELDS
// RUN: %clang -### -mno-ms-bitfields -mms-bitfields %s 2>&1 | FileCheck %s -check-prefix=MSBITFIELDS
// RUN: %clang -### -mms-bitfields -mno-ms-bitfields %s 2>&1 | FileCheck %s -check-prefix=NO-MSBITFIELDS

// MSBITFIELDS: -mms-bitfields
// NO-MSBITFIELDS-NOT: -mms-bitfields
