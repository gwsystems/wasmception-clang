//===--- CGBlocks.cpp - Emit LLVM Code for declarations -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit blocks.
//
//===----------------------------------------------------------------------===//

#include "CGDebugInfo.h"
#include "CodeGenFunction.h"
#include "CGObjCRuntime.h"
#include "CodeGenModule.h"
#include "clang/AST/DeclObjC.h"
#include "llvm/Module.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Target/TargetData.h"
#include <algorithm>

using namespace clang;
using namespace CodeGen;

CGBlockInfo::CGBlockInfo(const BlockExpr *blockExpr, const char *N)
  : Name(N), CXXThisIndex(0), CanBeGlobal(false), NeedsCopyDispose(false),
    HasCXXObject(false), HasWeakBlockVariable(false),
    StructureType(0), Block(blockExpr) {
    
  // Skip asm prefix, if any.
  if (Name && Name[0] == '\01')
    ++Name;
}

/// Build the given block as a global block.
static llvm::Constant *buildGlobalBlock(CodeGenModule &CGM,
                                        const CGBlockInfo &blockInfo,
                                        llvm::Constant *blockFn);

/// Build the helper function to copy a block.
static llvm::Constant *buildCopyHelper(CodeGenModule &CGM,
                                       const CGBlockInfo &blockInfo) {
  return CodeGenFunction(CGM).GenerateCopyHelperFunction(blockInfo);
}

/// Build the helper function to dipose of a block.
static llvm::Constant *buildDisposeHelper(CodeGenModule &CGM,
                                          const CGBlockInfo &blockInfo) {
  return CodeGenFunction(CGM).GenerateDestroyHelperFunction(blockInfo);
}

/// Build the block descriptor constant for a block.
static llvm::Constant *buildBlockDescriptor(CodeGenModule &CGM,
                                            const CGBlockInfo &blockInfo) {
  ASTContext &C = CGM.getContext();

  const llvm::Type *ulong = CGM.getTypes().ConvertType(C.UnsignedLongTy);
  const llvm::Type *i8p = CGM.getTypes().ConvertType(C.VoidPtrTy);

  llvm::SmallVector<llvm::Constant*, 6> elements;

  // reserved
  elements.push_back(llvm::ConstantInt::get(ulong, 0));

  // Size
  // FIXME: What is the right way to say this doesn't fit?  We should give
  // a user diagnostic in that case.  Better fix would be to change the
  // API to size_t.
  elements.push_back(llvm::ConstantInt::get(ulong,
                                            blockInfo.BlockSize.getQuantity()));

  // Optional copy/dispose helpers.
  if (blockInfo.NeedsCopyDispose) {
    // copy_func_helper_decl
    elements.push_back(buildCopyHelper(CGM, blockInfo));

    // destroy_func_decl
    elements.push_back(buildDisposeHelper(CGM, blockInfo));
  }

  // Signature.  Mandatory ObjC-style method descriptor @encode sequence.
  std::string typeAtEncoding =
    CGM.getContext().getObjCEncodingForBlock(blockInfo.getBlockExpr());
  elements.push_back(llvm::ConstantExpr::getBitCast(
                          CGM.GetAddrOfConstantCString(typeAtEncoding), i8p));
  
  // GC layout.
  if (C.getLangOptions().ObjC1)
    elements.push_back(CGM.getObjCRuntime().BuildGCBlockLayout(CGM, blockInfo));
  else
    elements.push_back(llvm::Constant::getNullValue(i8p));

  llvm::Constant *init =
    llvm::ConstantStruct::get(CGM.getLLVMContext(), elements.data(),
                              elements.size(), false);

  llvm::GlobalVariable *global =
    new llvm::GlobalVariable(CGM.getModule(), init->getType(), true,
                             llvm::GlobalValue::InternalLinkage,
                             init, "__block_descriptor_tmp");

  return llvm::ConstantExpr::getBitCast(global, CGM.getBlockDescriptorType());
}

static unsigned computeBlockFlag(CodeGenModule &CGM,
                                 const BlockExpr *BE, unsigned flags) {
  QualType BPT = BE->getType();
  const FunctionType *ftype = BPT->getPointeeType()->getAs<FunctionType>();
  QualType ResultType = ftype->getResultType();
  
  CallArgList Args;
  CodeGenTypes &Types = CGM.getTypes();
  const CGFunctionInfo &FnInfo = Types.getFunctionInfo(ResultType, Args,
                                                       FunctionType::ExtInfo());
  if (CGM.ReturnTypeUsesSRet(FnInfo))
    flags |= CodeGenFunction::BLOCK_USE_STRET;
  return flags;
}

/*
  Purely notional variadic template describing the layout of a block.

  template <class _ResultType, class... _ParamTypes, class... _CaptureTypes>
  struct Block_literal {
    /// Initialized to one of:
    ///   extern void *_NSConcreteStackBlock[];
    ///   extern void *_NSConcreteGlobalBlock[];
    ///
    /// In theory, we could start one off malloc'ed by setting
    /// BLOCK_NEEDS_FREE, giving it a refcount of 1, and using
    /// this isa:
    ///   extern void *_NSConcreteMallocBlock[];
    struct objc_class *isa;

    /// These are the flags (with corresponding bit number) that the
    /// compiler is actually supposed to know about.
    ///  25. BLOCK_HAS_COPY_DISPOSE - indicates that the block
    ///   descriptor provides copy and dispose helper functions
    ///  26. BLOCK_HAS_CXX_OBJ - indicates that there's a captured
    ///   object with a nontrivial destructor or copy constructor
    ///  28. BLOCK_IS_GLOBAL - indicates that the block is allocated
    ///   as global memory
    ///  29. BLOCK_USE_STRET - indicates that the block function
    ///   uses stret, which objc_msgSend needs to know about
    ///  30. BLOCK_HAS_SIGNATURE - indicates that the block has an
    ///   @encoded signature string
    /// And we're not supposed to manipulate these:
    ///  24. BLOCK_NEEDS_FREE - indicates that the block has been moved
    ///   to malloc'ed memory
    ///  27. BLOCK_IS_GC - indicates that the block has been moved to
    ///   to GC-allocated memory
    /// Additionally, the bottom 16 bits are a reference count which
    /// should be zero on the stack.
    int flags;

    /// Reserved;  should be zero-initialized.
    int reserved;

    /// Function pointer generated from block literal.
    _ResultType (*invoke)(Block_literal *, _ParamTypes...);

    /// Block description metadata generated from block literal.
    struct Block_descriptor *block_descriptor;

    /// Captured values follow.
    _CapturesTypes captures...;
  };
 */

/// The number of fields in a block header.
const unsigned BlockHeaderSize = 5;

namespace {
  /// A chunk of data that we actually have to capture in the block.
  struct BlockLayoutChunk {
    CharUnits Alignment;
    CharUnits Size;
    const BlockDecl::Capture *Capture; // null for 'this'
    const llvm::Type *Type;

    BlockLayoutChunk(CharUnits align, CharUnits size,
                     const BlockDecl::Capture *capture,
                     const llvm::Type *type)
      : Alignment(align), Size(size), Capture(capture), Type(type) {}

