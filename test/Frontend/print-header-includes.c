// RUN: %clang_cc1 -I%S -include Inputs/test3.h -E -H -o /dev/null %s 2> %t.stderr
// RUN: FileCheck < %t.stderr %s

// CHECK-NOT: test3.h
// CHECK: . {{.*test.h}}
// CHECK: .. {{.*test2.h}}

// RUN: %clang_cc1 -I%S -include Inputs/test3.h --show-includes -o /dev/null %s | \
// RUN:     FileCheck --strict-whitespace --check-prefix=MS-STDOUT %s
// MS-STDOUT-NOT: <command line>
// MS-STDOUT: Note: including file: {{[^ ]*test3.h}}
// MS-STDOUT: Note: including file: {{[^ ]*test.h}}
// MS-STDOUT: Note: including file:  {{[^ ]*test2.h}}
// MS-STDOUT-NOT: Note

// RUN: %clang_cc1 -I%S -include Inputs/test3.h -E --show-includes -o /dev/null %s 2> %t.stderr
// RUN: FileCheck --strict-whitespace --check-prefix=MS-STDERR < %t.stderr %s
// MS-STDERR-NOT: <command line>
// MS-STDERR: Note: including file: {{[^ ]*test3.h}}
// MS-STDERR: Note: including file: {{[^ ]*test.h}}
// MS-STDERR: Note: including file:  {{[^ ]*test2.h}}
// MS-STDERR-NOT: Note

// RUN: echo "fun:foo" > %t.blacklist
// RUN: %clang_cc1 -I%S -fsanitize=address -fdepfile-entry=%t.blacklist --show-includes -o /dev/null %s | \
// RUN:     FileCheck --strict-whitespace --check-prefix=MS-BLACKLIST %s
// MS-BLACKLIST: Note: including file: {{[^ ]*\.blacklist}}
// MS-BLACKLIST: Note: including file: {{[^ ]*test.h}}
// MS-BLACKLIST: Note: including file:  {{[^ ]*test2.h}}
// MS-BLACKLIST-NOT: Note

#include "Inputs/test.h"
