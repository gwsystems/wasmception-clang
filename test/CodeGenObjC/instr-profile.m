// Test that instrumentation based profiling feeds branch prediction
// correctly. This tests both generation of profile data and use of the same,
// and the input file for the -fprofile-instr-use case is expected to be result
// of running the program generated by the -fprofile-instr-generate case. As
// such, main() should call every function in this test.

// RUN: %clang %s -o - -emit-llvm -S -fprofile-instr-generate | FileCheck -check-prefix=PGOGEN %s
// RUN: %clang %s -o - -emit-llvm -S -fprofile-instr-use=%S/Inputs/instr-profile.profdata | FileCheck -check-prefix=PGOUSE %s

#ifdef HAVE_FOUNDATION
// Use this to build an instrumented version to regenerate the input file.
#import <Foundation/Foundation.h>
#else
struct NSFastEnumerationState;
@interface NSArray
- (unsigned long) countByEnumeratingWithState: (struct NSFastEnumerationState*) state
                  objects: (id*) buffer
                  count: (unsigned long) bufferSize;
+(NSArray*) arrayWithObjects: (id) first, ...;
@end;
#endif

// PGOGEN: @[[FOR:__llvm_pgo_ctr[0-9]*]] = private global [2 x i64] zeroinitializer

// PGOGEN-LABEL: @foreach
// PGOUSE-LABEL: @foreach
// PGOGEN: store {{.*}} @[[FOR]], i64 0, i64 0
void foreach(NSArray *array) {
  // PGOGEN: store {{.*}} @[[FOR]], i64 0, i64 1
  // FIXME: We don't emit branch weights for this yet.
  for (id x in array) {
  }
}

int main(int argc, const char *argv[]) {
  NSArray *array = [NSArray arrayWithObjects: @"0", @"1", (void*)0];
  foreach(array);
}