    /// Tell the block info that this chunk has the given field index.
    void setIndex(CGBlockInfo &info, unsigned index) {
      if (!Capture)
        info.CXXThisIndex = index;
      else
        info.Captures[Capture->getVariable()]
          = CGBlockInfo::Capture::makeIndex(index);
    }
  };

  /// Order by descending alignment.
  bool operator<(const BlockLayoutChunk &left, const BlockLayoutChunk &right) {
    return left.Alignment > right.Alignment;
  }
}

/// It is illegal to modify a const object after initialization.
/// Therefore, if a const object has a constant initializer, we don't
/// actually need to keep storage for it in the block; we'll just
/// rematerialize it at the start of the block function.  This is
/// acceptable because we make no promises about address stability of
/// captured variables.
static llvm::Constant *tryCaptureAsConstant(CodeGenModule &CGM,
                                            const VarDecl *var) {
  QualType type = var->getType();

  // We can only do this if the variable is const.
  if (!type.isConstQualified()) return 0;

  // Furthermore, in C++ we can't do this for classes.  TODO: we might
  // actually be able to get away with it for classes with a trivial
  // destructor and a trivial copy constructor and no mutable fields.
  if (CGM.getLangOptions().CPlusPlus &&
      type->getBaseElementTypeUnsafe()->isRecordType())
    return 0;

  // If the variable doesn't have any initializer (shouldn't this be
  // invalid?), it's not clear what we should do.  Maybe capture as
  // zero?
  const Expr *init = var->getInit();
  if (!init) return 0;

  return CGM.EmitConstantExpr(init, var->getType());
}

/// Get the low bit of a nonzero character count.  This is the
/// alignment of the nth byte if the 0th byte is universally aligned.
static CharUnits getLowBit(CharUnits v) {
  return CharUnits::fromQuantity(v.getQuantity() & (~v.getQuantity() + 1));
}

static void initializeForBlockHeader(CodeGenModule &CGM, CGBlockInfo &info,
                                std::vector<const llvm::Type*> &elementTypes) {
  ASTContext &C = CGM.getContext();

  // The header is basically a 'struct { void *; int; int; void *; void *; }'.
  CharUnits ptrSize, ptrAlign, intSize, intAlign;
  llvm::tie(ptrSize, ptrAlign) = C.getTypeInfoInChars(C.VoidPtrTy);
  llvm::tie(intSize, intAlign) = C.getTypeInfoInChars(C.IntTy);

  // Are there crazy embedded platforms where this isn't true?
  assert(intSize <= ptrSize && "layout assumptions horribly violated");

  CharUnits headerSize = ptrSize;
  if (2 * intSize < ptrAlign) headerSize += ptrSize;
  else headerSize += 2 * intSize;
  headerSize += 2 * ptrSize;

  info.BlockAlign = ptrAlign;
  info.BlockSize = headerSize;

  assert(elementTypes.empty());
  const llvm::Type *i8p = CGM.getTypes().ConvertType(C.VoidPtrTy);
  const llvm::Type *intTy = CGM.getTypes().ConvertType(C.IntTy);
  elementTypes.push_back(i8p);
  elementTypes.push_back(intTy);
  elementTypes.push_back(intTy);
  elementTypes.push_back(i8p);
  elementTypes.push_back(CGM.getBlockDescriptorType());

  assert(elementTypes.size() == BlockHeaderSize);
}

/// Compute the layout of the given block.  Attempts to lay the block
/// out with minimal space requirements.
static void computeBlockInfo(CodeGenModule &CGM, CGBlockInfo &info) {
  ASTContext &C = CGM.getContext();
  const BlockDecl *block = info.getBlockDecl();

  std::vector<const llvm::Type*> elementTypes;
  initializeForBlockHeader(CGM, info, elementTypes);

  if (!block->hasCaptures()) {
    info.StructureType =
      llvm::StructType::get(CGM.getLLVMContext(), elementTypes, true);
    info.CanBeGlobal = true;
    return;
  }

  // Collect the layout chunks.
  llvm::SmallVector<BlockLayoutChunk, 16> layout;
  layout.reserve(block->capturesCXXThis() +
                 (block->capture_end() - block->capture_begin()));

  CharUnits maxFieldAlign;

  // First, 'this'.
  if (block->capturesCXXThis()) {
    const DeclContext *DC = block->getDeclContext();
    for (; isa<BlockDecl>(DC); DC = cast<BlockDecl>(DC)->getDeclContext())
      ;
    QualType thisType = cast<CXXMethodDecl>(DC)->getThisType(C);

    const llvm::Type *llvmType = CGM.getTypes().ConvertType(thisType);
    std::pair<CharUnits,CharUnits> tinfo
      = CGM.getContext().getTypeInfoInChars(thisType);
    maxFieldAlign = std::max(maxFieldAlign, tinfo.second);

    layout.push_back(BlockLayoutChunk(tinfo.second, tinfo.first, 0, llvmType));
  }

  // Next, all the block captures.
  for (BlockDecl::capture_const_iterator ci = block->capture_begin(),
         ce = block->capture_end(); ci != ce; ++ci) {
    const VarDecl *variable = ci->getVariable();

    if (ci->isByRef()) {
      // We have to copy/dispose of the __block reference.
      info.NeedsCopyDispose = true;

      // Also note that it's weak for GC purposes.
      if (variable->getType().isObjCGCWeak())
        info.HasWeakBlockVariable = true;

      // Just use void* instead of a pointer to the byref type.
      QualType byRefPtrTy = C.VoidPtrTy;

      const llvm::Type *llvmType = CGM.getTypes().ConvertType(byRefPtrTy);
      std::pair<CharUnits,CharUnits> tinfo
        = CGM.getContext().getTypeInfoInChars(byRefPtrTy);
      maxFieldAlign = std::max(maxFieldAlign, tinfo.second);

      layout.push_back(BlockLayoutChunk(tinfo.second, tinfo.first,
                                        &*ci, llvmType));
      continue;
    }

    // Otherwise, build a layout chunk with the size and alignment of
    // the declaration.
    if (llvm::Constant *constant = tryCaptureAsConstant(CGM, variable)) {
      info.Captures[variable] = CGBlockInfo::Capture::makeConstant(constant);
      continue;
    }

    // Block pointers require copy/dispose.
    if (variable->getType()->isBlockPointerType()) {
      info.NeedsCopyDispose = true;

    // So do Objective-C pointers.
    } else if (variable->getType()->isObjCObjectPointerType() ||
               C.isObjCNSObjectType(variable->getType())) {
      info.NeedsCopyDispose = true;

    // So do types that require non-trivial copy construction.
    } else if (ci->hasCopyExpr()) {
      info.NeedsCopyDispose = true;
      info.HasCXXObject = true;

    // And so do types with destructors.
    } else if (CGM.getLangOptions().CPlusPlus) {
      if (const CXXRecordDecl *record =
            variable->getType()->getAsCXXRecordDecl()) {
        if (!record->hasTrivialDestructor()) {
          info.HasCXXObject = true;
          info.NeedsCopyDispose = true;
        }
      }
    }

    CharUnits size = C.getTypeSizeInChars(variable->getType());
    CharUnits align = C.getDeclAlign(variable);
    maxFieldAlign = std::max(maxFieldAlign, align);

    const llvm::Type *llvmType =
      CGM.getTypes().ConvertTypeForMem(variable->getType());

    layout.push_back(BlockLayoutChunk(align, size, &*ci, llvmType));
  }

  // If that was everything, we're done here.
  if (layout.empty()) {
    info.StructureType =
      llvm::StructType::get(CGM.getLLVMContext(), elementTypes, true);
    info.CanBeGlobal = true;
    return;
  }

  // Sort the layout by alignment.  We have to use a stable sort here
  // to get reproducible results.  There should probably be an
  // llvm::array_pod_stable_sort.
  std::stable_sort(layout.begin(), layout.end());

  CharUnits &blockSize = info.BlockSize;
  info.BlockAlign = std::max(maxFieldAlign, info.BlockAlign);

  // Assuming that the first byte in the header is maximally aligned,
  // get the alignment of the first byte following the header.
  CharUnits endAlign = getLowBit(blockSize);

  // If the end of the header isn't satisfactorily aligned for the
  // maximum thing, look for things that are okay with the header-end
  // alignment, and keep appending them until we get something that's
  // aligned right.  This algorithm is only guaranteed optimal if
  // that condition is satisfied at some point; otherwise we can get
  // things like:
  //   header                 // next byte has alignment 4
  //   something_with_size_5; // next byte has alignment 1
  //   something_with_alignment_8;
  // which has 7 bytes of padding, as opposed to the naive solution
  // which might have less (?).
  if (endAlign < maxFieldAlign) {
    llvm::SmallVectorImpl<BlockLayoutChunk>::iterator
      li = layout.begin() + 1, le = layout.end();

    // Look for something that the header end is already
    // satisfactorily aligned for.
    for (; li != le && endAlign < li->Alignment; ++li)
      ;

    // If we found something that's naturally aligned for the end of
    // the header, keep adding things...
    if (li != le) {
      llvm::SmallVectorImpl<BlockLayoutChunk>::iterator first = li;
      for (; li != le; ++li) {
        assert(endAlign >= li->Alignment);

        li->setIndex(info, elementTypes.size());
        elementTypes.push_back(li->Type);
        blockSize += li->Size;
        endAlign = getLowBit(blockSize);

        // ...until we get to the alignment of the maximum field.
        if (endAlign >= maxFieldAlign)
          break;
      }

      // Don't re-append everything we just appended.
      layout.erase(first, li);
    }
  }

  // At this point, we just have to add padding if the end align still
  // isn't aligned right.
  if (endAlign < maxFieldAlign) {
    CharUnits padding = maxFieldAlign - endAlign;

    const llvm::Type *i8 = llvm::IntegerType::get(CGM.getLLVMContext(), 8);
    elementTypes.push_back(llvm::ArrayType::get(i8, padding.getQuantity()));
    blockSize += padding;

    endAlign = getLowBit(blockSize);
    assert(endAlign >= maxFieldAlign);
  }

  // Slam everything else on now.  This works because they have
  // strictly decreasing alignment and we expect that size is always a
  // multiple of alignment.
  for (llvm::SmallVectorImpl<BlockLayoutChunk>::iterator
         li = layout.begin(), le = layout.end(); li != le; ++li) {
    assert(endAlign >= li->Alignment);
    li->setIndex(info, elementTypes.size());
    elementTypes.push_back(li->Type);
    blockSize += li->Size;
    endAlign = getLowBit(blockSize);
  }

  info.StructureType =
    llvm::StructType::get(CGM.getLLVMContext(), elementTypes, true);
}

