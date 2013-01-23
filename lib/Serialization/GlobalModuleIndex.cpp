//===--- GlobalModuleIndex.cpp - Global Module Index ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the GlobalModuleIndex class.
//
//===----------------------------------------------------------------------===//

#include "ASTReaderInternals.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/OnDiskHashTable.h"
#include "clang/Serialization/ASTBitCodes.h"
#include "clang/Serialization/GlobalModuleIndex.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/Filesystem.h"
#include "llvm/Support/LockFileManager.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PathV2.h"
using namespace clang;
using namespace serialization;

//----------------------------------------------------------------------------//
// Shared constants
//----------------------------------------------------------------------------//
namespace {
  enum {
    /// \brief The block containing the index.
    GLOBAL_INDEX_BLOCK_ID = llvm::bitc::FIRST_APPLICATION_BLOCKID
  };

  /// \brief Describes the record types in the index.
  enum IndexRecordTypes {
    /// \brief Contains version information and potentially other metadata,
    /// used to determine if we can read this global index file.
    METADATA,
    /// \brief Describes a module, including its file name and dependencies.
    MODULE,
    /// \brief The index for identifiers.
    IDENTIFIER_INDEX
  };
}

/// \brief The name of the global index file.
static const char * const IndexFileName = "modules.idx";

/// \brief The global index file version.
static const unsigned CurrentVersion = 1;

//----------------------------------------------------------------------------//
// Global module index writer.
//----------------------------------------------------------------------------//

namespace {
  /// \brief Provides information about a specific module file.
  struct ModuleFileInfo {
    /// \brief The numberic ID for this module file.
    unsigned ID;

    /// \brief The set of modules on which this module depends. Each entry is
    /// a module ID.
    SmallVector<unsigned, 4> Dependencies;
  };

  /// \brief Builder that generates the global module index file.
  class GlobalModuleIndexBuilder {
    FileManager &FileMgr;

    /// \brief Mapping from files to module file information.
    typedef llvm::MapVector<const FileEntry *, ModuleFileInfo> ModuleFilesMap;

    /// \brief Information about each of the known module files.
    ModuleFilesMap ModuleFiles;

    /// \brief Mapping from identifiers to the list of module file IDs that
    /// consider this identifier to be interesting.
    typedef llvm::StringMap<SmallVector<unsigned, 2> > InterestingIdentifierMap;

    /// \brief A mapping from all interesting identifiers to the set of module
    /// files in which those identifiers are considered interesting.
    InterestingIdentifierMap InterestingIdentifiers;
    
    /// \brief Write the block-info block for the global module index file.
    void emitBlockInfoBlock(llvm::BitstreamWriter &Stream);

    /// \brief Retrieve the module file information for the given file.
    ModuleFileInfo &getModuleFileInfo(const FileEntry *File) {
      llvm::MapVector<const FileEntry *, ModuleFileInfo>::iterator Known
        = ModuleFiles.find(File);
      if (Known != ModuleFiles.end())
        return Known->second;

      unsigned NewID = ModuleFiles.size();
      ModuleFileInfo &Info = ModuleFiles[File];
      Info.ID = NewID;
      return Info;
    }

  public:
    explicit GlobalModuleIndexBuilder(FileManager &FileMgr) : FileMgr(FileMgr){}

    /// \brief Load the contents of the given module file into the builder.
    ///
    /// \returns true if an error occurred, false otherwise.
    bool loadModuleFile(const FileEntry *File);

    /// \brief Write the index to the given bitstream.
    void writeIndex(llvm::BitstreamWriter &Stream);
  };
}

static void emitBlockID(unsigned ID, const char *Name,
                        llvm::BitstreamWriter &Stream,
                        SmallVectorImpl<uint64_t> &Record) {
  Record.clear();
  Record.push_back(ID);
  Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_SETBID, Record);

  // Emit the block name if present.
  if (Name == 0 || Name[0] == 0) return;
  Record.clear();
  while (*Name)
    Record.push_back(*Name++);
  Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_BLOCKNAME, Record);
}

static void emitRecordID(unsigned ID, const char *Name,
                         llvm::BitstreamWriter &Stream,
                         SmallVectorImpl<uint64_t> &Record) {
  Record.clear();
  Record.push_back(ID);
  while (*Name)
    Record.push_back(*Name++);
  Stream.EmitRecord(llvm::bitc::BLOCKINFO_CODE_SETRECORDNAME, Record);
}

