// RUN: %clang_cc1 -triple x86_64-apple-darwin10 -fsyntax-only -verify -std=c2x %s

struct S {};
struct S * [[clang::address_space(1)]] Foo;

enum [[clang::enum_extensibility(open)]] EnumOpen {
  C0 = 1, C1 = 10
};

enum [[clang::flag_enum]] EnumFlag {
  D0 = 1, D1 = 8
};

void foo(void *c) [[clang::overloadable]];
void foo(char *c) [[clang::overloadable]];

void context_okay(void *context [[clang::swift_context]]) [[clang::swiftcall]];
void context_okay2(void *context [[clang::swift_context]], void *selfType, char **selfWitnessTable) [[clang::swiftcall]];

void *f1(void) [[clang::ownership_returns(foo)]];
void *f2() [[clang::ownership_returns(foo)]]; // expected-warning {{'ownership_returns' attribute only applies to non-K&R-style functions}}

void foo2(void) [[clang::unavailable("not available - replaced")]]; // expected-note {{'foo2' has been explicitly marked unavailable here}}
void bar(void) {
  foo2(); // expected-error {{'foo2' is unavailable: not available - replaced}}
}