/// Emit a block literal expression in the current function.
llvm::Value *CodeGenFunction::EmitBlockLiteral(const BlockExpr *blockExpr) {
  std::string Name = CurFn->getName();
  CGBlockInfo blockInfo(blockExpr, Name.c_str());

  // Compute information about the layout, etc., of this block.
  computeBlockInfo(CGM, blockInfo);

  // Using that metadata, generate the actual block function.
  llvm::Constant *blockFn
    = CodeGenFunction(CGM).GenerateBlockFunction(CurGD, blockInfo,
                                                 CurFuncDecl, LocalDeclMap);
  blockFn = llvm::ConstantExpr::getBitCast(blockFn, PtrToInt8Ty);

  // If there is nothing to capture, we can emit this as a global block.
  if (blockInfo.CanBeGlobal)
    return buildGlobalBlock(CGM, blockInfo, blockFn);

  // Otherwise, we have to emit this as a local block.

  llvm::Constant *isa = CGM.getNSConcreteStackBlock();
  isa = llvm::ConstantExpr::getBitCast(isa, PtrToInt8Ty);

  // Build the block descriptor.
  llvm::Constant *descriptor = buildBlockDescriptor(CGM, blockInfo);

  const llvm::Type *intTy = ConvertType(getContext().IntTy);

  llvm::AllocaInst *blockAddr =
    CreateTempAlloca(blockInfo.StructureType, "block");
  blockAddr->setAlignment(blockInfo.BlockAlign.getQuantity());

  // Compute the initial on-stack block flags.
  unsigned int flags = BLOCK_HAS_SIGNATURE;
  if (blockInfo.NeedsCopyDispose) flags |= BLOCK_HAS_COPY_DISPOSE;
  if (blockInfo.HasCXXObject) flags |= BLOCK_HAS_CXX_OBJ;
  flags = computeBlockFlag(CGM, blockInfo.getBlockExpr(), flags);

  // Initialize the block literal.
  Builder.CreateStore(isa, Builder.CreateStructGEP(blockAddr, 0, "block.isa"));
  Builder.CreateStore(llvm::ConstantInt::get(intTy, flags),
                      Builder.CreateStructGEP(blockAddr, 1, "block.flags"));
  Builder.CreateStore(llvm::ConstantInt::get(intTy, 0),
                      Builder.CreateStructGEP(blockAddr, 2, "block.reserved"));
  Builder.CreateStore(blockFn, Builder.CreateStructGEP(blockAddr, 3,
                                                       "block.invoke"));
  Builder.CreateStore(descriptor, Builder.CreateStructGEP(blockAddr, 4,
                                                          "block.descriptor"));

  // Finally, capture all the values into the block.
  const BlockDecl *blockDecl = blockInfo.getBlockDecl();

  // First, 'this'.
  if (blockDecl->capturesCXXThis()) {
    llvm::Value *addr = Builder.CreateStructGEP(blockAddr,
                                                blockInfo.CXXThisIndex,
                                                "block.captured-this.addr");
    Builder.CreateStore(LoadCXXThis(), addr);
  }

  // Next, captured variables.
  for (BlockDecl::capture_const_iterator ci = blockDecl->capture_begin(),
         ce = blockDecl->capture_end(); ci != ce; ++ci) {
    const VarDecl *variable = ci->getVariable();
    const CGBlockInfo::Capture &capture = blockInfo.getCapture(variable);

    // Ignore constant captures.
    if (capture.isConstant()) continue;

    QualType type = variable->getType();

    // This will be a [[type]]*, except that a byref entry will just be
    // an i8**.
    llvm::Value *blockField =
      Builder.CreateStructGEP(blockAddr, capture.getIndex(),
                              "block.captured");

    // Compute the address of the thing we're going to move into the
    // block literal.
    llvm::Value *src;
    if (ci->isNested()) {
      // We need to use the capture from the enclosing block.
      const CGBlockInfo::Capture &enclosingCapture =
        BlockInfo->getCapture(variable);

      // This is a [[type]]*, except that a byref entry wil just be an i8**.
      src = Builder.CreateStructGEP(LoadBlockStruct(),
                                    enclosingCapture.getIndex(),
                                    "block.capture.addr");
    } else {
      // This is a [[type]]*.
      src = LocalDeclMap[variable];
    }

    // For byrefs, we just write the pointer to the byref struct into
    // the block field.  There's no need to chase the forwarding
    // pointer at this point, since we're building something that will
    // live a shorter life than the stack byref anyway.
    if (ci->isByRef()) {
      // Get an i8* that points to the byref struct.
      if (ci->isNested())
        src = Builder.CreateLoad(src, "byref.capture");
      else
        src = Builder.CreateBitCast(src, PtrToInt8Ty);

      // Write that i8* into the capture field.
      Builder.CreateStore(src, blockField);

    // If we have a copy constructor, evaluate that into the block field.
    } else if (const Expr *copyExpr = ci->getCopyExpr()) {
      EmitSynthesizedCXXCopyCtor(blockField, src, copyExpr);

    // If it's a reference variable, copy the reference into the block field.
    } else if (type->isReferenceType()) {
      Builder.CreateStore(Builder.CreateLoad(src, "ref.val"), blockField);

    // Otherwise, fake up a POD copy into the block field.
    } else {
      // We use one of these or the other depending on whether the
      // reference is nested.
      DeclRefExpr notNested(const_cast<VarDecl*>(variable), type, VK_LValue,
                            SourceLocation());
      BlockDeclRefExpr nested(const_cast<VarDecl*>(variable), type,
                              VK_LValue, SourceLocation(), /*byref*/ false);

      Expr *declRef = 
        (ci->isNested() ? static_cast<Expr*>(&nested) : &notNested);

      ImplicitCastExpr l2r(ImplicitCastExpr::OnStack, type, CK_LValueToRValue,
                           declRef, VK_RValue);
      EmitAnyExprToMem(&l2r, blockField, /*volatile*/ false, /*init*/ true);
    }

    // Push a destructor if necessary.  The semantics for when this
    // actually gets run are really obscure.
    if (!ci->isByRef() && CGM.getLangOptions().CPlusPlus)
      PushDestructorCleanup(type, blockField);
  }

  // Cast to the converted block-pointer type, which happens (somewhat
  // unfortunately) to be a pointer to function type.
  llvm::Value *result =
    Builder.CreateBitCast(blockAddr,
                          ConvertType(blockInfo.getBlockExpr()->getType()));

  // We must call objc_read_weak on the block literal itself if it closes
  // on any __weak __block variables.  For some reason.
  if (blockInfo.HasWeakBlockVariable) {
    const llvm::Type *OrigTy = result->getType();

    // Must cast argument to id*
    const llvm::Type *ObjectPtrTy = 
      ConvertType(CGM.getContext().getObjCIdType());
    const llvm::Type *PtrObjectPtrTy = 
      llvm::PointerType::getUnqual(ObjectPtrTy);
    result = Builder.CreateBitCast(result, PtrObjectPtrTy);
    result = CGM.getObjCRuntime().EmitObjCWeakRead(*this, result);

    // Cast back to the original type.
    result = Builder.CreateBitCast(result, OrigTy);
  }
  return result;
}


