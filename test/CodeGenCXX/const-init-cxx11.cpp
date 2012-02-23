// RUN: %clang_cc1 -verify -triple x86_64-apple-darwin -emit-llvm -o - %s -std=c++11 | FileCheck %s

// FIXME: The padding in all these objects should be zero-initialized.
namespace StructUnion {
  struct A {
    int n;
    double d;
    union U {
      constexpr U(int x) : x(x) {}
      constexpr U(const char *y) : y(y) {}
      int x;
      const char *y;
    } u;

    constexpr A(int n, double d, int x) : n(n), d(d), u(x) {}
    constexpr A(int n, double d, const char *y) : n(n), d(d), u(y) {}
  };

  // CHECK: @_ZN11StructUnion1aE = constant {{.*}} { i32 1, double 2.000000e+00, {{.*}} { i32 3, [4 x i8] undef } }
  extern constexpr A a(1, 2.0, 3);

  // CHECK: @_ZN11StructUnion1bE = constant {{.*}} { i32 4, double 5.000000e+00, {{.*}} { i8* getelementptr inbounds ([6 x i8]* @{{.*}}, i32 0, i32 0) } }
  extern constexpr A b(4, 5, "hello");

  struct B {
    int n;
  };

  // CHECK: @_ZN11StructUnion1cE = global {{.*}} zeroinitializer
  // CHECK: @_ZN11StructUnion2c2E = global {{.*}} zeroinitializer
  B c;
  B c2 = B();

  // CHECK: @_ZN11StructUnion1dE = global {{.*}} zeroinitializer
  B d[10];

  struct C {
    constexpr C() : c(0) {}
    int c;
  };

  // CHECK: @_ZN11StructUnion1eE = global {{.*}} zeroinitializer
  C e[10];

  struct D {
    constexpr D() : d(5) {}
    int d;
  };

  // CHECK: @_ZN11StructUnion1fE = global {{.*}} { i32 5 }
  D f;
}

namespace BaseClass {
  template<typename T, unsigned> struct X : T {};
  struct C { char c = 1; };
  template<unsigned... Ns> struct Cs : X<C,Ns>... {};
  struct N { int n = 3; };
  struct D { double d = 4.0; };

  template<typename ...Ts>
  struct Test : Ts... { constexpr Test() : Ts()..., n(5) {} int n; };

  using Test1 = Test<N, C, Cs<1,2>, D, X<C,1>>;
  // CHECK: @_ZN9BaseClass2t1E = constant {{.*}} { i32 3, i8 1, i8 1, i8 1, double 4.000000e+00, i8 1, i32 5 }, align 8
  extern constexpr Test1 t1 = Test1();

  struct DN : D, N {};
  struct DND : DN, X<D,0> {};
  struct DNN : DN, X<N,0> {};
  // CHECK: @_ZN9BaseClass3dndE = constant {{.*}} { double 4.000000e+00, i32 3, double 4.000000e+00 }
  extern constexpr DND dnd = DND();
  // Note, N subobject is laid out in DN subobject's tail padding.
  // CHECK: @_ZN9BaseClass3dnnE = constant {{.*}} { double 4.000000e+00, i32 3, i32 3 }
  extern constexpr DNN dnn = DNN();

  struct E {};
  struct Test2 : X<E,0>, X<E,1>, X<E,2>, X<E,3> {};
  // CHECK: @_ZN9BaseClass2t2E = constant {{.*}} undef
  extern constexpr Test2 t2 = Test2();
}

namespace Array {
  // CHECK: @_ZN5Array3arrE = constant [2 x i32] [i32 4, i32 0]
  extern constexpr int arr[2] = { 4 };

  // CHECK: @_ZN5Array1cE = constant [6 x [4 x i8]] [{{.*}} c"foo\00", [4 x i8] c"a\00\00\00", [4 x i8] c"bar\00", [4 x i8] c"xyz\00", [4 x i8] c"b\00\00\00", [4 x i8] c"123\00"]
  extern constexpr char c[6][4] = { "foo", "a", { "bar" }, { 'x', 'y', 'z' }, { "b" }, '1', '2', '3' };

  struct C { constexpr C() : n(5) {} int n, m = 3 * n + 1; };
  // CHECK: @_ZN5Array5ctorsE = constant [3 x {{.*}}] [{{.*}} { i32 5, i32 16 }, {{.*}} { i32 5, i32 16 }, {{.*}} { i32 5, i32 16 }]
  extern const C ctors[3];
  constexpr C ctors[3];

