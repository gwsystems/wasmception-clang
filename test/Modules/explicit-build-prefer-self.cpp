// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fno-implicit-modules -emit-module -fmodule-name=a %S/Inputs/explicit-build-prefer-self/map -o a.pcm
// RUN: %clang_cc1 -fmodules -fno-implicit-modules -emit-module -fmodule-name=b %S/Inputs/explicit-build-prefer-self/map -o b.pcm