const llvm::Type *BlockModule::getBlockDescriptorType() {
  if (BlockDescriptorType)
    return BlockDescriptorType;

  const llvm::Type *UnsignedLongTy =
    getTypes().ConvertType(getContext().UnsignedLongTy);

  // struct __block_descriptor {
  //   unsigned long reserved;
  //   unsigned long block_size;
  //
  //   // later, the following will be added
  //
  //   struct {
  //     void (*copyHelper)();
  //     void (*copyHelper)();
  //   } helpers;                // !!! optional
  //
  //   const char *signature;   // the block signature
  //   const char *layout;      // reserved
  // };
  BlockDescriptorType = llvm::StructType::get(UnsignedLongTy->getContext(),
                                              UnsignedLongTy,
                                              UnsignedLongTy,
                                              NULL);

  getModule().addTypeName("struct.__block_descriptor",
                          BlockDescriptorType);

  // Now form a pointer to that.
  BlockDescriptorType = llvm::PointerType::getUnqual(BlockDescriptorType);
  return BlockDescriptorType;
}

const llvm::Type *BlockModule::getGenericBlockLiteralType() {
  if (GenericBlockLiteralType)
    return GenericBlockLiteralType;

  const llvm::Type *BlockDescPtrTy = getBlockDescriptorType();

  const llvm::IntegerType *IntTy = cast<llvm::IntegerType>(
    getTypes().ConvertType(getContext().IntTy));

  // struct __block_literal_generic {
  //   void *__isa;
  //   int __flags;
  //   int __reserved;
  //   void (*__invoke)(void *);
  //   struct __block_descriptor *__descriptor;
  // };
  GenericBlockLiteralType = llvm::StructType::get(IntTy->getContext(),
                                                  PtrToInt8Ty,
                                                  IntTy,
                                                  IntTy,
                                                  PtrToInt8Ty,
                                                  BlockDescPtrTy,
                                                  NULL);

  getModule().addTypeName("struct.__block_literal_generic",
                          GenericBlockLiteralType);

  return GenericBlockLiteralType;
}


RValue CodeGenFunction::EmitBlockCallExpr(const CallExpr* E, 
                                          ReturnValueSlot ReturnValue) {
  const BlockPointerType *BPT =
    E->getCallee()->getType()->getAs<BlockPointerType>();

  llvm::Value *Callee = EmitScalarExpr(E->getCallee());

  // Get a pointer to the generic block literal.
  const llvm::Type *BlockLiteralTy =
    llvm::PointerType::getUnqual(CGM.getGenericBlockLiteralType());

  // Bitcast the callee to a block literal.
  llvm::Value *BlockLiteral =
    Builder.CreateBitCast(Callee, BlockLiteralTy, "block.literal");

  // Get the function pointer from the literal.
  llvm::Value *FuncPtr = Builder.CreateStructGEP(BlockLiteral, 3, "tmp");

  BlockLiteral =
    Builder.CreateBitCast(BlockLiteral,
                          llvm::Type::getInt8PtrTy(VMContext),
                          "tmp");

  // Add the block literal.
  QualType VoidPtrTy = getContext().getPointerType(getContext().VoidTy);
  CallArgList Args;
  Args.push_back(std::make_pair(RValue::get(BlockLiteral), VoidPtrTy));

  QualType FnType = BPT->getPointeeType();

  // And the rest of the arguments.
  EmitCallArgs(Args, FnType->getAs<FunctionProtoType>(),
               E->arg_begin(), E->arg_end());

  // Load the function.
  llvm::Value *Func = Builder.CreateLoad(FuncPtr, "tmp");

  const FunctionType *FuncTy = FnType->getAs<FunctionType>();
  QualType ResultType = FuncTy->getResultType();

  const CGFunctionInfo &FnInfo =
    CGM.getTypes().getFunctionInfo(ResultType, Args,
                                   FuncTy->getExtInfo());

  // Cast the function pointer to the right type.
  const llvm::Type *BlockFTy =
    CGM.getTypes().GetFunctionType(FnInfo, false);

  const llvm::Type *BlockFTyPtr = llvm::PointerType::getUnqual(BlockFTy);
  Func = Builder.CreateBitCast(Func, BlockFTyPtr);

  // And call the block.
  return EmitCall(FnInfo, Func, ReturnValue, Args);
}

