// RUN: %clang_cc1 -verify -fopenmp -x c++ -triple x86_64-unknown-unknown -emit-llvm %s -o - | FileCheck %s
// RUN: %clang_cc1 -fopenmp -x c++ -std=c++11 -triple x86_64-unknown-unknown -emit-pch -o %t %s
// RUN: %clang_cc1 -fopenmp -x c++ -triple x86_64-unknown-unknown -std=c++11 -include-pch %t -verify %s -emit-llvm -o - | FileCheck %s
// RUN: %clang_cc1 -verify -fopenmp -x c++ -std=c++11 -DLAMBDA -triple %itanium_abi_triple -emit-llvm %s -o - | FileCheck -check-prefix=LAMBDA %s
// RUN: %clang_cc1 -verify -fopenmp -x c++ -fblocks -DBLOCKS -triple %itanium_abi_triple -emit-llvm %s -o - | FileCheck -check-prefix=BLOCKS %s
// expected-no-diagnostics
#ifndef HEADER
#define HEADER

template <class T>
struct S {
  T f;
  S(T a) : f(a) {}
  S() : f() {}
  operator T() { return T(); }
  ~S() {}
};

volatile double g;

// CHECK: [[S_FLOAT_TY:%.+]] = type { float }
// CHECK: [[CAP_MAIN_TY:%.+]] = type { i{{[0-9]+}}*, [2 x i{{[0-9]+}}]*, [2 x [[S_FLOAT_TY]]]*, [[S_FLOAT_TY]]* }
// CHECK: [[S_INT_TY:%.+]] = type { i{{[0-9]+}} }
// CHECK: [[CAP_TMAIN_TY:%.+]] = type { i{{[0-9]+}}*, [2 x i{{[0-9]+}}]*, [2 x [[S_INT_TY]]]*, [[S_INT_TY]]* }
template <typename T>
T tmain() {
  S<T> test;
  T t_var = T();
  T vec[] = {1, 2};
  S<T> s_arr[] = {1, 2};
  S<T> var(3);
#pragma omp parallel
#pragma omp for private(t_var, vec, s_arr, s_arr, var, var)
  for (int i = 0; i < 2; ++i) {
    vec[i] = t_var;
    s_arr[i] = var;
  }
  return T();
}