  // CHECK: @_ZN5Array1dE = constant {{.*}} { [2 x i32] [i32 1, i32 2], [3 x i32] [i32 3, i32 4, i32 5] }
  struct D { int n[2]; int m[3]; } extern constexpr d = { 1, 2, 3, 4, 5 };

  struct E {
    char c[4];
    char d[4];
    constexpr E() : c("foo"), d("x") {}
  };
  // CHECK: @_ZN5Array1eE = constant {{.*}} { [4 x i8] c"foo\00", [4 x i8] c"x\00\00\00" }
  extern constexpr E e = E();
}

namespace MemberPtr {
  struct B1 {
    int a, b;
    virtual void f();
    void g();
  };
  struct B2 {
    int c, d;
    virtual void h();
    void i();
  };
  struct C : B1 {
    int e;
    virtual void j();
    void k();
  };
  struct D : C, B2 {
    int z;
    virtual void l();
    void m();
  };

  // CHECK: @_ZN9MemberPtr2daE = constant i64 8
  // CHECK: @_ZN9MemberPtr2dbE = constant i64 12
  // CHECK: @_ZN9MemberPtr2dcE = constant i64 32
  // CHECK: @_ZN9MemberPtr2ddE = constant i64 36
  // CHECK: @_ZN9MemberPtr2deE = constant i64 16
  // CHECK: @_ZN9MemberPtr2dzE = constant i64 40
  extern constexpr int (D::*da) = &B1::a;
  extern constexpr int (D::*db) = &C::b;
  extern constexpr int (D::*dc) = &B2::c;
  extern constexpr int (D::*dd) = &D::d;
  extern constexpr int (D::*de) = &C::e;
  extern constexpr int (D::*dz) = &D::z;

  // CHECK: @_ZN9MemberPtr2baE = constant i64 8
  // CHECK: @_ZN9MemberPtr2bbE = constant i64 12
  // CHECK: @_ZN9MemberPtr2bcE = constant i64 8
  // CHECK: @_ZN9MemberPtr2bdE = constant i64 12
  // CHECK: @_ZN9MemberPtr2beE = constant i64 16
  // CHECK: @_ZN9MemberPtr3b1zE = constant i64 40
  // CHECK: @_ZN9MemberPtr3b2zE = constant i64 16
  extern constexpr int (B1::*ba) = (int(B1::*))&B1::a;
  extern constexpr int (B1::*bb) = (int(B1::*))&C::b;
  extern constexpr int (B2::*bc) = (int(B2::*))&B2::c;
  extern constexpr int (B2::*bd) = (int(B2::*))&D::d;
  extern constexpr int (B1::*be) = (int(B1::*))&C::e;
  extern constexpr int (B1::*b1z) = (int(B1::*))&D::z;
  extern constexpr int (B2::*b2z) = (int(B2::*))&D::z;

  // CHECK: @_ZN9MemberPtr2dfE = constant {{.*}} { i64 1, i64 0 }
  // CHECK: @_ZN9MemberPtr2dgE = constant {{.*}} { i64 {{.*}}2B11gEv{{.*}}, i64 0 }
  // CHECK: @_ZN9MemberPtr2dhE = constant {{.*}} { i64 1, i64 24 }
  // CHECK: @_ZN9MemberPtr2diE = constant {{.*}} { i64 {{.*}}2B21iEv{{.*}}, i64 24 }
  // CHECK: @_ZN9MemberPtr2djE = constant {{.*}} { i64 9, i64 0 }
  // CHECK: @_ZN9MemberPtr2dkE = constant {{.*}} { i64 {{.*}}1C1kEv{{.*}}, i64 0 }
  // CHECK: @_ZN9MemberPtr2dlE = constant {{.*}} { i64 17, i64 0 }
  // CHECK: @_ZN9MemberPtr2dmE = constant {{.*}} { i64 {{.*}}1D1mEv{{.*}}, i64 0 }
  extern constexpr void (D::*df)() = &C::f;
  extern constexpr void (D::*dg)() = &B1::g;
  extern constexpr void (D::*dh)() = &B2::h;
  extern constexpr void (D::*di)() = &D::i;
  extern constexpr void (D::*dj)() = &C::j;
  extern constexpr void (D::*dk)() = &C::k;
  extern constexpr void (D::*dl)() = &D::l;
  extern constexpr void (D::*dm)() = &D::m;

