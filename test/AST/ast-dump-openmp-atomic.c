// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fopenmp -ast-dump %s | FileCheck --match-full-lines -implicit-check-not=openmp_structured_block %s

void test(int i) {
#pragma omp atomic
  ++i;
}

// CHECK: TranslationUnitDecl {{.*}} <<invalid sloc>> <invalid sloc>
// CHECK: `-FunctionDecl {{.*}} <{{.*}}ast-dump-openmp-atomic.c:3:1, line:6:1> line:3:6 test 'void (int)'
// CHECK-NEXT:   |-ParmVarDecl {{.*}} <col:11, col:15> col:15 used i 'int'
// CHECK-NEXT:   `-CompoundStmt {{.*}} <col:18, line:6:1>
// CHECK-NEXT:     `-OMPAtomicDirective {{.*}} <line:4:9, col:19>
// CHECK-NEXT:       `-CapturedStmt {{.*}} <line:5:3, col:5>
// CHECK-NEXT:         |-CapturedDecl {{.*}} <<invalid sloc>> <invalid sloc>
// CHECK-NEXT:         | |-UnaryOperator {{.*}} <col:3, col:5> openmp_structured_block 'int' prefix '++'
// CHECK-NEXT:         | | `-DeclRefExpr {{.*}} <col:5> 'int' lvalue ParmVar {{.*}} 'i' 'int'
// CHECK-NEXT:         | `-ImplicitParamDecl {{.*}} <line:4:9> col:9 implicit __context 'struct (anonymous at {{.*}}ast-dump-openmp-atomic.c:4:9) *const restrict'
// CHECK-NEXT:         `-DeclRefExpr {{.*}} <line:5:5> 'int' lvalue ParmVar {{.*}} 'i' 'int'
