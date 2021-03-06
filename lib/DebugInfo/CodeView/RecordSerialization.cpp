//===-- RecordSerialization.cpp -------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Utilities for serializing and deserializing CodeView records.
//
//===----------------------------------------------------------------------===//

#include "llvm/DebugInfo/CodeView/RecordSerialization.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/DebugInfo/CodeView/CodeViewError.h"
#include "llvm/DebugInfo/CodeView/TypeRecord.h"
#include "llvm/DebugInfo/MSF/BinaryByteStream.h"

using namespace llvm;
using namespace llvm::codeview;
using namespace llvm::support;

/// Reinterpret a byte array as an array of characters. Does not interpret as
/// a C string, as StringRef has several helpers (split) that make that easy.
StringRef llvm::codeview::getBytesAsCharacters(ArrayRef<uint8_t> LeafData) {
  return StringRef(reinterpret_cast<const char *>(LeafData.data()),
                   LeafData.size());
}

StringRef llvm::codeview::getBytesAsCString(ArrayRef<uint8_t> LeafData) {
  return getBytesAsCharacters(LeafData).split('\0').first;
}

Error llvm::codeview::consume(msf::StreamReader &Reader, APSInt &Num) {
  // Used to avoid overload ambiguity on APInt construtor.
  bool FalseVal = false;
  uint16_t Short;
  if (auto EC = Reader.readInteger(Short, llvm::support::little))
    return EC;

  if (Short < LF_NUMERIC) {
    Num = APSInt(APInt(/*numBits=*/16, Short, /*isSigned=*/false),
                 /*isUnsigned=*/true);
    return Error::success();
  }

  switch (Short) {
  case LF_CHAR: {
    int8_t N;
    if (auto EC = Reader.readInteger(N, llvm::support::little))
      return EC;
    Num = APSInt(APInt(8, N, true), false);
    return Error::success();
  }
  case LF_SHORT: {
    int16_t N;
    if (auto EC = Reader.readInteger(N, llvm::support::little))
      return EC;
    Num = APSInt(APInt(16, N, true), false);
    return Error::success();
  }
  case LF_USHORT: {
    uint16_t N;
    if (auto EC = Reader.readInteger(N, llvm::support::little))
      return EC;
    Num = APSInt(APInt(16, N, false), true);
    return Error::success();
  }
  case LF_LONG: {
    int32_t N;
    if (auto EC = Reader.readInteger(N, llvm::support::little))
      return EC;
    Num = APSInt(APInt(32, N, true), false);
    return Error::success();
  }
  case LF_ULONG: {
    uint32_t N;
    if (auto EC = Reader.readInteger(N, llvm::support::little))
      return EC;
    Num = APSInt(APInt(32, N, FalseVal), true);
    return Error::success();
  }
  case LF_QUADWORD: {
    int64_t N;
    if (auto EC = Reader.readInteger(N, llvm::support::little))
      return EC;
    Num = APSInt(APInt(64, N, true), false);
    return Error::success();
  }
  case LF_UQUADWORD: {
    uint64_t N;
    if (auto EC = Reader.readInteger(N, llvm::support::little))
      return EC;
    Num = APSInt(APInt(64, N, false), true);
    return Error::success();
  }
  }
  return make_error<CodeViewError>(cv_error_code::corrupt_record,
                                   "Buffer contains invalid APSInt type");
}

Error llvm::codeview::consume(StringRef &Data, APSInt &Num) {
  ArrayRef<uint8_t> Bytes(Data.bytes_begin(), Data.bytes_end());
  msf::ByteStream S(Bytes);
  msf::StreamReader SR(S);
  auto EC = consume(SR, Num);
  Data = Data.take_back(SR.bytesRemaining());
  return EC;
}

/// Decode a numeric leaf value that is known to be a uint64_t.
Error llvm::codeview::consume_numeric(msf::StreamReader &Reader,
                                      uint64_t &Num) {
  APSInt N;
  if (auto EC = consume(Reader, N))
    return EC;
  if (N.isSigned() || !N.isIntN(64))
    return make_error<CodeViewError>(cv_error_code::corrupt_record,
                                     "Data is not a numeric value!");
  Num = N.getLimitedValue();
  return Error::success();
}

Error llvm::codeview::consume(msf::StreamReader &Reader, uint32_t &Item) {
  return Reader.readInteger(Item, llvm::support::little);
}

Error llvm::codeview::consume(StringRef &Data, uint32_t &Item) {
  ArrayRef<uint8_t> Bytes(Data.bytes_begin(), Data.bytes_end());
  msf::ByteStream S(Bytes);
  msf::StreamReader SR(S);
  auto EC = consume(SR, Item);
  Data = Data.take_back(SR.bytesRemaining());
  return EC;
}

Error llvm::codeview::consume(msf::StreamReader &Reader, int32_t &Item) {
  return Reader.readInteger(Item, llvm::support::little);
}

Error llvm::codeview::consume(msf::StreamReader &Reader, StringRef &Item) {
  if (Reader.empty())
    return make_error<CodeViewError>(cv_error_code::corrupt_record,
                                     "Null terminated string buffer is empty!");

  return Reader.readZeroString(Item);
}
