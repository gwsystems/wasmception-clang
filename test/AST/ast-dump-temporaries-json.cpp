// RUN: %clang_cc1 -triple x86_64-linux-gnu -fsyntax-only -ast-dump=json %s | FileCheck %s

struct S {
  S() {}
  ~S() {}
};

void func(const int &);

void BindTemp() {
  const S &s = S();
}

void MaterializeTemp() {
  const int &i = 1.0;

  float f = 1.0f;
  func(f);
}



// CHECK:  "kind": "ExprWithCleanups",
// CHECK-NEXT:  "range": {
// CHECK-NEXT:   "begin": {
// CHECK-NEXT:    "col": 16,
// CHECK-NEXT:    "file": "{{.*}}",
// CHECK-NEXT:    "line": 11
// CHECK-NEXT:   },
// CHECK-NEXT:   "end": {
// CHECK-NEXT:    "col": 18,
// CHECK-NEXT:    "file": "{{.*}}",
// CHECK-NEXT:    "line": 11
// CHECK-NEXT:   }
// CHECK-NEXT:  },
// CHECK-NEXT:  "type": {
// CHECK-NEXT:   "qualType": "const S"
// CHECK-NEXT:  },
// CHECK-NEXT:  "valueCategory": "lvalue",
// CHECK-NEXT:  "cleanupsHaveSideEffects": true,
// CHECK-NEXT:  "inner": [
// CHECK-NEXT:   {
// CHECK-NEXT:    "id": "0x{{.*}}",
// CHECK-NEXT:    "kind": "MaterializeTemporaryExpr",
// CHECK-NEXT:    "range": {
// CHECK-NEXT:     "begin": {
// CHECK-NEXT:      "col": 16,
// CHECK-NEXT:      "file": "{{.*}}",
// CHECK-NEXT:      "line": 11
// CHECK-NEXT:     },
// CHECK-NEXT:     "end": {
// CHECK-NEXT:      "col": 18,
// CHECK-NEXT:      "file": "{{.*}}",
// CHECK-NEXT:      "line": 11
// CHECK-NEXT:     }
// CHECK-NEXT:    },
// CHECK-NEXT:    "type": {
// CHECK-NEXT:     "qualType": "const S"
// CHECK-NEXT:    },
// CHECK-NEXT:    "valueCategory": "lvalue",
// CHECK-NEXT:    "extendingDecl": {
// CHECK-NEXT:     "id": "0x{{.*}}",
// CHECK-NEXT:     "kind": "VarDecl",
// CHECK-NEXT:     "name": "s",
// CHECK-NEXT:     "type": {
// CHECK-NEXT:      "qualType": "const S &"
// CHECK-NEXT:     }
// CHECK-NEXT:    },
// CHECK-NEXT:    "storageDuration": "automatic",
// CHECK-NEXT:    "boundToLValueRef": true,
// CHECK-NEXT:    "inner": [
// CHECK-NEXT:     {
// CHECK-NEXT:      "id": "0x{{.*}}",
// CHECK-NEXT:      "kind": "ImplicitCastExpr",
// CHECK-NEXT:      "range": {
// CHECK-NEXT:       "begin": {
// CHECK-NEXT:        "col": 16,
// CHECK-NEXT:        "file": "{{.*}}",
// CHECK-NEXT:        "line": 11
// CHECK-NEXT:       },
// CHECK-NEXT:       "end": {
// CHECK-NEXT:        "col": 18,
// CHECK-NEXT:        "file": "{{.*}}",
// CHECK-NEXT:        "line": 11
// CHECK-NEXT:       }
// CHECK-NEXT:      },
// CHECK-NEXT:      "type": {
// CHECK-NEXT:       "qualType": "const S"
// CHECK-NEXT:      },
// CHECK-NEXT:      "valueCategory": "rvalue",
// CHECK-NEXT:      "castKind": "NoOp",
// CHECK-NEXT:      "inner": [
// CHECK-NEXT:       {
// CHECK-NEXT:        "id": "0x{{.*}}",
// CHECK-NEXT:        "kind": "CXXBindTemporaryExpr",
// CHECK-NEXT:        "range": {
// CHECK-NEXT:         "begin": {
// CHECK-NEXT:          "col": 16,
// CHECK-NEXT:          "file": "{{.*}}",
// CHECK-NEXT:          "line": 11
// CHECK-NEXT:         },
// CHECK-NEXT:         "end": {
// CHECK-NEXT:          "col": 18,
// CHECK-NEXT:          "file": "{{.*}}",
// CHECK-NEXT:          "line": 11
// CHECK-NEXT:         }
// CHECK-NEXT:        },
// CHECK-NEXT:        "type": {
// CHECK-NEXT:         "qualType": "S"
// CHECK-NEXT:        },
// CHECK-NEXT:        "valueCategory": "rvalue",
// CHECK-NEXT:        "temp": "0x{{.*}}",
// CHECK-NEXT:        "dtor": {
// CHECK-NEXT:         "id": "0x{{.*}}",
// CHECK-NEXT:         "kind": "CXXDestructorDecl",
// CHECK-NEXT:         "name": "~S",
// CHECK-NEXT:         "type": {
// CHECK-NEXT:          "qualType": "void () noexcept"
// CHECK-NEXT:         }
// CHECK-NEXT:        },
// CHECK-NEXT:        "inner": [
// CHECK-NEXT:         {
// CHECK-NEXT:          "id": "0x{{.*}}",
// CHECK-NEXT:          "kind": "CXXTemporaryObjectExpr",
// CHECK-NEXT:          "range": {
// CHECK-NEXT:           "begin": {
// CHECK-NEXT:            "col": 16,
// CHECK-NEXT:            "file": "{{.*}}",
// CHECK-NEXT:            "line": 11
// CHECK-NEXT:           },
// CHECK-NEXT:           "end": {
// CHECK-NEXT:            "col": 18,
// CHECK-NEXT:            "file": "{{.*}}",
// CHECK-NEXT:            "line": 11
// CHECK-NEXT:           }
// CHECK-NEXT:          },
// CHECK-NEXT:          "type": {
// CHECK-NEXT:           "qualType": "S"
// CHECK-NEXT:          },
// CHECK-NEXT:          "valueCategory": "rvalue",
// CHECK-NEXT:          "ctorType": {
// CHECK-NEXT:           "qualType": "void ()"
// CHECK-NEXT:          },
// CHECK-NEXT:          "hadMultipleCandidates": true,
// CHECK-NEXT:          "constructionKind": "complete"
// CHECK-NEXT:         }
// CHECK-NEXT:        ]
// CHECK-NEXT:       }
// CHECK-NEXT:      ]
// CHECK-NEXT:     }
// CHECK-NEXT:    ]
// CHECK-NEXT:   }
// CHECK-NEXT:  ]
// CHECK-NEXT: }


