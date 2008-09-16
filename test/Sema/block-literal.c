// RUN: clang -fsyntax-only %s -verify

void I( void (^)(void));
void (^noop)(void);

void nothing();
int printf(const char*, ...);

typedef void (^T) (void);

void takeclosure(T);
int takeintint(int (^C)(int)) { return C(4); }

T somefunction() {
	if (^{ })
	  nothing();

	noop = ^{};

	noop = ^{printf("\nClosure\n"); };

	I(^{ });

	return ^{printf("\nClosure\n"); };  // expected-error {{returning block that lives on the local stack}}
}
void test2() {
	int x = 4;

	takeclosure(^{ printf("%d\n", x); });

  while (1) {
	  takeclosure(^{ 
      break;  // expected-error {{'break' statement not in loop or switch statement}}
	    continue; // expected-error {{'continue' statement not in loop statement}}
	    while(1) break;  // ok
      goto foo; // expected-error {{goto not allowed}}
    });
    break;
	}

foo:
	takeclosure(^{ x = 4; });  // expected-error {{expression is not assignable}}
}


void (^test3())(void) { 
  return ^{};   // expected-error {{returning block that lives on the local stack}}
}

void test4() {
  void (^noop)(void) = ^{};
  void (*noop2)() = 0;
}

void *X;

void test_arguments() {
  int y;
  int (^c)(char);
  (1 ? c : 0)('x');
  (1 ? 0 : c)('x');

  (1 ? c : c)('x');
}

#if 0
// Old syntax. FIXME: convert/test.
void test_byref() {
  int i;
  
  X = ^{| g |};  // expected-error {{use of undeclared identifier 'g'}}

  X = ^{| i,i,i | };

  X = ^{|i| i = 0; };

}

// TODO: global closures someday.
void *A = ^{};
void *B = ^(int){ A = 0; };


// Closures can not take return types at this point.
void test_retvals() {
  // Explicit return value.
  ^int{};   // expected-error {{closure with explicit return type requires argument list}}
  X = ^void(){};

  // Optional specification of return type.
  X = ^char{ return 'x'; };  // expected-error {{closure with explicit return type requires argument list}}

  X = ^/*missing declspec*/ *() { return (void*)0; };
  X = ^void*() { return (void*)0; };
  
  //X = ^char(short c){ if (c) return c; else return (int)4; };
  
}

#endif
