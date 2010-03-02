// RUN: %clang_cc1 -fsyntax-only -verify %s 

struct S {
   S (S);  // expected-error {{copy constructor must pass its first argument by reference}}
};

S f();

void g() { 
  S a( f() );
}

namespace PR6064 {
  struct A {
    A() { }
    inline A(A&, int);
  };

  A::A(A&, int = 0) { }

  void f() {
    A const a;
    A b(a);
  }
}
