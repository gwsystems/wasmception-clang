// RUN: %clang_cc1 %s -triple=x86_64-apple-darwin10 -emit-llvm -o - | FileCheck %s

// CHECK: ; ModuleID
struct A {
  template<typename T>
  A(T);
};

template<typename T> A::A(T) {}

struct B {
  template<typename T>
  B(T);
};

template<typename T> B::B(T) {}

// CHECK: define weak_odr void @_ZN1BC1IiEET_(%struct.B* %this, i32)
// CHECK: define weak_odr void @_ZN1BC2IiEET_(%struct.B* %this, i32)
template B::B(int);

template<typename T>
struct C {
  void f() {
    int a[] = { 1, 2, 3 };
  }
};

void f(C<int>& c) {
  c.f();
}
