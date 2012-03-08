// RUN: %clang_cc1 -fsyntax-only -fobjc-arc -fblocks -verify %s

// rdar://problem/10982793
// [p foo] in ARC creates a cleanup.
// The plus is invalid and causes the cleanup to go unbound.
// Don't crash.
@interface A
- (id) foo;
@end
void takeBlock(void (^)(void));
void test0(id p) {
  takeBlock(^{ [p foo] + p; }); // expected-error {{invalid operands to binary expression}}
}