// CHECK:  "kind": "ExprWithCleanups",
// CHECK-NEXT:  "range": {
// CHECK-NEXT:   "begin": {
// CHECK-NEXT:    "col": 18,
// CHECK-NEXT:    "file": "{{.*}}",
// CHECK-NEXT:    "line": 15
// CHECK-NEXT:   },
// CHECK-NEXT:   "end": {
// CHECK-NEXT:    "col": 18,
// CHECK-NEXT:    "file": "{{.*}}",
// CHECK-NEXT:    "line": 15
// CHECK-NEXT:   }
// CHECK-NEXT:  },
// CHECK-NEXT:  "type": {
// CHECK-NEXT:   "qualType": "const int"
// CHECK-NEXT:  },
// CHECK-NEXT:  "valueCategory": "lvalue",
// CHECK-NEXT:  "inner": [
// CHECK-NEXT:   {
// CHECK-NEXT:    "id": "0x{{.*}}",
// CHECK-NEXT:    "kind": "MaterializeTemporaryExpr",
// CHECK-NEXT:    "range": {
// CHECK-NEXT:     "begin": {
// CHECK-NEXT:      "col": 18,
// CHECK-NEXT:      "file": "{{.*}}",
// CHECK-NEXT:      "line": 15
// CHECK-NEXT:     },
// CHECK-NEXT:     "end": {
// CHECK-NEXT:      "col": 18,
// CHECK-NEXT:      "file": "{{.*}}",
// CHECK-NEXT:      "line": 15
// CHECK-NEXT:     }
// CHECK-NEXT:    },
// CHECK-NEXT:    "type": {
// CHECK-NEXT:     "qualType": "const int"
// CHECK-NEXT:    },
// CHECK-NEXT:    "valueCategory": "lvalue",
// CHECK-NEXT:    "extendingDecl": {
// CHECK-NEXT:     "id": "0x{{.*}}",
// CHECK-NEXT:     "kind": "VarDecl",
// CHECK-NEXT:     "name": "i",
// CHECK-NEXT:     "type": {
// CHECK-NEXT:      "qualType": "const int &"
// CHECK-NEXT:     }
// CHECK-NEXT:    },
// CHECK-NEXT:    "storageDuration": "automatic",
// CHECK-NEXT:    "boundToLValueRef": true,
// CHECK-NEXT:    "inner": [
// CHECK-NEXT:     {
// CHECK-NEXT:      "id": "0x{{.*}}",
// CHECK-NEXT:      "kind": "ImplicitCastExpr",
// CHECK-NEXT:      "range": {
// CHECK-NEXT:       "begin": {
// CHECK-NEXT:        "col": 18,
// CHECK-NEXT:        "file": "{{.*}}",
// CHECK-NEXT:        "line": 15
// CHECK-NEXT:       },
// CHECK-NEXT:       "end": {
// CHECK-NEXT:        "col": 18,
// CHECK-NEXT:        "file": "{{.*}}",
// CHECK-NEXT:        "line": 15
// CHECK-NEXT:       }
// CHECK-NEXT:      },
// CHECK-NEXT:      "type": {
// CHECK-NEXT:       "qualType": "const int"
// CHECK-NEXT:      },
// CHECK-NEXT:      "valueCategory": "rvalue",
// CHECK-NEXT:      "castKind": "FloatingToIntegral",
// CHECK-NEXT:      "inner": [
// CHECK-NEXT:       {
// CHECK-NEXT:        "id": "0x{{.*}}",
// CHECK-NEXT:        "kind": "FloatingLiteral",
// CHECK-NEXT:        "range": {
// CHECK-NEXT:         "begin": {
// CHECK-NEXT:          "col": 18,
// CHECK-NEXT:          "file": "{{.*}}",
// CHECK-NEXT:          "line": 15
// CHECK-NEXT:         },
// CHECK-NEXT:         "end": {
// CHECK-NEXT:          "col": 18,
// CHECK-NEXT:          "file": "{{.*}}",
// CHECK-NEXT:          "line": 15
// CHECK-NEXT:         }
// CHECK-NEXT:        },
// CHECK-NEXT:        "type": {
// CHECK-NEXT:         "qualType": "double"
// CHECK-NEXT:        },
// CHECK-NEXT:        "valueCategory": "rvalue",
// CHECK-NEXT:        "value": 1
// CHECK-NEXT:       }
// CHECK-NEXT:      ]
// CHECK-NEXT:     }
// CHECK-NEXT:    ]
// CHECK-NEXT:   }
// CHECK-NEXT:  ]
// CHECK-NEXT: }


