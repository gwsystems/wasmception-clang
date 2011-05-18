// RUN: %clang_cc1 -std=c++0x -fexceptions -fcxx-exceptions -emit-llvm -o - %s | FileCheck %s

struct A {
  ~A();
};

struct B {
  ~B() throw(int);
};

struct C {
  B b;
  ~C() {}
};

struct D {
  ~D() noexcept(false);
};

struct E {
  D d;
  ~E() {}
};

void foo() {
  A a;
  C c;
  E e;
  // CHECK: invoke void @_ZN1ED1Ev
  // CHECK: invoke void @_ZN1CD1Ev
  // CHECK: call void @_ZN1AD1Ev
}

struct F {
  D d;
  ~F();
};
F::~F() noexcept(false) {}

struct G {
  D d;
  ~G();
};
G::~G() {}

struct H {
  B b;
  ~H();
};
H::~H() throw(int) {}

struct I {
  B b;
  ~I();
};
I::~I() {}

// Template variants.

template <typename T>
struct TA {
  ~TA();
};

template <typename T>
struct TB {
  ~TB() throw(int);
};

template <typename T>
struct TC {
  TB<T> b;
  ~TC() {}
};

template <typename T>
struct TD {
  ~TD() noexcept(false);
};

template <typename T>
struct TE {
  TD<T> d;
  ~TE() {}
};

void tfoo() {
  TA<int> a;
  TC<int> c;
  TE<int> e;
  // CHECK: invoke void @_ZN2TEIiED1Ev
  // CHECK: invoke void @_ZN2TCIiED1Ev
  // CHECK: call void @_ZN2TAIiED1Ev
}

template <typename T>
struct TF {
  TD<T> d;
  ~TF();
};
template <typename T>
TF<T>::~TF() noexcept(false) {}

template <typename T>
struct TG {
  TD<T> d;
  ~TG();
};
template <typename T>
TG<T>::~TG() {}

template <typename T>
struct TH {
  TB<T> b;
  ~TH();
};
template <typename T>
TH<T>::~TH() {}

void tinst() {
  TF<int> f;
  TG<int> g;
  TH<int> h;
}
// CHECK: define linkonce_odr void @_ZN2THIiED1Ev
// CHECK: _ZTIi
// CHECK: __cxa_call_unexpected
