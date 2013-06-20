// RUN: %clang_cc1 -fsyntax-only -verify -std=gnu++11 %s
// RUN: %clang_cc1 -fsyntax-only -verify -std=c++11 %s

#if !defined(__STRICT_ANSI__)
__float128 f;  // expected-error{{support for type '__float128' is not yet implemented}}
// But this should work:
template<typename> struct __is_floating_point_helper {};
template<> struct __is_floating_point_helper<__float128> {};
#else
__float128 f;  // expected-error {{unknown type name '__float128'}}
template<typename> struct __is_floating_point_helper {};
template<> struct __is_floating_point_helper<__float128> {};  // expected-error {{use of undeclared identifier '__float128'}}
#endif
