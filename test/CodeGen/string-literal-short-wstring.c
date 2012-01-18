// RUN: %clang_cc1 -x c++ -emit-llvm -fshort-wchar %s -o - | FileCheck %s
// Runs in c++ mode so that wchar_t is available.

int main() {
  // This should convert to utf8.
  // CHECK: private unnamed_addr constant [10 x i8] c"\E1\84\A0\C8\A0\F4\82\80\B0\00", align 1
  char b[10] = "\u1120\u0220\U00102030";

  // CHECK: private unnamed_addr constant [3 x i16] [i16 65, i16 66, i16 0]
  const wchar_t *foo = L"AB";

  // This should convert to utf16.
  // CHECK: private unnamed_addr constant [5 x i16] [i16 4384, i16 544, i16 -9272, i16 -9168, i16 0]
  const wchar_t *bar = L"\u1120\u0220\U00102030";



  // Should pick second character.
  // CHECK: store i8 98
  char c = 'ab';

  // CHECK: store i16 97
  wchar_t wa = L'a';

  // Should pick second character.
  // CHECK: store i16 98
  wchar_t wb = L'ab';

  // -4085 == 0xf00b
  // CHECK: store i16 -4085
  wchar_t wc = L'\uF00B';
}
