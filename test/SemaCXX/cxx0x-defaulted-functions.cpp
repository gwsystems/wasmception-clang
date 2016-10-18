// RUN: %clang_cc1 -std=c++11 -fsyntax-only -verify -fcxx-exceptions %s

void fn() = default; // expected-error {{only special member}}
struct foo {
  void fn() = default; // expected-error {{only special member}}

  foo() = default;
  foo(const foo&) = default;
  foo(foo&&) = default;
  foo& operator = (const foo&) = default;
  foo& operator = (foo&&) = default;
  ~foo() = default;
};

struct bar {
  bar();
  bar(const bar&);
  bar(bar&&);
  bar& operator = (const bar&);
  bar& operator = (bar&&);
  ~bar();
};

bar::bar() = default;
bar::bar(const bar&) = default;
bar::bar(bar&&) = default;
bar& bar::operator = (const bar&) = default;
bar& bar::operator = (bar&&) = default;
bar::~bar() = default;

static_assert(__is_trivial(foo), "foo should be trivial");

static_assert(!__has_trivial_destructor(bar), "bar's destructor isn't trivial");
static_assert(!__has_trivial_constructor(bar),
              "bar's default constructor isn't trivial");
static_assert(!__has_trivial_copy(bar), "bar has no trivial copy");
static_assert(!__has_trivial_assign(bar), "bar has no trivial assign");

void tester() {
  foo f, g(f);
  bar b, c(b);
  f = g;
  b = c;
}

template<typename T> struct S : T {
  constexpr S() = default;
  constexpr S(const S&) = default;
  constexpr S(S&&) = default;
};
struct lit { constexpr lit() {} };
S<lit> s_lit; // ok
S<bar> s_bar; // ok

struct Friends {
  friend S<bar>::S();
  friend S<bar>::S(const S&);
  friend S<bar>::S(S&&);
};

namespace DefaultedFnExceptionSpec {
  // DR1330: The exception-specification of an implicitly-declared special
  // member function is evaluated as needed.
  template<typename T> T &&declval();
  template<typename T> struct pair {
    pair(const pair&) noexcept(noexcept(T(declval<T>())));
  };

  struct Y;
  struct X { X(); X(const Y&); };
  struct Y { pair<X> p; };

  template<typename T>
  struct A {
    pair<T> p;
  };
  struct B {
    B();
    B(const A<B>&);
  };

  // Don't crash here.
  void f() {
    X x = X();
    (void)noexcept(B(declval<B>()));
  }

  template<typename T>
  struct Error {
    void f() noexcept(T::error);

    Error() noexcept(T::error); // expected-error {{type 'int' cannot be used prior to '::' because it has no members}}
    Error(const Error&) noexcept(T::error);
    Error(Error&&) noexcept(T::error);
    Error &operator=(const Error&) noexcept(T::error);
    Error &operator=(Error&&) noexcept(T::error);
    ~Error() noexcept(T::error); // expected-error {{type 'int' cannot be used prior to '::' because it has no members}}
  };

  struct DelayImplicit {
    Error<int> e;
  };

  // Don't instantiate the exception specification here.
  void test1(decltype(declval<DelayImplicit>() = DelayImplicit(DelayImplicit())));
  void test2(decltype(declval<DelayImplicit>() = declval<const DelayImplicit>()));
  void test3(decltype(DelayImplicit(declval<const DelayImplicit>())));

  // Any odr-use causes the exception specification to be evaluated.
  struct OdrUse { // \
    expected-note {{instantiation of exception specification for 'Error'}} \
    expected-note {{instantiation of exception specification for '~Error'}}
    Error<int> e;
  };
  OdrUse use; // expected-note {{implicit default constructor for 'DefaultedFnExceptionSpec::OdrUse' first required here}}
}