  // CHECK: @_ZN9MemberPtr2bfE = constant {{.*}} { i64 1, i64 0 }
  // CHECK: @_ZN9MemberPtr2bgE = constant {{.*}} { i64 {{.*}}2B11gEv{{.*}}, i64 0 }
  // CHECK: @_ZN9MemberPtr2bhE = constant {{.*}} { i64 1, i64 0 }
  // CHECK: @_ZN9MemberPtr2biE = constant {{.*}} { i64 {{.*}}2B21iEv{{.*}}, i64 0 }
  // CHECK: @_ZN9MemberPtr2bjE = constant {{.*}} { i64 9, i64 0 }
  // CHECK: @_ZN9MemberPtr2bkE = constant {{.*}} { i64 {{.*}}1C1kEv{{.*}}, i64 0 }
  // CHECK: @_ZN9MemberPtr3b1lE = constant {{.*}} { i64 17, i64 0 }
  // CHECK: @_ZN9MemberPtr3b1mE = constant {{.*}} { i64 {{.*}}1D1mEv{{.*}}, i64 0 }
  // CHECK: @_ZN9MemberPtr3b2lE = constant {{.*}} { i64 17, i64 -24 }
  // CHECK: @_ZN9MemberPtr3b2mE = constant {{.*}} { i64 {{.*}}1D1mEv{{.*}}, i64 -24 }
  extern constexpr void (B1::*bf)()  = (void(B1::*)())&C::f;
  extern constexpr void (B1::*bg)()  = (void(B1::*)())&B1::g;
  extern constexpr void (B2::*bh)()  = (void(B2::*)())&B2::h;
  extern constexpr void (B2::*bi)()  = (void(B2::*)())&D::i;
  extern constexpr void (B1::*bj)()  = (void(B1::*)())&C::j;
  extern constexpr void (B1::*bk)()  = (void(B1::*)())&C::k;
  extern constexpr void (B1::*b1l)() = (void(B1::*)())&D::l;
  extern constexpr void (B1::*b1m)() = (void(B1::*)())&D::m;
  extern constexpr void (B2::*b2l)() = (void(B2::*)())&D::l;
  extern constexpr void (B2::*b2m)() = (void(B2::*)())&D::m;
}

namespace LiteralReference {
  struct Lit {
    constexpr Lit() : n(5) {}
    int n;
  };
  // FIXME: This should have static initialization, but we do not implement
  // that yet. For now, just check that we don't set the (pointer) value of
  // the reference to 5!
  //
  // CHECK: @_ZN16LiteralReference3litE = global {{.*}} null
  const Lit &lit = Lit();
}

namespace NonLiteralConstexpr {
  constexpr int factorial(int n) {
    return n ? factorial(n-1) * n : 1;
  }
  extern void f(int *p);

  struct NonTrivialDtor {
    constexpr NonTrivialDtor() : n(factorial(5)), p(&n) {}
    ~NonTrivialDtor() {
      f(p);
    }

    int n;
    int *p;
  };
  static_assert(!__is_literal(NonTrivialDtor), "");
  // CHECK: @_ZN19NonLiteralConstexpr3ntdE = global {{.*}} { i32 120, i32* getelementptr
  NonTrivialDtor ntd;

  struct VolatileMember {
    constexpr VolatileMember() : n(5) {}
    volatile int n;
  };
  static_assert(!__is_literal(VolatileMember), "");
  // CHECK: @_ZN19NonLiteralConstexpr2vmE = global {{.*}} { i32 5 }
  VolatileMember vm;

  struct Both {
    constexpr Both() : n(10) {}
    ~Both();
    volatile int n;
  };
  // CHECK: @_ZN19NonLiteralConstexpr1bE = global {{.*}} { i32 10 }
  Both b;

  void StaticVars() {
    // CHECK: @_ZZN19NonLiteralConstexpr10StaticVarsEvE3ntd = {{.*}} { i32 120, i32* getelementptr {{.*}}
    // CHECK: @_ZGVZN19NonLiteralConstexpr10StaticVarsEvE3ntd =
    static NonTrivialDtor ntd;
    // CHECK: @_ZZN19NonLiteralConstexpr10StaticVarsEvE2vm = {{.*}} { i32 5 }
    // CHECK-NOT: @_ZGVZN19NonLiteralConstexpr10StaticVarsEvE2vm =
    static VolatileMember vm;
    // CHECK: @_ZZN19NonLiteralConstexpr10StaticVarsEvE1b = {{.*}} { i32 10 }
    // CHECK: @_ZGVZN19NonLiteralConstexpr10StaticVarsEvE1b =
    static Both b;
  }
}