llvm::Value *CodeGenFunction::GetAddrOfBlockDecl(const VarDecl *variable,
                                                 bool isByRef) {
  assert(BlockInfo && "evaluating block ref without block information?");
  const CGBlockInfo::Capture &capture = BlockInfo->getCapture(variable);

  // Handle constant captures.
  if (capture.isConstant()) return LocalDeclMap[variable];

  llvm::Value *addr =
    Builder.CreateStructGEP(LoadBlockStruct(), capture.getIndex(),
                            "block.capture.addr");

  if (isByRef) {
    // addr should be a void** right now.  Load, then cast the result
    // to byref*.

    addr = Builder.CreateLoad(addr);
    const llvm::PointerType *byrefPointerType
      = llvm::PointerType::get(BuildByRefType(variable), 0);
    addr = Builder.CreateBitCast(addr, byrefPointerType,
                                 "byref.addr");

    // Follow the forwarding pointer.
    addr = Builder.CreateStructGEP(addr, 1, "byref.forwarding");
    addr = Builder.CreateLoad(addr, "byref.addr.forwarded");

    // Cast back to byref* and GEP over to the actual object.
    addr = Builder.CreateBitCast(addr, byrefPointerType);
    addr = Builder.CreateStructGEP(addr, getByRefValueLLVMField(variable), 
                                   variable->getNameAsString());
  }

  if (variable->getType()->isReferenceType())
    addr = Builder.CreateLoad(addr, "ref.tmp");

  return addr;
}

llvm::Constant *
BlockModule::GetAddrOfGlobalBlock(const BlockExpr *blockExpr,
                                  const char *name) {
  CGBlockInfo blockInfo(blockExpr, name);

  // Compute information about the layout, etc., of this block.
  computeBlockInfo(CGM, blockInfo);

  // Using that metadata, generate the actual block function.
  llvm::Constant *blockFn;
  {
    llvm::DenseMap<const Decl*, llvm::Value*> LocalDeclMap;
    blockFn = CodeGenFunction(CGM).GenerateBlockFunction(GlobalDecl(),
                                                         blockInfo,
                                                         0, LocalDeclMap);
  }
  blockFn = llvm::ConstantExpr::getBitCast(blockFn, PtrToInt8Ty);

  return buildGlobalBlock(CGM, blockInfo, blockFn);
}

static llvm::Constant *buildGlobalBlock(CodeGenModule &CGM,
                                        const CGBlockInfo &blockInfo,
                                        llvm::Constant *blockFn) {
  assert(blockInfo.CanBeGlobal);

  // Generate the constants for the block literal initializer.
  llvm::Constant *fields[BlockHeaderSize];

  // isa
  fields[0] = CGM.getNSConcreteGlobalBlock();

  // __flags
  unsigned flags = computeBlockFlag(CGM, blockInfo.getBlockExpr(),
                                    BlockBase::BLOCK_IS_GLOBAL |
                                    BlockBase::BLOCK_HAS_SIGNATURE);
  const llvm::Type *intTy = CGM.getTypes().ConvertType(CGM.getContext().IntTy);
  fields[1] = llvm::ConstantInt::get(intTy, flags);

  // Reserved
  fields[2] = llvm::Constant::getNullValue(intTy);

  // Function
  fields[3] = blockFn;

  // Descriptor
  fields[4] = buildBlockDescriptor(CGM, blockInfo);

  llvm::Constant *init =
    llvm::ConstantStruct::get(CGM.getLLVMContext(), fields, BlockHeaderSize,
                              /*packed*/ false);

  llvm::GlobalVariable *literal =
    new llvm::GlobalVariable(CGM.getModule(),
                             init->getType(),
                             /*constant*/ true,
                             llvm::GlobalVariable::InternalLinkage,
                             init,
                             "__block_literal_global");
  literal->setAlignment(blockInfo.BlockAlign.getQuantity());

  // Return a constant of the appropriately-casted type.
  const llvm::Type *requiredType =
    CGM.getTypes().ConvertType(blockInfo.getBlockExpr()->getType());
  return llvm::ConstantExpr::getBitCast(literal, requiredType);
}