int main() {
#ifdef LAMBDA
  // LAMBDA: [[G:@.+]] = global double
  // LAMBDA-LABEL: @main
  // LAMBDA: call{{( x86_thiscallcc)?}} void [[OUTER_LAMBDA:@.+]](
  [&]() {
  // LAMBDA: define{{.*}} internal{{.*}} void [[OUTER_LAMBDA]](
  // LAMBDA: call void {{.+}} @__kmpc_fork_call({{.+}}, i32 1, {{.+}}* [[OMP_REGION:@.+]] to {{.+}}, i8* %{{.+}})
#pragma omp parallel
#pragma omp for private(g)
  for (int i = 0; i < 2; ++i) {
    // LAMBDA: define{{.*}} internal{{.*}} void [[OMP_REGION]](i32* %{{.+}}, i32* %{{.+}}, %{{.+}}* [[ARG:%.+]])
    // LAMBDA: [[G_PRIVATE_ADDR:%.+]] = alloca double,
    // LAMBDA: store %{{.+}}* [[ARG]], %{{.+}}** [[ARG_REF:%.+]],
    g = 1;
    // LAMBDA: call void @__kmpc_for_static_init_4(
    // LAMBDA: store volatile double 1.0{{.+}}, double* [[G_PRIVATE_ADDR]],
    // LAMBDA: [[G_PRIVATE_ADDR_REF:%.+]] = getelementptr inbounds %{{.+}}, %{{.+}}* [[ARG:%.+]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
    // LAMBDA: store double* [[G_PRIVATE_ADDR]], double** [[G_PRIVATE_ADDR_REF]]
    // LAMBDA: call{{( x86_thiscallcc)?}} void [[INNER_LAMBDA:@.+]](%{{.+}}* [[ARG]])
    // LAMBDA: call void @__kmpc_for_static_fini(
    [&]() {
      // LAMBDA: define {{.+}} void [[INNER_LAMBDA]](%{{.+}}* [[ARG_PTR:%.+]])
      // LAMBDA: store %{{.+}}* [[ARG_PTR]], %{{.+}}** [[ARG_PTR_REF:%.+]],
      g = 2;
      // LAMBDA: [[ARG_PTR:%.+]] = load %{{.+}}*, %{{.+}}** [[ARG_PTR_REF]]
      // LAMBDA: [[G_PTR_REF:%.+]] = getelementptr inbounds %{{.+}}, %{{.+}}* [[ARG_PTR]], i{{[0-9]+}} 0, i{{[0-9]+}} 0
      // LAMBDA: [[G_REF:%.+]] = load double*, double** [[G_PTR_REF]]
      // LAMBDA: store volatile double 2.0{{.+}}, double* [[G_REF]]
    }();
  }
  }();
  return 0;
#elif defined(BLOCKS)
  // BLOCKS: [[G:@.+]] = global double
  // BLOCKS-LABEL: @main
  // BLOCKS: call void {{%.+}}(i8
  ^{
  // BLOCKS: define{{.*}} internal{{.*}} void {{.+}}(i8*
  // BLOCKS: call void {{.+}} @__kmpc_fork_call({{.+}}, i32 1, {{.+}}* [[OMP_REGION:@.+]] to {{.+}}, i8* {{.+}})
#pragma omp parallel
#pragma omp for private(g)
  for (int i = 0; i < 2; ++i) {
    // BLOCKS: define{{.*}} internal{{.*}} void [[OMP_REGION]](i32* %{{.+}}, i32* %{{.+}}, %{{.+}}* [[ARG:%.+]])
    // BLOCKS: [[G_PRIVATE_ADDR:%.+]] = alloca double,
    // BLOCKS: store %{{.+}}* [[ARG]], %{{.+}}** [[ARG_REF:%.+]],
    g = 1;
    // BLOCKS: call void @__kmpc_for_static_init_4(
    // BLOCKS: store volatile double 1.0{{.+}}, double* [[G_PRIVATE_ADDR]],
    // BLOCKS-NOT: [[G]]{{[[^:word:]]}}
    // BLOCKS: double* [[G_PRIVATE_ADDR]]
    // BLOCKS-NOT: [[G]]{{[[^:word:]]}}
    // BLOCKS: call void {{%.+}}(i8
    // BLOCKS: call void @__kmpc_for_static_fini(
    ^{
      // BLOCKS: define {{.+}} void {{@.+}}(i8*
      g = 2;
      // BLOCKS-NOT: [[G]]{{[[^:word:]]}}
      // BLOCKS: store volatile double 2.0{{.+}}, double*
      // BLOCKS-NOT: [[G]]{{[[^:word:]]}}
      // BLOCKS: ret
    }();
  }
  }();
  return 0;
#else
  S<float> test;
  int t_var = 0;
  int vec[] = {1, 2};
  S<float> s_arr[] = {1, 2};
  S<float> var(3);
#pragma omp parallel
#pragma omp for private(t_var, vec, s_arr, s_arr, var, var)
  for (int i = 0; i < 2; ++i) {
    vec[i] = t_var;
    s_arr[i] = var;
  }
  int i;
#pragma omp parallel
#pragma omp for private(i)
  for (i = 0; i < 2; ++i) {
    ;
  }
  return tmain<int>();
#endif
}

// CHECK: define i{{[0-9]+}} @main()
// CHECK: [[TEST:%.+]] = alloca [[S_FLOAT_TY]],
// CHECK: call {{.*}} [[S_FLOAT_TY_DEF_CONSTR:@.+]]([[S_FLOAT_TY]]* [[TEST]])
// CHECK: %{{.+}} = bitcast [[CAP_MAIN_TY]]*
// CHECK: call void (%{{.+}}*, i{{[0-9]+}}, void (i{{[0-9]+}}*, i{{[0-9]+}}*, ...)*, ...) @__kmpc_fork_call(%{{.+}}* @{{.+}}, i{{[0-9]+}} 1, void (i{{[0-9]+}}*, i{{[0-9]+}}*, ...)* bitcast (void (i{{[0-9]+}}*, i{{[0-9]+}}*, [[CAP_MAIN_TY]]*)* [[MAIN_MICROTASK:@.+]] to void
// CHECK: = call i{{.+}} [[TMAIN_INT:@.+]]()
// CHECK: call void [[S_FLOAT_TY_DESTR:@.+]]([[S_FLOAT_TY]]*
// CHECK: ret
//
// CHECK: define internal void [[MAIN_MICROTASK]](i{{[0-9]+}}* [[GTID_ADDR:%.+]], i{{[0-9]+}}* %{{.+}}, [[CAP_MAIN_TY]]* %{{.+}})
// CHECK: [[T_VAR_PRIV:%.+]] = alloca i{{[0-9]+}},
// CHECK: [[VEC_PRIV:%.+]] = alloca [2 x i{{[0-9]+}}],
// CHECK: [[S_ARR_PRIV:%.+]] = alloca [2 x [[S_FLOAT_TY]]],
// CHECK-NOT: alloca [2 x [[S_FLOAT_TY]]],
// CHECK: [[VAR_PRIV:%.+]] = alloca [[S_FLOAT_TY]],
// CHECK-NOT: alloca [[S_FLOAT_TY]],
// CHECK: store i{{[0-9]+}}* [[GTID_ADDR]], i{{[0-9]+}}** [[GTID_ADDR_REF:%.+]]
// CHECK-NOT: [[T_VAR_PRIV]]
// CHECK-NOT: [[VEC_PRIV]]
// CHECK: {{.+}}:
// CHECK: [[S_ARR_PRIV_ITEM:%.+]] = phi [[S_FLOAT_TY]]*
// CHECK: call {{.*}} [[S_FLOAT_TY_DEF_CONSTR]]([[S_FLOAT_TY]]* [[S_ARR_PRIV_ITEM]])
// CHECK-NOT: [[T_VAR_PRIV]]
// CHECK-NOT: [[VEC_PRIV]]
// CHECK: call {{.*}} [[S_FLOAT_TY_DEF_CONSTR]]([[S_FLOAT_TY]]* [[VAR_PRIV]])
// CHECK: call void @__kmpc_for_static_init_4(
// CHECK: call void @__kmpc_for_static_fini(
// CHECK-DAG: call void [[S_FLOAT_TY_DESTR]]([[S_FLOAT_TY]]* [[VAR_PRIV]])
// CHECK-DAG: call void [[S_FLOAT_TY_DESTR]]([[S_FLOAT_TY]]*
// CHECK: ret void

