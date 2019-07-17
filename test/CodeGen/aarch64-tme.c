// RUN: %clang_cc1 -triple aarch64-eabi -target-feature +tme -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -DUSE_ACLE  -triple aarch64-eabi -target-feature +tme -S -emit-llvm %s -o - | FileCheck %s

#ifdef USE_ACLE
#include "arm_acle.h"
void test_tme_funcs() {
  __tstart();
  (void)__ttest();
  __tcommit();
  __tcancel(0x789a);
}
#else
void test_tme_funcs() {
  __builtin_arm_tstart();
  (void)__builtin_arm_ttest();
  __builtin_arm_tcommit();
  __builtin_arm_tcancel(0x789a);
}
#endif
// CHECK: call i64 @llvm.aarch64.tstart()
// CHECK: call i64 @llvm.aarch64.ttest()
// CHECK: call void @llvm.aarch64.tcommit()
// CHECK: call void @llvm.aarch64.tcancel(i64 30874)

// CHECK: declare i64 @llvm.aarch64.tstart() #1
// CHECK: declare i64 @llvm.aarch64.ttest() #1
// CHECK: declare void @llvm.aarch64.tcommit() #1
// CHECK: declare void @llvm.aarch64.tcancel(i64 immarg) #2

#ifdef __ARM_FEATURE_TME
void arm_feature_tme_defined() {}
#endif
// CHECK: define void @arm_feature_tme_defined()

// CHECK: attributes #1 = { nounwind }
// CHECK: attributes #2 = { noreturn nounwind }
