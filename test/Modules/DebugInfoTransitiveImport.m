// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fmodule-format=obj -g -dwarf-ext-refs \
// RUN:     -fimplicit-module-maps -fmodules-cache-path=%t -I %S/Inputs \
// RUN:     %s -mllvm -debug-only=pchcontainer 2>&1 | FileCheck %s
// REQUIRES: asserts

@import diamond_left;

// Definition of top:
// CHECK: !DICompileUnit({{.*}}dwoId:
// CHECK: !DIFile({{.*}}diamond_top.h

// Definition of left:
// CHECK: !DICompileUnit({{.*}}dwoId:
// CHECK: !DIFile({{.*}}diamond_left
// CHECK: !DIImportedEntity(tag: DW_TAG_imported_declaration,
// CHECK-SAME:              entity: ![[MODULE:.*]], line: 3)
// CHECK: ![[MODULE]] = !DIModule(scope: null, name: "diamond_top"

// Skeleton for top:
// CHECK: !DICompileUnit({{.*}}splitDebugFilename: {{.*}}diamond_top{{.*}}dwoId:

