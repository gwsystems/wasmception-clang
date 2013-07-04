// RUN: not %clang_cc1 -fsyntax-only %s 2>&1 | FileCheck -strict-whitespace %s
// RUN: not %clang_cc1 -fsyntax-only -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck -check-prefix=CHECK-MACHINE %s

struct Foo {
  int bar;
};

// PR13312
void test1() {
  struct Foo foo;
  foo.bar = 42☃
// CHECK: error: non-ASCII characters are not allowed outside of literals and identifiers
// CHECK: {{^              \^}}
// CHECK: error: expected ';' after expression
// Make sure we emit the fixit right in front of the snowman.
// CHECK: {{^              \^}}
// CHECK: {{^              ;}}

// CHECK-MACHINE: fix-it:"{{.*}}fixit-unicode.c":{[[@LINE-8]]:15-[[@LINE-8]]:18}:""
// CHECK-MACHINE: fix-it:"{{.*}}fixit-unicode.c":{[[@LINE-9]]:15-[[@LINE-9]]:15}:";"
}


int printf(const char *, ...);
void test2() {
  printf("∆: %d", 1L);
// CHECK: warning: format specifies type 'int' but the argument has type 'long'
// Don't crash emitting a fixit after the delta.
// CHECK:  printf("
// CHECK: : %d", 1L);
// Unfortunately, we can't actually check the location of the printed fixit,
// because different systems will render the delta differently (either as a
// character, or as <U+2206>.) The fixit should line up with the %d regardless.

// CHECK-MACHINE: fix-it:"{{.*}}fixit-unicode.c":{[[@LINE-9]]:16-[[@LINE-9]]:18}:"%ld"
}

void test3() {
  int กssss = 42;
  int a = กsss; // expected-error{{use of undeclared identifier 'กsss'; did you mean 'กssss'?}}
// CHECK: {{^          \^}}
// CHECK: {{^          [^ ]+ssss}}
// CHECK-MACHINE: fix-it:"{{.*}}":{[[@LINE-3]]:11-[[@LINE-3]]:17}:"\340\270\201ssss"

  int ssกss = 42;
  int b = ssกs; // expected-error{{use of undeclared identifier 'ssกs'; did you mean 'ssกss'?}}
// CHECK: {{^          \^}}
// CHECK: {{^          ss.+ss}}
// CHECK-MACHINE: fix-it:"{{.*}}":{[[@LINE-3]]:11-[[@LINE-3]]:17}:"ss\340\270\201ss"

  int sssssssssก = 42;
  int c = sssssssss; // expected-error{{use of undeclared identifier 'sssssssss'; did you mean 'sssssssssก'?}}
// CHECK: {{^          \^}}
// CHECK: {{^          sssssssss.+}}
// CHECK-MACHINE: fix-it:"{{.*}}":{[[@LINE-3]]:11-[[@LINE-3]]:20}:"sssssssss\340\270\201"
}
