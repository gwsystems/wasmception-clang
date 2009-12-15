/* RUN: %clang_cc1 -E %s | grep 'a 3'
 * RUN: %clang_cc1 -E %s | grep 'b 16'
 * RUN: %clang_cc1 -E -P %s | grep 'a 3'
 * RUN: %clang_cc1 -E -P %s | grep 'b 16'
 * RUN: %clang_cc1 -E %s | not grep '# 0 '
 * PR1848
 * PR3437
*/

#define t(x) x

t(a
3)

t(b
__LINE__)