// CHECK: define {{.*}} i{{[0-9]+}} [[TMAIN_INT]]()
// CHECK: [[TEST:%.+]] = alloca [[S_INT_TY]],
// CHECK: call {{.*}} [[S_INT_TY_DEF_CONSTR:@.+]]([[S_INT_TY]]* [[TEST]])
// CHECK: call void (%{{.+}}*, i{{[0-9]+}}, void (i{{[0-9]+}}*, i{{[0-9]+}}*, ...)*, ...) @__kmpc_fork_call(%{{.+}}* @{{.+}}, i{{[0-9]+}} 1, void (i{{[0-9]+}}*, i{{[0-9]+}}*, ...)* bitcast (void (i{{[0-9]+}}*, i{{[0-9]+}}*, [[CAP_TMAIN_TY]]*)* [[TMAIN_MICROTASK:@.+]] to void
// CHECK: call void [[S_INT_TY_DESTR:@.+]]([[S_INT_TY]]*
// CHECK: ret
//
// CHECK: define internal void [[TMAIN_MICROTASK]](i{{[0-9]+}}* [[GTID_ADDR:%.+]], i{{[0-9]+}}* %{{.+}}, [[CAP_TMAIN_TY]]* %{{.+}})
// CHECK: [[T_VAR_PRIV:%.+]] = alloca i{{[0-9]+}},
// CHECK: [[VEC_PRIV:%.+]] = alloca [2 x i{{[0-9]+}}],
// CHECK: [[S_ARR_PRIV:%.+]] = alloca [2 x [[S_INT_TY]]],
// CHECK-NOT: alloca [2 x [[S_INT_TY]]],
// CHECK: [[VAR_PRIV:%.+]] = alloca [[S_INT_TY]],
// CHECK-NOT: alloca [[S_INT_TY]],
// CHECK: store i{{[0-9]+}}* [[GTID_ADDR]], i{{[0-9]+}}** [[GTID_ADDR_REF:%.+]]
// CHECK-NOT: [[T_VAR_PRIV]]
// CHECK-NOT: [[VEC_PRIV]]
// CHECK: {{.+}}:
// CHECK: [[S_ARR_PRIV_ITEM:%.+]] = phi [[S_INT_TY]]*
// CHECK: call {{.*}} [[S_INT_TY_DEF_CONSTR]]([[S_INT_TY]]* [[S_ARR_PRIV_ITEM]])
// CHECK-NOT: [[T_VAR_PRIV]]
// CHECK-NOT: [[VEC_PRIV]]
// CHECK: call {{.*}} [[S_INT_TY_DEF_CONSTR]]([[S_INT_TY]]* [[VAR_PRIV]])
// CHECK: call void @__kmpc_for_static_init_4(
// CHECK: call void @__kmpc_for_static_fini(
// CHECK-DAG: call void [[S_INT_TY_DESTR]]([[S_INT_TY]]* [[VAR_PRIV]])
// CHECK-DAG: call void [[S_INT_TY_DESTR]]([[S_INT_TY]]*
// CHECK: ret void
#endif