namespace PR13527 {
  struct X {
    X() = delete; // expected-note {{here}}
    X(const X&) = delete; // expected-note {{here}}
    X(X&&) = delete; // expected-note {{here}}
    X &operator=(const X&) = delete; // expected-note {{here}}
    X &operator=(X&&) = delete; // expected-note {{here}}
    ~X() = delete; // expected-note {{here}}
  };
  X::X() = default; // expected-error {{redefinition}}
  X::X(const X&) = default; // expected-error {{redefinition}}
  X::X(X&&) = default; // expected-error {{redefinition}}
  X &X::operator=(const X&) = default; // expected-error {{redefinition}}
  X &X::operator=(X&&) = default; // expected-error {{redefinition}}
  X::~X() = default; // expected-error {{redefinition}}

  struct Y {
    Y() = default;
    Y(const Y&) = default;
    Y(Y&&) = default;
    Y &operator=(const Y&) = default;
    Y &operator=(Y&&) = default;
    ~Y() = default;
  };
  Y::Y() noexcept = default; // expected-error {{definition of explicitly defaulted}}
  Y::Y(const Y&) noexcept = default; // expected-error {{definition of explicitly defaulted}}
  Y::Y(Y&&) noexcept = default; // expected-error {{definition of explicitly defaulted}}
  Y &Y::operator=(const Y&) noexcept = default; // expected-error {{definition of explicitly defaulted}}
  Y &Y::operator=(Y&&) noexcept = default; // expected-error {{definition of explicitly defaulted}}
  Y::~Y() = default; // expected-error {{definition of explicitly defaulted}}
}

namespace PR27699 {
  struct X {
    X();
  };
  X::X() = default; // expected-note {{here}}
  X::X() = default; // expected-error {{redefinition of 'X'}}
}

namespace PR14577 {
  template<typename T>
  struct Outer {
    template<typename U>
    struct Inner1 {
      ~Inner1();
    };

    template<typename U>
    struct Inner2 {
      ~Inner2();
    };
  };

  template<typename T>
  Outer<T>::Inner1<T>::~Inner1() = delete; // expected-error {{nested name specifier 'Outer<T>::Inner1<T>::' for declaration does not refer into a class, class template or class template partial specialization}}  expected-error {{only functions can have deleted definitions}}

  template<typename T>
  Outer<T>::Inner2<T>::~Inner2() = default; // expected-error {{nested name specifier 'Outer<T>::Inner2<T>::' for declaration does not refer into a class, class template or class template partial specialization}}  expected-error {{only special member functions may be defaulted}}
}

extern "C" {
 template<typename _Tp> // expected-error {{templates must have C++ linkage}}
 void PR13573(const _Tp&) = delete;
}

namespace PR15597 {
  template<typename T> struct A {
    A() noexcept(true) = default;
    ~A() noexcept(true) = default;
  };
  template<typename T> struct B {
    B() noexcept(false) = default; // expected-error {{does not match the calculated one}}
    ~B() noexcept(false) = default; // expected-error {{does not match the calculated one}}
  };
  A<int> a;
  B<int> b; // expected-note {{here}}
}

namespace PR27941 {
struct ExplicitBool {
  ExplicitBool &operator=(bool) = default; // expected-error{{only special member functions may be defaulted}}
  int member;
};

int fn() {
  ExplicitBool t;
  t = true;
}
}

namespace dependent_classes {
template <bool B, typename X, typename Y>
struct conditional;

template <typename X, typename Y>
struct conditional<true, X, Y> { typedef X type; };

template <typename X, typename Y>
struct conditional<false, X, Y> { typedef Y type; };

template<bool B> struct X {
  X();

  // B == false triggers error for = default.
  using T = typename conditional<B, const X &, int>::type;
  X(T) = default;  // expected-error {{only special member functions}}

  // Either value of B creates a constructor that can be default
  using U = typename conditional<B, X&&, const X&>::type;
  X(U) = default;
};

X<true> x1;
X<false> x2; // expected-note {{in instantiation}}

template <typename Type>
class E {
  explicit E(const int &) = default;
};

template <typename Type>
E<Type>::E(const int&) {}  // expected-error {{definition of explicitly defaulted function}}

}