void
GlobalModuleIndexBuilder::emitBlockInfoBlock(llvm::BitstreamWriter &Stream) {
  SmallVector<uint64_t, 64> Record;
  Stream.EnterSubblock(llvm::bitc::BLOCKINFO_BLOCK_ID, 3);

#define BLOCK(X) emitBlockID(X ## _ID, #X, Stream, Record)
#define RECORD(X) emitRecordID(X, #X, Stream, Record)
  BLOCK(GLOBAL_INDEX_BLOCK);
  RECORD(METADATA);
  RECORD(MODULE);
  RECORD(IDENTIFIER_INDEX);
#undef RECORD
#undef BLOCK

  Stream.ExitBlock();
}

namespace clang {
  class InterestingASTIdentifierLookupTrait
    : public serialization::reader::ASTIdentifierLookupTraitBase {

  public:
    /// \brief The identifier and whether it is "interesting".
    typedef std::pair<StringRef, bool> data_type;

    data_type ReadData(const internal_key_type& k,
                       const unsigned char* d,
                       unsigned DataLen) {
      // The first bit indicates whether this identifier is interesting.
      // That's all we care about.
      using namespace clang::io;
      unsigned RawID = ReadUnalignedLE32(d);
      bool IsInteresting = RawID & 0x01;
      return std::make_pair(k, IsInteresting);
    }
  };
}

bool GlobalModuleIndexBuilder::loadModuleFile(const FileEntry *File) {
  // Open the module file.
  OwningPtr<llvm::MemoryBuffer> Buffer;
  Buffer.reset(FileMgr.getBufferForFile(File));
  if (!Buffer) {
    return true;
  }

  // Initialize the input stream
  llvm::BitstreamReader InStreamFile;
  llvm::BitstreamCursor InStream;
  InStreamFile.init((const unsigned char *)Buffer->getBufferStart(),
                  (const unsigned char *)Buffer->getBufferEnd());
  InStream.init(InStreamFile);

  // Sniff for the signature.
  if (InStream.Read(8) != 'C' ||
      InStream.Read(8) != 'P' ||
      InStream.Read(8) != 'C' ||
      InStream.Read(8) != 'H') {
    return true;
  }

  // Record this module file and assign it a unique ID (if it doesn't have
  // one already).
  unsigned ID = getModuleFileInfo(File).ID;

  // Search for the blocks and records we care about.
  enum { Outer, ControlBlock, ASTBlock } State = Outer;
  bool Done = false;
  while (!Done) {
    const unsigned Flags = llvm::BitstreamCursor::AF_DontPopBlockAtEnd;
    llvm::BitstreamEntry Entry = InStream.advance(Flags);
    switch (Entry.Kind) {
    case llvm::BitstreamEntry::Error:
      return true;

    case llvm::BitstreamEntry::Record:
      // In the outer state, just skip the record. We don't care.
      if (State == Outer) {
        InStream.skipRecord(Entry.ID);
        continue;
      }

      // Handle potentially-interesting records below.
      break;

    case llvm::BitstreamEntry::SubBlock:
      if (State == Outer && Entry.ID == CONTROL_BLOCK_ID) {
        if (InStream.EnterSubBlock(CONTROL_BLOCK_ID))
          return true;

        // Found the control block.
        State = ControlBlock;
        continue;
      }

      if (State == Outer && Entry.ID == AST_BLOCK_ID) {
        if (InStream.EnterSubBlock(AST_BLOCK_ID))
          return true;

        // Found the AST block.
        State = ASTBlock;
        continue;

      }

      if (InStream.SkipBlock())
        return true;

      continue;

    case llvm::BitstreamEntry::EndBlock:
      if (State == Outer) {
        Done = true;
      }
      State = Outer;
      continue;
    }

    // Read the given record.
    SmallVector<uint64_t, 64> Record;
    StringRef Blob;
    unsigned Code = InStream.readRecord(Entry.ID, Record, &Blob);

    // Handle module dependencies.
    if (State == ControlBlock && Code == IMPORTS) {
      // Load each of the imported PCH files.
      unsigned Idx = 0, N = Record.size();
      while (Idx < N) {
        // Read information about the AST file.

        // Skip the imported kind
        ++Idx;

        // Skip the import location
        ++Idx;

        // Retrieve the imported file name.
        unsigned Length = Record[Idx++];
        SmallString<128> ImportedFile(Record.begin() + Idx,
                                      Record.begin() + Idx + Length);
        Idx += Length;

        // Find the imported module file.
        const FileEntry *DependsOnFile = FileMgr.getFile(ImportedFile);
        if (!DependsOnFile)
          return true;

        // Record the dependency.
        unsigned DependsOnID = getModuleFileInfo(DependsOnFile).ID;
        getModuleFileInfo(File).Dependencies.push_back(DependsOnID);
      }

      continue;
    }

    // Handle the identifier table
    if (State == ASTBlock && Code == IDENTIFIER_TABLE && Record[0] > 0) {
      typedef OnDiskChainedHashTable<InterestingASTIdentifierLookupTrait>
        InterestingIdentifierTable;
      llvm::OwningPtr<InterestingIdentifierTable>
        Table(InterestingIdentifierTable::Create(
                (const unsigned char *)Blob.data() + Record[0],
                (const unsigned char *)Blob.data()));
      for (InterestingIdentifierTable::data_iterator D = Table->data_begin(),
                                                     DEnd = Table->data_end();
           D != DEnd; ++D) {
        std::pair<StringRef, bool> Ident = *D;
        if (Ident.second)
          InterestingIdentifiers[Ident.first].push_back(ID);
      }
    }

    // FIXME: Handle the selector table.
    
    // We don't care about this record.
  }

  return false;
}