llvm::Function *
CodeGenFunction::GenerateBlockFunction(GlobalDecl GD,
                                       const CGBlockInfo &blockInfo,
                                       const Decl *outerFnDecl,
                                       const DeclMapTy &ldm) {
  const BlockDecl *blockDecl = blockInfo.getBlockDecl();

  DebugInfo = CGM.getDebugInfo();
  BlockInfo = &blockInfo;

  // Arrange for local static and local extern declarations to appear
  // to be local to this function as well, in case they're directly
  // referenced in a block.
  for (DeclMapTy::const_iterator i = ldm.begin(), e = ldm.end(); i != e; ++i) {
    const VarDecl *var = dyn_cast<VarDecl>(i->first);
    if (var && !var->hasLocalStorage())
      LocalDeclMap[var] = i->second;
  }

  // Begin building the function declaration.

  // Build the argument list.
  FunctionArgList args;

  // The first argument is the block pointer.  Just take it as a void*
  // and cast it later.
  QualType selfTy = getContext().VoidPtrTy;
  IdentifierInfo *II = &CGM.getContext().Idents.get(".block_descriptor");

  // FIXME: this leaks, and we only need it very temporarily.
  ImplicitParamDecl *selfDecl =
    ImplicitParamDecl::Create(getContext(),
                              const_cast<BlockDecl*>(blockDecl),
                              SourceLocation(), II, selfTy);
  args.push_back(std::make_pair(selfDecl, selfTy));

  // Now add the rest of the parameters.
  for (BlockDecl::param_const_iterator i = blockDecl->param_begin(),
       e = blockDecl->param_end(); i != e; ++i)
    args.push_back(std::make_pair(*i, (*i)->getType()));

  // Create the function declaration.
  const FunctionProtoType *fnType =
    cast<FunctionProtoType>(blockInfo.getBlockExpr()->getFunctionType());
  const CGFunctionInfo &fnInfo =
    CGM.getTypes().getFunctionInfo(fnType->getResultType(), args,
                                   fnType->getExtInfo());
  const llvm::FunctionType *fnLLVMType =
    CGM.getTypes().GetFunctionType(fnInfo, fnType->isVariadic());

  MangleBuffer name;
  CGM.getBlockMangledName(GD, name, blockDecl);
  llvm::Function *fn =
    llvm::Function::Create(fnLLVMType, llvm::GlobalValue::InternalLinkage, 
                           name.getString(), &CGM.getModule());
  CGM.SetInternalFunctionAttributes(blockDecl, fn, fnInfo);

  // Begin generating the function.
  StartFunction(blockDecl, fnType->getResultType(), fn, args,
                blockInfo.getBlockExpr()->getBody()->getLocEnd());
  CurFuncDecl = outerFnDecl; // StartFunction sets this to blockDecl

  // Okay.  Undo some of what StartFunction did.  We really don't need
  // an alloca for the block address;  in theory we could remove it,
  // but that might do unpleasant things to debug info.
  llvm::AllocaInst *blockAddrAlloca
    = cast<llvm::AllocaInst>(LocalDeclMap[selfDecl]);
  llvm::Value *blockAddr = Builder.CreateLoad(blockAddrAlloca);
  BlockPointer = Builder.CreateBitCast(blockAddr,
                                       blockInfo.StructureType->getPointerTo(),
                                       "block");

  // If we have a C++ 'this' reference, go ahead and force it into
  // existence now.
  if (blockDecl->capturesCXXThis()) {
    llvm::Value *addr = Builder.CreateStructGEP(BlockPointer,
                                                blockInfo.CXXThisIndex,
                                                "block.captured-this");
    CXXThisValue = Builder.CreateLoad(addr, "this");
  }

  // LoadObjCSelf() expects there to be an entry for 'self' in LocalDeclMap;
  // appease it.
  if (const ObjCMethodDecl *method
        = dyn_cast_or_null<ObjCMethodDecl>(CurFuncDecl)) {
    const VarDecl *self = method->getSelfDecl();

    // There might not be a capture for 'self', but if there is...
    if (blockInfo.Captures.count(self)) {
      const CGBlockInfo::Capture &capture = blockInfo.getCapture(self);
      llvm::Value *selfAddr = Builder.CreateStructGEP(BlockPointer,
                                                      capture.getIndex(),
                                                      "block.captured-self");
      LocalDeclMap[self] = selfAddr;
    }
  }

  // Also force all the constant captures.
  for (BlockDecl::capture_const_iterator ci = blockDecl->capture_begin(),
         ce = blockDecl->capture_end(); ci != ce; ++ci) {
    const VarDecl *variable = ci->getVariable();
    const CGBlockInfo::Capture &capture = blockInfo.getCapture(variable);
    if (!capture.isConstant()) continue;

    unsigned align = getContext().getDeclAlign(variable).getQuantity();

    llvm::AllocaInst *alloca =
      CreateMemTemp(variable->getType(), "block.captured-const");
    alloca->setAlignment(align);

    Builder.CreateStore(capture.getConstant(), alloca, align);

    LocalDeclMap[variable] = alloca;
  }

  // Save a spot to insert the debug information for all the BlockDeclRefDecls.
  llvm::BasicBlock *entry = Builder.GetInsertBlock();
  llvm::BasicBlock::iterator entry_ptr = Builder.GetInsertPoint();
  --entry_ptr;

  EmitStmt(blockDecl->getBody());

  // Remember where we were...
  llvm::BasicBlock *resume = Builder.GetInsertBlock();

  // Go back to the entry.
  ++entry_ptr;
  Builder.SetInsertPoint(entry, entry_ptr);

  // Emit debug information for all the BlockDeclRefDecls.
  // FIXME: also for 'this'
  if (CGDebugInfo *DI = getDebugInfo()) {
    for (BlockDecl::capture_const_iterator ci = blockDecl->capture_begin(),
           ce = blockDecl->capture_end(); ci != ce; ++ci) {
      const VarDecl *variable = ci->getVariable();
      DI->setLocation(variable->getLocation());

      const CGBlockInfo::Capture &capture = blockInfo.getCapture(variable);
      if (capture.isConstant()) {
        DI->EmitDeclareOfAutoVariable(variable, LocalDeclMap[variable],
                                      Builder);
        continue;
      }

      DI->EmitDeclareOfBlockDeclRefVariable(variable, blockAddrAlloca,
                                            Builder, blockInfo);
    }
  }

  // And resume where we left off.
  if (resume == 0)
    Builder.ClearInsertionPoint();
  else
    Builder.SetInsertPoint(resume);

  FinishFunction(cast<CompoundStmt>(blockDecl->getBody())->getRBracLoc());

  return fn;
}

/*
    notes.push_back(HelperInfo());
    HelperInfo &note = notes.back();
    note.index = capture.getIndex();
    note.RequiresCopying = (ci->hasCopyExpr() || BlockRequiresCopying(type));
    note.cxxbar_import = ci->getCopyExpr();

    if (ci->isByRef()) {
      note.flag = BLOCK_FIELD_IS_BYREF;
      if (type.isObjCGCWeak())
        note.flag |= BLOCK_FIELD_IS_WEAK;
    } else if (type->isBlockPointerType()) {
      note.flag = BLOCK_FIELD_IS_BLOCK;
    } else {
      note.flag = BLOCK_FIELD_IS_OBJECT;
    }
 */





llvm::Constant *
BlockFunction::GenerateCopyHelperFunction(const CGBlockInfo &blockInfo) {
  ASTContext &C = getContext();

  FunctionArgList args;
  // FIXME: This leaks
  ImplicitParamDecl *dstDecl =
    ImplicitParamDecl::Create(C, 0, SourceLocation(), 0, C.VoidPtrTy);
  args.push_back(std::make_pair(dstDecl, dstDecl->getType()));
  ImplicitParamDecl *srcDecl =
    ImplicitParamDecl::Create(C, 0, SourceLocation(), 0, C.VoidPtrTy);
  args.push_back(std::make_pair(srcDecl, srcDecl->getType()));

  const CGFunctionInfo &FI =
      CGM.getTypes().getFunctionInfo(C.VoidTy, args, FunctionType::ExtInfo());

  // FIXME: it would be nice if these were mergeable with things with
  // identical semantics.
  const llvm::FunctionType *LTy = CGM.getTypes().GetFunctionType(FI, false);

  llvm::Function *Fn =
    llvm::Function::Create(LTy, llvm::GlobalValue::InternalLinkage,
                           "__copy_helper_block_", &CGM.getModule());

  IdentifierInfo *II
    = &CGM.getContext().Idents.get("__copy_helper_block_");

  FunctionDecl *FD = FunctionDecl::Create(C,
                                          C.getTranslationUnitDecl(),
                                          SourceLocation(), II, C.VoidTy, 0,
                                          SC_Static,
                                          SC_None,
                                          false,
                                          true);
  CGF.StartFunction(FD, C.VoidTy, Fn, args, SourceLocation());

  const llvm::Type *structPtrTy = blockInfo.StructureType->getPointerTo();

  llvm::Value *src = CGF.GetAddrOfLocalVar(srcDecl);
  src = CGF.Builder.CreateLoad(src);
  src = CGF.Builder.CreateBitCast(src, structPtrTy, "block.source");

  llvm::Value *dst = CGF.GetAddrOfLocalVar(dstDecl);
  dst = CGF.Builder.CreateLoad(dst);
  dst = CGF.Builder.CreateBitCast(dst, structPtrTy, "block.dest");

  const BlockDecl *blockDecl = blockInfo.getBlockDecl();

  for (BlockDecl::capture_const_iterator ci = blockDecl->capture_begin(),
         ce = blockDecl->capture_end(); ci != ce; ++ci) {
    const VarDecl *variable = ci->getVariable();
    QualType type = variable->getType();

    const CGBlockInfo::Capture &capture = blockInfo.getCapture(variable);
    if (capture.isConstant()) continue;

    const Expr *copyExpr = ci->getCopyExpr();
    unsigned flags = 0;

    if (copyExpr) {
      assert(!ci->isByRef());
      // don't bother computing flags
    } else if (ci->isByRef()) {
      flags = BLOCK_FIELD_IS_BYREF;
      if (type.isObjCGCWeak()) flags |= BLOCK_FIELD_IS_WEAK;
    } else if (type->isBlockPointerType()) {
      flags = BLOCK_FIELD_IS_BLOCK;
    } else if (type->isObjCObjectPointerType() || C.isObjCNSObjectType(type)) {
      flags = BLOCK_FIELD_IS_OBJECT;
    }

    if (!copyExpr && !flags) continue;

    unsigned index = capture.getIndex();
    llvm::Value *srcField = CGF.Builder.CreateStructGEP(src, index);
    llvm::Value *dstField = CGF.Builder.CreateStructGEP(dst, index);

    // If there's an explicit copy expression, we do that.
    if (copyExpr) {
      CGF.EmitSynthesizedCXXCopyCtor(dstField, srcField, copyExpr);
    } else {
      llvm::Value *srcValue = Builder.CreateLoad(srcField, "blockcopy.src");
      srcValue = Builder.CreateBitCast(srcValue, PtrToInt8Ty);
      llvm::Value *dstAddr = Builder.CreateBitCast(dstField, PtrToInt8Ty);
      Builder.CreateCall3(CGM.getBlockObjectAssign(), dstAddr, srcValue,
                          llvm::ConstantInt::get(CGF.Int32Ty, flags));
    }
  }

  CGF.FinishFunction();

  return llvm::ConstantExpr::getBitCast(Fn, PtrToInt8Ty);
}