// CHECK:  "kind": "ExprWithCleanups",
// CHECK-NEXT:  "range": {
// CHECK-NEXT:   "begin": {
// CHECK-NEXT:    "col": 3,
// CHECK-NEXT:    "file": "{{.*}}",
// CHECK-NEXT:    "line": 18
// CHECK-NEXT:   },
// CHECK-NEXT:   "end": {
// CHECK-NEXT:    "col": 9,
// CHECK-NEXT:    "file": "{{.*}}",
// CHECK-NEXT:    "line": 18
// CHECK-NEXT:   }
// CHECK-NEXT:  },
// CHECK-NEXT:  "type": {
// CHECK-NEXT:   "qualType": "void"
// CHECK-NEXT:  },
// CHECK-NEXT:  "valueCategory": "rvalue",
// CHECK-NEXT:  "inner": [
// CHECK-NEXT:   {
// CHECK-NEXT:    "id": "0x{{.*}}",
// CHECK-NEXT:    "kind": "CallExpr",
// CHECK-NEXT:    "range": {
// CHECK-NEXT:     "begin": {
// CHECK-NEXT:      "col": 3,
// CHECK-NEXT:      "file": "{{.*}}",
// CHECK-NEXT:      "line": 18
// CHECK-NEXT:     },
// CHECK-NEXT:     "end": {
// CHECK-NEXT:      "col": 9,
// CHECK-NEXT:      "file": "{{.*}}",
// CHECK-NEXT:      "line": 18
// CHECK-NEXT:     }
// CHECK-NEXT:    },
// CHECK-NEXT:    "type": {
// CHECK-NEXT:     "qualType": "void"
// CHECK-NEXT:    },
// CHECK-NEXT:    "valueCategory": "rvalue",
// CHECK-NEXT:    "inner": [
// CHECK-NEXT:     {
// CHECK-NEXT:      "id": "0x{{.*}}",
// CHECK-NEXT:      "kind": "ImplicitCastExpr",
// CHECK-NEXT:      "range": {
// CHECK-NEXT:       "begin": {
// CHECK-NEXT:        "col": 3,
// CHECK-NEXT:        "file": "{{.*}}",
// CHECK-NEXT:        "line": 18
// CHECK-NEXT:       },
// CHECK-NEXT:       "end": {
// CHECK-NEXT:        "col": 3,
// CHECK-NEXT:        "file": "{{.*}}",
// CHECK-NEXT:        "line": 18
// CHECK-NEXT:       }
// CHECK-NEXT:      },
// CHECK-NEXT:      "type": {
// CHECK-NEXT:       "qualType": "void (*)(const int &)"
// CHECK-NEXT:      },
// CHECK-NEXT:      "valueCategory": "rvalue",
// CHECK-NEXT:      "castKind": "FunctionToPointerDecay",
// CHECK-NEXT:      "inner": [
// CHECK-NEXT:       {
// CHECK-NEXT:        "id": "0x{{.*}}",
// CHECK-NEXT:        "kind": "DeclRefExpr",
// CHECK-NEXT:        "range": {
// CHECK-NEXT:         "begin": {
// CHECK-NEXT:          "col": 3,
// CHECK-NEXT:          "file": "{{.*}}",
// CHECK-NEXT:          "line": 18
// CHECK-NEXT:         },
// CHECK-NEXT:         "end": {
// CHECK-NEXT:          "col": 3,
// CHECK-NEXT:          "file": "{{.*}}",
// CHECK-NEXT:          "line": 18
// CHECK-NEXT:         }
// CHECK-NEXT:        },
// CHECK-NEXT:        "type": {
// CHECK-NEXT:         "qualType": "void (const int &)"
// CHECK-NEXT:        },
// CHECK-NEXT:        "valueCategory": "lvalue",
// CHECK-NEXT:        "referencedDecl": {
// CHECK-NEXT:         "id": "0x{{.*}}",
// CHECK-NEXT:         "kind": "FunctionDecl",
// CHECK-NEXT:         "name": "func",
// CHECK-NEXT:         "type": {
// CHECK-NEXT:          "qualType": "void (const int &)"
// CHECK-NEXT:         }
// CHECK-NEXT:        }
// CHECK-NEXT:       }
// CHECK-NEXT:      ]
// CHECK-NEXT:     },
// CHECK-NEXT:     {
// CHECK-NEXT:      "id": "0x{{.*}}",
// CHECK-NEXT:      "kind": "MaterializeTemporaryExpr",
// CHECK-NEXT:      "range": {
// CHECK-NEXT:       "begin": {
// CHECK-NEXT:        "col": 8,
// CHECK-NEXT:        "file": "{{.*}}",
// CHECK-NEXT:        "line": 18
// CHECK-NEXT:       },
// CHECK-NEXT:       "end": {
// CHECK-NEXT:        "col": 8,
// CHECK-NEXT:        "file": "{{.*}}",
// CHECK-NEXT:        "line": 18
// CHECK-NEXT:       }
// CHECK-NEXT:      },
// CHECK-NEXT:      "type": {
// CHECK-NEXT:       "qualType": "const int"
// CHECK-NEXT:      },
// CHECK-NEXT:      "valueCategory": "lvalue",
// CHECK-NEXT:      "storageDuration": "full expression",
// CHECK-NEXT:      "boundToLValueRef": true,
// CHECK-NEXT:      "inner": [
// CHECK-NEXT:       {
// CHECK-NEXT:        "id": "0x{{.*}}",
// CHECK-NEXT:        "kind": "ImplicitCastExpr",
// CHECK-NEXT:        "range": {
// CHECK-NEXT:         "begin": {
// CHECK-NEXT:          "col": 8,
// CHECK-NEXT:          "file": "{{.*}}",
// CHECK-NEXT:          "line": 18
// CHECK-NEXT:         },
// CHECK-NEXT:         "end": {
// CHECK-NEXT:          "col": 8,
// CHECK-NEXT:          "file": "{{.*}}",
// CHECK-NEXT:          "line": 18
// CHECK-NEXT:         }
// CHECK-NEXT:        },
// CHECK-NEXT:        "type": {
// CHECK-NEXT:         "qualType": "const int"
// CHECK-NEXT:        },
// CHECK-NEXT:        "valueCategory": "rvalue",
// CHECK-NEXT:        "castKind": "FloatingToIntegral",
// CHECK-NEXT:        "inner": [
// CHECK-NEXT:         {
// CHECK-NEXT:          "id": "0x{{.*}}",
// CHECK-NEXT:          "kind": "ImplicitCastExpr",
// CHECK-NEXT:          "range": {
// CHECK-NEXT:           "begin": {
// CHECK-NEXT:            "col": 8,
// CHECK-NEXT:            "file": "{{.*}}",
// CHECK-NEXT:            "line": 18
// CHECK-NEXT:           },
// CHECK-NEXT:           "end": {
// CHECK-NEXT:            "col": 8,
// CHECK-NEXT:            "file": "{{.*}}",
// CHECK-NEXT:            "line": 18
// CHECK-NEXT:           }
// CHECK-NEXT:          },
// CHECK-NEXT:          "type": {
// CHECK-NEXT:           "qualType": "float"
// CHECK-NEXT:          },
// CHECK-NEXT:          "valueCategory": "rvalue",
// CHECK-NEXT:          "castKind": "LValueToRValue",
// CHECK-NEXT:          "inner": [
// CHECK-NEXT:           {
// CHECK-NEXT:            "id": "0x{{.*}}",
// CHECK-NEXT:            "kind": "DeclRefExpr",
// CHECK-NEXT:            "range": {
// CHECK-NEXT:             "begin": {
// CHECK-NEXT:              "col": 8,
// CHECK-NEXT:              "file": "{{.*}}",
// CHECK-NEXT:              "line": 18
// CHECK-NEXT:             },
// CHECK-NEXT:             "end": {
// CHECK-NEXT:              "col": 8,
// CHECK-NEXT:              "file": "{{.*}}",
// CHECK-NEXT:              "line": 18
// CHECK-NEXT:             }
// CHECK-NEXT:            },
// CHECK-NEXT:            "type": {
// CHECK-NEXT:             "qualType": "float"
// CHECK-NEXT:            },
// CHECK-NEXT:            "valueCategory": "lvalue",
// CHECK-NEXT:            "referencedDecl": {
// CHECK-NEXT:             "id": "0x{{.*}}",
// CHECK-NEXT:             "kind": "VarDecl",
// CHECK-NEXT:             "name": "f",
// CHECK-NEXT:             "type": {
// CHECK-NEXT:              "qualType": "float"
// CHECK-NEXT:             }
// CHECK-NEXT:            }
// CHECK-NEXT:           }
// CHECK-NEXT:          ]
// CHECK-NEXT:         }
// CHECK-NEXT:        ]
// CHECK-NEXT:       }
// CHECK-NEXT:      ]
// CHECK-NEXT:     }
// CHECK-NEXT:    ]
// CHECK-NEXT:   }
// CHECK-NEXT:  ]
// CHECK-NEXT: }