namespace {

/// \brief Trait used to generate the identifier index as an on-disk hash
/// table.
class IdentifierIndexWriterTrait {
public:
  typedef StringRef key_type;
  typedef StringRef key_type_ref;
  typedef SmallVector<unsigned, 2> data_type;
  typedef const SmallVector<unsigned, 2> &data_type_ref;

  static unsigned ComputeHash(key_type_ref Key) {
    return llvm::HashString(Key);
  }

  std::pair<unsigned,unsigned>
  EmitKeyDataLength(raw_ostream& Out, key_type_ref Key, data_type_ref Data) {
    unsigned KeyLen = Key.size();
    unsigned DataLen = Data.size() * 4;
    clang::io::Emit16(Out, KeyLen);
    clang::io::Emit16(Out, DataLen);
    return std::make_pair(KeyLen, DataLen);
  }
  
  void EmitKey(raw_ostream& Out, key_type_ref Key, unsigned KeyLen) {
    Out.write(Key.data(), KeyLen);
  }

  void EmitData(raw_ostream& Out, key_type_ref Key, data_type_ref Data,
                unsigned DataLen) {
    for (unsigned I = 0, N = Data.size(); I != N; ++I)
      clang::io::Emit32(Out, Data[I]);
  }
};

}

void GlobalModuleIndexBuilder::writeIndex(llvm::BitstreamWriter &Stream) {
  using namespace llvm;
  
  // Emit the file header.
  Stream.Emit((unsigned)'B', 8);
  Stream.Emit((unsigned)'C', 8);
  Stream.Emit((unsigned)'G', 8);
  Stream.Emit((unsigned)'I', 8);

  // Write the block-info block, which describes the records in this bitcode
  // file.
  emitBlockInfoBlock(Stream);

  Stream.EnterSubblock(GLOBAL_INDEX_BLOCK_ID, 3);

  // Write the metadata.
  SmallVector<uint64_t, 2> Record;
  Record.push_back(CurrentVersion);
  Stream.EmitRecord(METADATA, Record);

  // Write the set of known module files.
  for (ModuleFilesMap::iterator M = ModuleFiles.begin(),
                                MEnd = ModuleFiles.end();
       M != MEnd; ++M) {
    Record.clear();
    Record.push_back(M->second.ID);
    Record.push_back(M->first->getSize());
    Record.push_back(M->first->getModificationTime());

    // File name
    StringRef Name(M->first->getName());
    Record.push_back(Name.size());
    Record.append(Name.begin(), Name.end());

    // Dependencies
    Record.push_back(M->second.Dependencies.size());
    Record.append(M->second.Dependencies.begin(), M->second.Dependencies.end());
    Stream.EmitRecord(MODULE, Record);
  }

  // Write the identifier -> module file mapping.
  {
    OnDiskChainedHashTableGenerator<IdentifierIndexWriterTrait> Generator;
    IdentifierIndexWriterTrait Trait;

    // Populate the hash table.
    for (InterestingIdentifierMap::iterator I = InterestingIdentifiers.begin(),
                                            IEnd = InterestingIdentifiers.end();
         I != IEnd; ++I) {
      Generator.insert(I->first(), I->second, Trait);
    }
    
    // Create the on-disk hash table in a buffer.
    SmallString<4096> IdentifierTable;
    uint32_t BucketOffset;
    {
      llvm::raw_svector_ostream Out(IdentifierTable);
      // Make sure that no bucket is at offset 0
      clang::io::Emit32(Out, 0);
      BucketOffset = Generator.Emit(Out, Trait);
    }

    // Create a blob abbreviation
    BitCodeAbbrev *Abbrev = new BitCodeAbbrev();
    Abbrev->Add(BitCodeAbbrevOp(IDENTIFIER_INDEX));
    Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Fixed, 32));
    Abbrev->Add(BitCodeAbbrevOp(BitCodeAbbrevOp::Blob));
    unsigned IDTableAbbrev = Stream.EmitAbbrev(Abbrev);

    // Write the identifier table
    Record.clear();
    Record.push_back(IDENTIFIER_INDEX);
    Record.push_back(BucketOffset);
    Stream.EmitRecordWithBlob(IDTableAbbrev, Record, IdentifierTable.str());
  }

  // FIXME: Selectors.

  Stream.ExitBlock();
}