llvm::Constant *
BlockFunction::GenerateDestroyHelperFunction(const CGBlockInfo &blockInfo) {
  ASTContext &C = getContext();

  FunctionArgList args;
  // FIXME: This leaks
  ImplicitParamDecl *srcDecl =
    ImplicitParamDecl::Create(C, 0, SourceLocation(), 0, C.VoidPtrTy);
  args.push_back(std::make_pair(srcDecl, srcDecl->getType()));

  const CGFunctionInfo &FI =
      CGM.getTypes().getFunctionInfo(C.VoidTy, args, FunctionType::ExtInfo());

  // FIXME: We'd like to put these into a mergable by content, with
  // internal linkage.
  const llvm::FunctionType *LTy = CGM.getTypes().GetFunctionType(FI, false);

  llvm::Function *Fn =
    llvm::Function::Create(LTy, llvm::GlobalValue::InternalLinkage,
                           "__destroy_helper_block_", &CGM.getModule());

  IdentifierInfo *II
    = &CGM.getContext().Idents.get("__destroy_helper_block_");

  FunctionDecl *FD = FunctionDecl::Create(C, C.getTranslationUnitDecl(),
                                          SourceLocation(), II, C.VoidTy, 0,
                                          SC_Static,
                                          SC_None,
                                          false, true);
  CGF.StartFunction(FD, C.VoidTy, Fn, args, SourceLocation());

  const llvm::Type *structPtrTy = blockInfo.StructureType->getPointerTo();

  llvm::Value *src = CGF.GetAddrOfLocalVar(srcDecl);
  src = CGF.Builder.CreateLoad(src);
  src = CGF.Builder.CreateBitCast(src, structPtrTy, "block");

  const BlockDecl *blockDecl = blockInfo.getBlockDecl();

  CodeGenFunction::RunCleanupsScope cleanups(CGF);

  for (BlockDecl::capture_const_iterator ci = blockDecl->capture_begin(),
         ce = blockDecl->capture_end(); ci != ce; ++ci) {
    const VarDecl *variable = ci->getVariable();
    QualType type = variable->getType();

    const CGBlockInfo::Capture &capture = blockInfo.getCapture(variable);
    if (capture.isConstant()) continue;

    unsigned flags = 0;
    const CXXDestructorDecl *dtor = 0;

    if (ci->isByRef()) {
      flags = BLOCK_FIELD_IS_BYREF;
      if (type.isObjCGCWeak()) flags |= BLOCK_FIELD_IS_WEAK;
    } else if (type->isBlockPointerType()) {
      flags = BLOCK_FIELD_IS_BLOCK;
    } else if (type->isObjCObjectPointerType() || C.isObjCNSObjectType(type)) {
      flags = BLOCK_FIELD_IS_OBJECT;
    } else if (C.getLangOptions().CPlusPlus) {
      if (const CXXRecordDecl *record = type->getAsCXXRecordDecl())
        if (!record->hasTrivialDestructor())
          dtor = record->getDestructor();
    }

    if (!dtor && !flags) continue;

    unsigned index = capture.getIndex();
    llvm::Value *srcField = CGF.Builder.CreateStructGEP(src, index);

    // If there's an explicit copy expression, we do that.
    if (dtor) {
      CGF.PushDestructorCleanup(dtor, srcField);

    // Otherwise we call _Block_object_dispose.  It wouldn't be too
    // hard to just emit this as a cleanup if we wanted to make sure
    // that things were done in reverse.
    } else {
      llvm::Value *value = Builder.CreateLoad(srcField);
      value = Builder.CreateBitCast(value, PtrToInt8Ty);
      BuildBlockRelease(value, flags);
    }
  }

  cleanups.ForceCleanup();

  CGF.FinishFunction();

  return llvm::ConstantExpr::getBitCast(Fn, PtrToInt8Ty);
}

