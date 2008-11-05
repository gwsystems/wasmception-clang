// RUN: clang -fsyntax-only -verify -std=c++98 %s 

// Verify that we can't initialize non-aggregates with an initializer
// list.
struct NonAggr1 {
  NonAggr1(int) { }

  int m;
};

struct Base { };
struct NonAggr2 : public Base {
  int m;
};

class NonAggr3 {
  int m;
};

// FIXME: virtual functions
struct NonAggr4 {
};

NonAggr1 na1 = { 17 }; // expected-error{{initialization of non-aggregate type 'struct NonAggr1' with an initializer list}}
NonAggr2 na2 = { 17 }; // expected-error{{initialization of non-aggregate type 'struct NonAggr2' with an initializer list}}
NonAggr3 na3 = { 17 }; // expected-error{{initialization of non-aggregate type 'class NonAggr3' with an initializer list}}