GlobalModuleIndex::ErrorCode
GlobalModuleIndex::writeIndex(FileManager &FileMgr, StringRef Path) {
  llvm::SmallString<128> IndexPath;
  IndexPath += Path;
  llvm::sys::path::append(IndexPath, IndexFileName);

  // Coordinate building the global index file with other processes that might
  // try to do the same.
  llvm::LockFileManager Locked(IndexPath);
  switch (Locked) {
  case llvm::LockFileManager::LFS_Error:
    return EC_IOError;

  case llvm::LockFileManager::LFS_Owned:
    // We're responsible for building the index ourselves. Do so below.
    break;

  case llvm::LockFileManager::LFS_Shared:
    // Someone else is responsible for building the index. We don't care
    // when they finish, so we're done.
    return EC_Building;
  }

  // The module index builder.
  GlobalModuleIndexBuilder Builder(FileMgr);
  
  // Load each of the module files.
  llvm::error_code EC;
  for (llvm::sys::fs::directory_iterator D(Path, EC), DEnd;
       D != DEnd && !EC;
       D.increment(EC)) {
    // If this isn't a module file, we don't care.
    if (llvm::sys::path::extension(D->path()) != ".pcm") {
      // ... unless it's a .pcm.lock file, which indicates that someone is
      // in the process of rebuilding a module. They'll rebuild the index
      // at the end of that translation unit, so we don't have to.
      if (llvm::sys::path::extension(D->path()) == ".pcm.lock")
        return EC_Building;

      continue;
    }

    // If we can't find the module file, skip it.
    const FileEntry *ModuleFile = FileMgr.getFile(D->path());
    if (!ModuleFile)
      continue;

    // Load this module file.
    if (Builder.loadModuleFile(ModuleFile))
      return EC_IOError;
  }

  // The output buffer, into which the global index will be written.
  SmallVector<char, 16> OutputBuffer;
  {
    llvm::BitstreamWriter OutputStream(OutputBuffer);
    Builder.writeIndex(OutputStream);
  }

  // Write the global index file to a temporary file.
  llvm::SmallString<128> IndexTmpPath;
  int TmpFD;
  if (llvm::sys::fs::unique_file(IndexPath + "-%%%%%%%%", TmpFD, IndexTmpPath))
    return EC_IOError;

  // Open the temporary global index file for output.
  std::string ErrorInfo;
  llvm::raw_fd_ostream Out(IndexTmpPath.c_str(), ErrorInfo,
                           llvm::raw_fd_ostream::F_Binary);
  if (Out.has_error())
    return EC_IOError;

  // Write the index.
  Out.write(OutputBuffer.data(), OutputBuffer.size());
  Out.close();
  if (Out.has_error())
    return EC_IOError;

  // Remove the old index file. It isn't relevant any more.
  bool OldIndexExisted;
  llvm::sys::fs::remove(IndexPath.str(), OldIndexExisted);

  // Rename the newly-written index file to the proper name.
  if (llvm::sys::fs::rename(IndexTmpPath.str(), IndexPath.str())) {
    // Rename failed; just remove the 
    llvm::sys::fs::remove(IndexTmpPath.str(), OldIndexExisted);
    return EC_IOError;
  }

  // We're done.
  return EC_None;
}