llvm::Constant *BlockFunction::
GeneratebyrefCopyHelperFunction(const llvm::Type *T, int flag, 
                                const VarDecl *BD) {
  QualType R = getContext().VoidTy;

  FunctionArgList Args;
  // FIXME: This leaks
  ImplicitParamDecl *Dst =
    ImplicitParamDecl::Create(getContext(), 0,
                              SourceLocation(), 0,
                              getContext().getPointerType(getContext().VoidTy));
  Args.push_back(std::make_pair(Dst, Dst->getType()));

  // FIXME: This leaks
  ImplicitParamDecl *Src =
    ImplicitParamDecl::Create(getContext(), 0,
                              SourceLocation(), 0,
                              getContext().getPointerType(getContext().VoidTy));
  Args.push_back(std::make_pair(Src, Src->getType()));

  const CGFunctionInfo &FI =
      CGM.getTypes().getFunctionInfo(R, Args, FunctionType::ExtInfo());

  CodeGenTypes &Types = CGM.getTypes();
  const llvm::FunctionType *LTy = Types.GetFunctionType(FI, false);

  // FIXME: We'd like to put these into a mergable by content, with
  // internal linkage.
  llvm::Function *Fn =
    llvm::Function::Create(LTy, llvm::GlobalValue::InternalLinkage,
                           "__Block_byref_object_copy_", &CGM.getModule());

  IdentifierInfo *II
    = &CGM.getContext().Idents.get("__Block_byref_object_copy_");

  FunctionDecl *FD = FunctionDecl::Create(getContext(),
                                          getContext().getTranslationUnitDecl(),
                                          SourceLocation(), II, R, 0,
                                          SC_Static,
                                          SC_None,
                                          false, true);
  CGF.StartFunction(FD, R, Fn, Args, SourceLocation());

  // dst->x
  llvm::Value *V = CGF.GetAddrOfLocalVar(Dst);
  V = Builder.CreateBitCast(V, llvm::PointerType::get(T, 0));
  V = Builder.CreateLoad(V);
  V = Builder.CreateStructGEP(V, 6, "x");
  llvm::Value *DstObj = V;

  // src->x
  V = CGF.GetAddrOfLocalVar(Src);
  V = Builder.CreateLoad(V);
  V = Builder.CreateBitCast(V, T);
  V = Builder.CreateStructGEP(V, 6, "x");
  
  if (flag & BLOCK_HAS_CXX_OBJ) {
    assert (BD && "VarDecl is null - GeneratebyrefCopyHelperFunction");
    llvm::Value *SrcObj = V;
    CGF.EmitSynthesizedCXXCopyCtor(DstObj, SrcObj, 
                                   getContext().getBlockVarCopyInits(BD));
  }
  else {
    DstObj = Builder.CreateBitCast(DstObj, PtrToInt8Ty);
    V = Builder.CreateBitCast(V, llvm::PointerType::get(PtrToInt8Ty, 0));
    llvm::Value *SrcObj = Builder.CreateLoad(V);
    flag |= BLOCK_BYREF_CALLER;
    llvm::Value *N = llvm::ConstantInt::get(CGF.Int32Ty, flag);
    llvm::Value *F = CGM.getBlockObjectAssign();
    Builder.CreateCall3(F, DstObj, SrcObj, N);
  }
  
  CGF.FinishFunction();

  return llvm::ConstantExpr::getBitCast(Fn, PtrToInt8Ty);
}

llvm::Constant *
BlockFunction::GeneratebyrefDestroyHelperFunction(const llvm::Type *T,
                                                  int flag,
                                                  const VarDecl *BD) {
  QualType R = getContext().VoidTy;

  FunctionArgList Args;
  // FIXME: This leaks
  ImplicitParamDecl *Src =
    ImplicitParamDecl::Create(getContext(), 0,
                              SourceLocation(), 0,
                              getContext().getPointerType(getContext().VoidTy));

  Args.push_back(std::make_pair(Src, Src->getType()));

  const CGFunctionInfo &FI =
      CGM.getTypes().getFunctionInfo(R, Args, FunctionType::ExtInfo());

  CodeGenTypes &Types = CGM.getTypes();
  const llvm::FunctionType *LTy = Types.GetFunctionType(FI, false);

  // FIXME: We'd like to put these into a mergable by content, with
  // internal linkage.
  llvm::Function *Fn =
    llvm::Function::Create(LTy, llvm::GlobalValue::InternalLinkage,
                           "__Block_byref_object_dispose_",
                           &CGM.getModule());

  IdentifierInfo *II
    = &CGM.getContext().Idents.get("__Block_byref_object_dispose_");

  FunctionDecl *FD = FunctionDecl::Create(getContext(),
                                          getContext().getTranslationUnitDecl(),
                                          SourceLocation(), II, R, 0,
                                          SC_Static,
                                          SC_None,
                                          false, true);
  CGF.StartFunction(FD, R, Fn, Args, SourceLocation());

  llvm::Value *V = CGF.GetAddrOfLocalVar(Src);
  V = Builder.CreateBitCast(V, llvm::PointerType::get(T, 0));
  V = Builder.CreateLoad(V);
  V = Builder.CreateStructGEP(V, 6, "x");
  if (flag & BLOCK_HAS_CXX_OBJ) {
    EHScopeStack::stable_iterator CleanupDepth = CGF.EHStack.stable_begin();
    assert (BD && "VarDecl is null - GeneratebyrefDestroyHelperFunction");
    QualType ClassTy = BD->getType();
    CGF.PushDestructorCleanup(ClassTy, V);
    CGF.PopCleanupBlocks(CleanupDepth);
  }
  else {
    V = Builder.CreateBitCast(V, llvm::PointerType::get(PtrToInt8Ty, 0));
    V = Builder.CreateLoad(V);

    flag |= BLOCK_BYREF_CALLER;
    BuildBlockRelease(V, flag);
  }
  CGF.FinishFunction();

  return llvm::ConstantExpr::getBitCast(Fn, PtrToInt8Ty);
}

llvm::Constant *BlockFunction::BuildbyrefCopyHelper(const llvm::Type *T,
                                                    uint32_t flags,
                                                    unsigned align,
                                                    const VarDecl *var) {
  // All alignments below that of pointer alignment collapse down to just
  // pointer alignment, as we always have at least that much alignment to begin
  // with.
  align /= unsigned(CGF.Target.getPointerAlign(0)/8);
  
  // As an optimization, we only generate a single function of each kind we
  // might need.  We need a different one for each alignment and for each
  // setting of flags.  We mix Align and flag to get the kind.
  uint64_t Kind = (uint64_t)align*BLOCK_BYREF_CURRENT_MAX + flags;
  llvm::Constant *&Entry = CGM.AssignCache[Kind];
  if (Entry)
    return Entry;
  return Entry = 
           CodeGenFunction(CGM).GeneratebyrefCopyHelperFunction(T, flags, var);
}

llvm::Constant *BlockFunction::BuildbyrefDestroyHelper(const llvm::Type *T,
                                                       uint32_t flags,
                                                       unsigned align,
                                                       const VarDecl *var) {
  // All alignments below that of pointer alignment collpase down to just
  // pointer alignment, as we always have at least that much alignment to begin
  // with.
  align /= unsigned(CGF.Target.getPointerAlign(0)/8);
  
  // As an optimization, we only generate a single function of each kind we
  // might need.  We need a different one for each alignment and for each
  // setting of flags.  We mix Align and flag to get the kind.
  uint64_t Kind = (uint64_t)align*BLOCK_BYREF_CURRENT_MAX + flags;
  llvm::Constant *&Entry = CGM.DestroyCache[Kind];
  if (Entry)
    return Entry;
  return Entry = 
       CodeGenFunction(CGM).GeneratebyrefDestroyHelperFunction(T, flags, var);
}

void BlockFunction::BuildBlockRelease(llvm::Value *V, uint32_t flags) {
  llvm::Value *F = CGM.getBlockObjectDispose();
  llvm::Value *N;
  V = Builder.CreateBitCast(V, PtrToInt8Ty);
  N = llvm::ConstantInt::get(CGF.Int32Ty, flags);
  Builder.CreateCall2(F, V, N);
}

ASTContext &BlockFunction::getContext() const { return CGM.getContext(); }

BlockFunction::BlockFunction(CodeGenModule &cgm, CodeGenFunction &cgf,
                             CGBuilderTy &B)
  : CGM(cgm), VMContext(cgm.getLLVMContext()), CGF(cgf),
    BlockInfo(0), BlockPointer(0), Builder(B) {
  PtrToInt8Ty = llvm::PointerType::getUnqual(
            llvm::Type::getInt8Ty(VMContext));
}
