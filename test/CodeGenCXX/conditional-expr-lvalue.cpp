// RUN: clang-cc -emit-llvm-only %s
void f(bool flag) {
  int a = 1;
  int b = 2;
  
  (flag ? a : b) = 3;
}