// PR12067
namespace VirtualMembers {
  struct A {
    constexpr A(double d) : d(d) {}
    virtual void f();
    double d;
  };
  struct B : A {
    constexpr B() : A(2.0), c{'h', 'e', 'l', 'l', 'o'} {}
    constexpr B(int n) : A(n), c{'w', 'o', 'r', 'l', 'd'} {}
    virtual void g();
    char c[5];
  };
  struct C {
    constexpr C() : n(64) {}
    int n;
  };
  struct D : C, A, B {
    constexpr D() : A(1.0), B(), s(5) {}
    short s;
  };
  struct E : D, B {
    constexpr E() : B(3), c{'b','y','e'} {}
    char c[3];
  };

  // CHECK: @_ZN14VirtualMembers1eE = global { i8**, double, i32, i8**, double, [5 x i8], i16, i8**, double, [5 x i8], [3 x i8] } { i8** getelementptr inbounds ([11 x i8*]* @_ZTVN14VirtualMembers1EE, i64 0, i64 2), double 1.000000e+00, i32 64, i8** getelementptr inbounds ([11 x i8*]* @_ZTVN14VirtualMembers1EE, i64 0, i64 5), double 2.000000e+00, [5 x i8] c"hello", i16 5, i8** getelementptr inbounds ([11 x i8*]* @_ZTVN14VirtualMembers1EE, i64 0, i64 9), double 3.000000e+00, [5 x i8] c"world", [3 x i8] c"bye" }
  E e;

  struct nsMemoryImpl {
    virtual void f();
  };
  // CHECK: @_ZN14VirtualMembersL13sGlobalMemoryE = internal global { i8** } { i8** getelementptr inbounds ([3 x i8*]* @_ZTVN14VirtualMembers12nsMemoryImplE, i64 0, i64 2) }
  static nsMemoryImpl sGlobalMemory;
}

// Constant initialization tests go before this point,
// dynamic initialization tests go after.

// We must emit a constant initializer for NonLiteralConstexpr::ntd, but also
// emit an initializer to register its destructor.
// CHECK: define {{.*}}cxx_global_var_init{{.*}}
// CHECK-NOT: NonLiteralConstexpr
// CHECK: call {{.*}}cxa_atexit{{.*}} @_ZN19NonLiteralConstexpr14NonTrivialDtorD1Ev {{.*}} @_ZN19NonLiteralConstexpr3ntdE
// CHECK-NEXT: ret void

// We don't need to emit any dynamic initialization for NonLiteralConstexpr::vm.
// CHECK-NOT: NonLiteralConstexpr2vm

// We must emit a constant initializer for NonLiteralConstexpr::b, but also
// emit an initializer to register its destructor.
// CHECK: define {{.*}}cxx_global_var_init{{.*}}
// CHECK-NOT: NonLiteralConstexpr
// CHECK: call {{.*}}cxa_atexit{{.*}} @_ZN19NonLiteralConstexpr4BothD1Ev {{.*}} @_ZN19NonLiteralConstexpr1bE
// CHECK-NEXT: ret void

// CHECK: define {{.*}}NonLiteralConstexpr10StaticVars
// CHECK-NOT: }
// CHECK: call {{.*}}cxa_atexit{{.*}}@_ZN19NonLiteralConstexpr14NonTrivialDtorD1Ev
// CHECK-NOT: }
// CHECK: call {{.*}}cxa_atexit{{.*}}@_ZN19NonLiteralConstexpr4BothD1Ev

namespace CrossFuncLabelDiff {
  // Make sure we refuse to constant-fold the variable b.
  constexpr long a(bool x) { return x ? 0 : (long)&&lbl + (0 && ({lbl: 0;})); }
  void test() { static long b = (long)&&lbl - a(false); lbl: return; }
  // CHECK: sub nsw i64 ptrtoint (i8* blockaddress(@_ZN18CrossFuncLabelDiff4testEv, {{.*}}) to i64),
  // CHECK: store i64 {{.*}}, i64* @_ZZN18CrossFuncLabelDiff4testEvE1b, align 8
}

// PR12012
namespace VirtualBase {
  struct B {};
  struct D : virtual B {};
  D d;
  // CHECK: call {{.*}}@_ZN11VirtualBase1DC1Ev

  template<typename T> struct X : T {
    constexpr X() : T() {}
  };
  X<D> x;
  // CHECK: call {{.*}}@_ZN11VirtualBase1XINS_1DEEC1Ev
}
