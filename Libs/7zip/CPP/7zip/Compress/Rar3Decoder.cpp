// Rar3Decoder.cpp
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives
 
#include "StdAfx.h"

#include "../Common/StreamUtils.h"

#include "Rar3Decoder.h"

namespace NCompress {
namespace NRar3 {

static const UInt32 kNumAlignReps = 15;

static const UInt32 kSymbolReadTable = 256;
static const UInt32 kSymbolRep = 259;
static const UInt32 kSymbolLen2 = kSymbolRep + kNumReps;

static const Byte kLenStart[kLenTableSize]      = {0,1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64,80,96,112,128,160,192,224};
static const Byte kLenDirectBits[kLenTableSize] = {0,0,0,0,0,0,0,0,1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5};

static const Byte kDistDirectBits[kDistTableSize] =
  {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  18,18,18,18,18,18,18,18,18,18,18,18};

static const Byte kLevelDirectBits[kLevelTableSize] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7};

static const Byte kLen2DistStarts[kNumLen2Symbols]={0,4,8,16,32,64,128,192};
static const Byte kLen2DistDirectBits[kNumLen2Symbols]={2,2,3, 4, 5, 6,  6,  6};

static const UInt32 kDistLimit3 = 0x2000 - 2;
static const UInt32 kDistLimit4 = 0x40000 - 2;

static const UInt32 kNormalMatchMinLen = 3;

static const UInt32 kVmDataSizeMax = 1 << 16;
static const UInt32 kVmCodeSizeMax = 1 << 16;

CDecoder::CDecoder():
  _window(0),
  _winPos(0),
  _wrPtr(0),
  _lzSize(0),
  _writtenFileSize(0),
  _vmData(0),
  _vmCode(0),
  m_IsSolid(false)
{
}

CDecoder::~CDecoder()
{
  InitFilters();
  ::MidFree(_vmData);
  ::MidFree(_window);
}

HRESULT CDecoder::WriteDataToStream(const Byte *data, UInt32 size)
{
  return WriteStream(_outStream, data, size);
}

HRESULT CDecoder::WriteData(const Byte *data, UInt32 size)
{
  HRESULT res = S_OK;
  if (_writtenFileSize < _unpackSize)
  {
    UInt32 curSize = size;
    UInt64 remain = _unpackSize - _writtenFileSize;
    if (remain < curSize)
      curSize = (UInt32)remain;
    res = WriteDataToStream(data, curSize);
  }
  _writtenFileSize += size;
  return res;
}

HRESULT CDecoder::WriteArea(UInt32 startPtr, UInt32 endPtr)
{
  if (startPtr <= endPtr)
    return WriteData(_window + startPtr, endPtr - startPtr);
  RINOK(WriteData(_window + startPtr, kWindowSize - startPtr));
  return WriteData(_window, endPtr);
}

void CDecoder::ExecuteFilter(int tempFilterIndex, NVm::CBlockRef &outBlockRef)
{
  CTempFilter *tempFilter = _tempFilters[tempFilterIndex];
  tempFilter->InitR[6] = (UInt32)_writtenFileSize;
  NVm::SetValue32(&tempFilter->GlobalData[0x24], (UInt32)_writtenFileSize);
  NVm::SetValue32(&tempFilter->GlobalData[0x28], (UInt32)(_writtenFileSize >> 32));
  CFilter *filter = _filters[tempFilter->FilterIndex];
  _vm.Execute(filter, tempFilter, outBlockRef, filter->GlobalData);
  delete tempFilter;
  _tempFilters[tempFilterIndex] = 0;
}

HRESULT CDecoder::WriteBuf()
{
  UInt32 writtenBorder = _wrPtr;
  UInt32 writeSize = (_winPos - writtenBorder) & kWindowMask;
  for (int i = 0; i < _tempFilters.Size(); i++)
  {
    CTempFilter *filter = _tempFilters[i];
    if (filter == NULL)
      continue;
    if (filter->NextWindow)
    {
      filter->NextWindow = false;
      continue;
    }
    UInt32 blockStart = filter->BlockStart;
    UInt32 blockSize = filter->BlockSize;
    if (((blockStart - writtenBorder) & kWindowMask) < writeSize)
    {
      if (writtenBorder != blockStart)
      {
        RINOK(WriteArea(writtenBorder, blockStart));
        writtenBorder = blockStart;
        writeSize = (_winPos - writtenBorder) & kWindowMask;
      }
      if (blockSize <= writeSize)
      {
        UInt32 blockEnd = (blockStart + blockSize) & kWindowMask;
        if (blockStart < blockEnd || blockEnd == 0)
          _vm.SetMemory(0, _window + blockStart, blockSize);
        else
        {
          UInt32 tailSize = kWindowSize - blockStart;
          _vm.SetMemory(0, _window + blockStart, tailSize);
          _vm.SetMemory(tailSize, _window, blockEnd);
        }
        NVm::CBlockRef outBlockRef;
        ExecuteFilter(i, outBlockRef);
        while (i + 1 < _tempFilters.Size())
        {
          CTempFilter *nextFilter = _tempFilters[i + 1];
          if (nextFilter == NULL || nextFilter->BlockStart != blockStart ||
              nextFilter->BlockSize != outBlockRef.Size || nextFilter->NextWindow)
            break;
          _vm.SetMemory(0, _vm.GetDataPointer(outBlockRef.Offset), outBlockRef.Size);
          ExecuteFilter(++i, outBlockRef);
        }
        WriteDataToStream(_vm.GetDataPointer(outBlockRef.Offset), outBlockRef.Size);
        _writtenFileSize += outBlockRef.Size;
        writtenBorder = blockEnd;
        writeSize = (_winPos - writtenBorder) & kWindowMask;
      }
      else
      {
        for (int j = i; j < _tempFilters.Size(); j++)
        {
          CTempFilter *filter = _tempFilters[j];
          if (filter != NULL && filter->NextWindow)
            filter->NextWindow = false;
        }
        _wrPtr = writtenBorder;
        return S_OK; // check it
      }
    }
  }
      
  _wrPtr = _winPos;
  return WriteArea(writtenBorder, _winPos);
}

void CDecoder::InitFilters()
{
  _lastFilter = 0;
  int i;
  for (i = 0; i < _tempFilters.Size(); i++)
    delete _tempFilters[i];
  _tempFilters.Clear();
  for (i = 0; i < _filters.Size(); i++)
    delete _filters[i];
  _filters.Clear();
}

bool CDecoder::AddVmCode(UInt32 firstByte, UInt32 codeSize)
{
  CMemBitDecoder inp;
  inp.Init(_vmData, codeSize);

  UInt32 filterIndex;
  if (firstByte & 0x80)
  {
    filterIndex = NVm::ReadEncodedUInt32(inp);
    if (filterIndex == 0)
      InitFilters();
    else
      filterIndex--;
  }
  else
    filterIndex = _lastFilter;
  if (filterIndex > (UInt32)_filters.Size())
    return false;
  _lastFilter = filterIndex;
  bool newFilter = (filterIndex == (UInt32)_filters.Size());

  CFilter *filter;
  if (newFilter)
  {
    // check if too many filters
    if (filterIndex > 1024)
      return false;
    filter = new CFilter;
    _filters.Add(filter);
  }
  else
  {
    filter = _filters[filterIndex];
    filter->ExecCount++;
  }

  int numEmptyItems = 0;
  int i;
  for (i = 0; i < _tempFilters.Size(); i++)
  {
    _tempFilters[i - numEmptyItems] = _tempFilters[i];
    if (_tempFilters[i] == NULL)
      numEmptyItems++;
    if (numEmptyItems > 0)
      _tempFilters[i] = NULL;
  }
  if (numEmptyItems == 0)
  {
    _tempFilters.Add(NULL);
    numEmptyItems = 1;
  }
  CTempFilter *tempFilter = new CTempFilter;
  _tempFilters[_tempFilters.Size() - numEmptyItems] = tempFilter;
  tempFilter->FilterIndex = filterIndex;
  tempFilter->ExecCount = filter->ExecCount;
 
  UInt32 blockStart = NVm::ReadEncodedUInt32(inp);
  if (firstByte & 0x40)
    blockStart += 258;
  tempFilter->BlockStart = (blockStart + _winPos) & kWindowMask;
  if (firstByte & 0x20)
    filter->BlockSize = NVm::ReadEncodedUInt32(inp);
  tempFilter->BlockSize = filter->BlockSize;
  tempFilter->NextWindow = _wrPtr != _winPos && ((_wrPtr - _winPos) & kWindowMask) <= blockStart;

  memset(tempFilter->InitR, 0, sizeof(tempFilter->InitR));
  tempFilter->InitR[3] = NVm::kGlobalOffset;
  tempFilter->InitR[4] = tempFilter->BlockSize;
  tempFilter->InitR[5] = tempFilter->ExecCount;
  if (firstByte & 0x10)
  {
    UInt32 initMask = inp.ReadBits(NVm::kNumGpRegs);
    for (int i = 0; i < NVm::kNumGpRegs; i++)
      if (initMask & (1 << i))
        tempFilter->InitR[i] = NVm::ReadEncodedUInt32(inp);
  }
  if (newFilter)
  {
    UInt32 vmCodeSize = NVm::ReadEncodedUInt32(inp);
    if (vmCodeSize >= kVmCodeSizeMax || vmCodeSize == 0)
      return false;
    for (UInt32 i = 0; i < vmCodeSize; i++)
      _vmCode[i] = (Byte)inp.ReadBits(8);
    _vm.PrepareProgram(_vmCode, vmCodeSize, filter);
  }

  tempFilter->AllocateEmptyFixedGlobal();

  Byte *globalData = &tempFilter->GlobalData[0];
  for (i = 0; i < NVm::kNumGpRegs; i++)
    NVm::SetValue32(&globalData[i * 4], tempFilter->InitR[i]);
  NVm::SetValue32(&globalData[NVm::NGlobalOffset::kBlockSize], tempFilter->BlockSize);
  NVm::SetValue32(&globalData[NVm::NGlobalOffset::kBlockPos], 0); // It was commented. why?
  NVm::SetValue32(&globalData[NVm::NGlobalOffset::kExecCount], tempFilter->ExecCount);

  if (firstByte & 8)
  {
    UInt32 dataSize = NVm::ReadEncodedUInt32(inp);
    if (dataSize > NVm::kGlobalSize - NVm::kFixedGlobalSize)
      return false;
    CRecordVector<Byte> &globalData = tempFilter->GlobalData;
    int requredSize = (int)(dataSize + NVm::kFixedGlobalSize);
    if (globalData.Size() < requredSize)
    {
      globalData.Reserve(requredSize);
      for (; globalData.Size() < requredSize; i++)
        globalData.Add(0);
    }
    for (UInt32 i = 0; i < dataSize; i++)
      globalData[NVm::kFixedGlobalSize + i] = (Byte)inp.ReadBits(8);
  }
  return true;
}

bool CDecoder::ReadVmCodeLZ()
{
  UInt32 firstByte = m_InBitStream.ReadBits(8);
  UInt32 length = (firstByte & 7) + 1;
  if (length == 7)
    length = m_InBitStream.ReadBits(8) + 7;
  else if (length == 8)
    length = m_InBitStream.ReadBits(16);
  if (length > kVmDataSizeMax)
    return false;
  for (UInt32 i = 0; i < length; i++)
    _vmData[i] = (Byte)m_InBitStream.ReadBits(8);
  return AddVmCode(firstByte, length);
}

bool CDecoder::ReadVmCodePPM()
{
  int firstByte = DecodePpmSymbol();
  if (firstByte == -1)
    return false;
  UInt32 length = (firstByte & 7) + 1;
  if (length == 7)
  {
    int b1 = DecodePpmSymbol();
    if (b1 == -1)
      return false;
    length = b1 + 7;
  }
  else if (length == 8)
  {
    int b1 = DecodePpmSymbol();
    if (b1 == -1)
      return false;
    int b2 = DecodePpmSymbol();
    if (b2 == -1)
      return false;
    length = b1 * 256 + b2;
  }
  if (length > kVmDataSizeMax)
    return false;
  for (UInt32 i = 0; i < length; i++)
  {
    int b = DecodePpmSymbol();
    if (b == -1)
      return false;
    _vmData[i] = (Byte)b;
  }
  return AddVmCode(firstByte, length);
}

#define RIF(x) { if (!(x)) return S_FALSE; }

UInt32 CDecoder::ReadBits(int numBits) { return m_InBitStream.ReadBits(numBits); }

/////////////////////////////////////////////////
// PPM

HRESULT CDecoder::InitPPM()
{
  Byte maxOrder = (Byte)ReadBits(7);

  bool reset = ((maxOrder & 0x20) != 0);
  int maxMB = 0;
  if (reset)
    maxMB = (Byte)ReadBits(8);
  else
  {
    if (_ppm.SubAllocator.GetSubAllocatorSize()== 0)
      return S_FALSE;
  }
  if (maxOrder & 0x40)
    PpmEscChar = (Byte)ReadBits(8);
  m_InBitStream.InitRangeCoder();
  /*
  if (m_InBitStream.m_BitPos != 0)
    return S_FALSE;
  */
  if (reset)
  {
    maxOrder = (maxOrder & 0x1F) + 1;
    if (maxOrder > 16)
      maxOrder = 16 + (maxOrder - 16) * 3;
    if (maxOrder == 1)
    {
      // SubAlloc.StopSubAllocator();
      _ppm.SubAllocator.StopSubAllocator();
      return S_FALSE;
    }
    // SubAlloc.StartSubAllocator(MaxMB+1);
    // StartModelRare(maxOrder);

    if (!_ppm.SubAllocator.StartSubAllocator((maxMB + 1) << 20))
      return E_OUTOFMEMORY;
    _ppm.MaxOrder = 0;
    _ppm.StartModelRare(maxOrder);

  }
  // return (minContext != NULL);

  return S_OK;
}

int CDecoder::DecodePpmSymbol() { return _ppm.DecodeSymbol(&m_InBitStream); }

HRESULT CDecoder::DecodePPM(Int32 num, bool &keepDecompressing)
{
  keepDecompressing = false;
  do
  {
    if (((_wrPtr - _winPos) & kWindowMask) < 260 && _wrPtr != _winPos)
    {
      RINOK(WriteBuf());
      if (_writtenFileSize > _unpackSize)
      {
        keepDecompressing = false;
        return S_OK;
      }
    }
    int c = DecodePpmSymbol();
    if (c == -1)
    {
      // Original code sets PPMError=true here and then it returns S_OK. Why ???
      // return S_OK;
      return S_FALSE;
    }
    if (c == PpmEscChar)
    {
      int nextCh = DecodePpmSymbol();
      if (nextCh == 0)
        return ReadTables(keepDecompressing);
      if (nextCh == 2 || nextCh == -1)
        return S_OK;
      if (nextCh == 3)
      {
        if (!ReadVmCodePPM())
          return S_FALSE;
        continue;
      }
      if (nextCh == 4 || nextCh == 5)
      {
        UInt32 distance = 0;
        UInt32 length = 4;
        if (nextCh == 4)
        {
          for (int i = 0; i < 3; i++)
          {
            int c = DecodePpmSymbol();
            if (c == -1)
              return S_OK;
            distance = (distance << 8) + (Byte)c;
          }
          distance++;
          length += 28;
        }
        int c = DecodePpmSymbol();
        if (c == -1)
          return S_OK;
        length += c;
        if (distance >= _lzSize)
          return S_FALSE;
        CopyBlock(distance, length);
        num -= (Int32)length;
        continue;
      }
    }
    PutByte((Byte)c);
    num--;
  }
  while (num >= 0);
  keepDecompressing = true;
  return S_OK;
}

/////////////////////////////////////////////////
// LZ

HRESULT CDecoder::ReadTables(bool &keepDecompressing)
{
  keepDecompressing = true;
  ReadBits((8 - m_InBitStream.GetBitPosition()) & 7);
  if (ReadBits(1) != 0)
  {
    _lzMode = false;
    return InitPPM();
  }

  _lzMode = true;
  PrevAlignBits = 0;
  PrevAlignCount = 0;

  Byte levelLevels[kLevelTableSize];
  Byte newLevels[kTablesSizesSum];

  if (ReadBits(1) == 0)
    memset(m_LastLevels, 0, kTablesSizesSum);

  int i;
  for (i = 0; i < kLevelTableSize; i++)
  {
    UInt32 length = ReadBits(4);
    if (length == 15)
    {
      UInt32 zeroCount = ReadBits(4);
      if (zeroCount != 0)
      {
        zeroCount += 2;
        while (zeroCount-- > 0 && i < kLevelTableSize)
          levelLevels[i++]=0;
        i--;
        continue;
      }
    }
    levelLevels[i] = (Byte)length;
  }
  RIF(m_LevelDecoder.SetCodeLengths(levelLevels));
  i = 0;
  while (i < kTablesSizesSum)
  {
    UInt32 number = m_LevelDecoder.DecodeSymbol(&m_InBitStream);
    if (number < 16)
    {
      newLevels[i] = Byte((number + m_LastLevels[i]) & 15);
      i++;
    }
    else if (number > kLevelTableSize)
      return S_FALSE;
    else
    {
      int num;
      if (((number - 16) & 1) == 0)
        num = ReadBits(3) + 3;
      else
        num = ReadBits(7) + 11;
      if (number < 18)
      {
        if (i == 0)
          return S_FALSE;
        for (; num > 0 && i < kTablesSizesSum; num--, i++)
          newLevels[i] = newLevels[i - 1];
      }
      else
      {
        for (; num > 0 && i < kTablesSizesSum; num--)
          newLevels[i++] = 0;
      }
    }
  }
  TablesRead = true;

  // original code has check here:
  /*
  if (InAddr > ReadTop)
  {
    keepDecompressing = false;
    return true;
  }
  */

  RIF(m_MainDecoder.SetCodeLengths(&newLevels[0]));
  RIF(m_DistDecoder.SetCodeLengths(&newLevels[kMainTableSize]));
  RIF(m_AlignDecoder.SetCodeLengths(&newLevels[kMainTableSize + kDistTableSize]));
  RIF(m_LenDecoder.SetCodeLengths(&newLevels[kMainTableSize + kDistTableSize + kAlignTableSize]));

  memcpy(m_LastLevels, newLevels, kTablesSizesSum);
  return S_OK;
}

class CCoderReleaser
{
  CDecoder *m_Coder;
public:
  CCoderReleaser(CDecoder *coder): m_Coder(coder) {}
  ~CCoderReleaser()
  {
    // m_Coder->m_OutWindowStream.Flush();
    m_Coder->ReleaseStreams();
  }
};

HRESULT CDecoder::ReadEndOfBlock(bool &keepDecompressing)
{
  if (ReadBits(1) != 0)
  {
    // old file
    TablesRead = false;
    return ReadTables(keepDecompressing);
  }
  // new file
  keepDecompressing = false;
  TablesRead = (ReadBits(1) == 0);
  return S_OK;
}

UInt32 kDistStart[kDistTableSize];

class CDistInit
{
public:
  CDistInit() { Init(); }
  void Init()
  {
    UInt32 start = 0;
    for (UInt32 i = 0; i < kDistTableSize; i++)
    {
      kDistStart[i] = start;
      start += (1 << kDistDirectBits[i]);
    }
  }
} g_DistInit;

HRESULT CDecoder::DecodeLZ(bool &keepDecompressing)
{
  UInt32 rep0 = _reps[0];
  UInt32 rep1 = _reps[1];
  UInt32 rep2 = _reps[2];
  UInt32 rep3 = _reps[3];
  UInt32 length = _lastLength;
  for (;;)
  {
    if (((_wrPtr - _winPos) & kWindowMask) < 260 && _wrPtr != _winPos)
    {
      RINOK(WriteBuf());
      if (_writtenFileSize > _unpackSize)
      {
        keepDecompressing = false;
        return S_OK;
      }
    }
    UInt32 number = m_MainDecoder.DecodeSymbol(&m_InBitStream);
    if (number < 256)
    {
      PutByte((Byte)number);
      continue;
    }
    else if (number == kSymbolReadTable)
    {
      RINOK(ReadEndOfBlock(keepDecompressing));
      break;
    }
    else if (number == 257)
    {
      if (!ReadVmCodeLZ())
        return S_FALSE;
      continue;
    }
    else if (number == 258)
    {
      if (length == 0)
        return S_FALSE;
    }
    else if (number < kSymbolRep + 4)
    {
      if (number != kSymbolRep)
      {
        UInt32 distance;
        if (number == kSymbolRep + 1)
          distance = rep1;
        else
        {
          if (number == kSymbolRep + 2)
            distance = rep2;
          else
          {
            distance = rep3;
            rep3 = rep2;
          }
          rep2 = rep1;
        }
        rep1 = rep0;
        rep0 = distance;
      }

      UInt32 number = m_LenDecoder.DecodeSymbol(&m_InBitStream);
      if (number >= kLenTableSize)
        return S_FALSE;
      length = 2 + kLenStart[number] + m_InBitStream.ReadBits(kLenDirectBits[number]);
    }
    else
    {
      rep3 = rep2;
      rep2 = rep1;
      rep1 = rep0;
      if (number < 271)
      {
        number -= 263;
        rep0 = kLen2DistStarts[number] + m_InBitStream.ReadBits(kLen2DistDirectBits[number]);
        length = 2;
      }
      else if (number < 299)
      {
        number -= 271;
        length = kNormalMatchMinLen + (UInt32)kLenStart[number] + m_InBitStream.ReadBits(kLenDirectBits[number]);
        UInt32 number = m_DistDecoder.DecodeSymbol(&m_InBitStream);
        if (number >= kDistTableSize)
          return S_FALSE;
        rep0 = kDistStart[number];
        int numBits = kDistDirectBits[number];
        if (number >= (kNumAlignBits * 2) + 2)
        {
          if (numBits > kNumAlignBits)
            rep0 += (m_InBitStream.ReadBits(numBits - kNumAlignBits) << kNumAlignBits);
          if (PrevAlignCount > 0)
          {
            PrevAlignCount--;
            rep0 += PrevAlignBits;
          }
          else
          {
            UInt32 number = m_AlignDecoder.DecodeSymbol(&m_InBitStream);
            if (number < (1 << kNumAlignBits))
            {
              rep0 += number;
              PrevAlignBits = number;
            }
            else if (number  == (1 << kNumAlignBits))
            {
              PrevAlignCount = kNumAlignReps;
              rep0 += PrevAlignBits;
            }
            else
              return S_FALSE;
          }
        }
        else
          rep0 += m_InBitStream.ReadBits(numBits);
        length += ((kDistLimit4 - rep0) >> 31) + ((kDistLimit3 - rep0) >> 31);
      }
      else
        return S_FALSE;
    }
    if (rep0 >= _lzSize)
      return S_FALSE;
    CopyBlock(rep0, length);
  }
  _reps[0] = rep0;
  _reps[1] = rep1;
  _reps[2] = rep2;
  _reps[3] = rep3;
  _lastLength = length;

  return S_OK;
}

HRESULT CDecoder::CodeReal(ICompressProgressInfo *progress)
{
  _writtenFileSize = 0;
  if (!m_IsSolid)
  {
    _lzSize = 0;
    _winPos = 0;
    _wrPtr = 0;
    for (int i = 0; i < kNumReps; i++)
      _reps[i] = 0;
    _lastLength = 0;
    memset(m_LastLevels, 0, kTablesSizesSum);
    TablesRead = false;
    PpmEscChar = 2;
    InitFilters();
  }
  if (!m_IsSolid || !TablesRead)
  {
    bool keepDecompressing;
    RINOK(ReadTables(keepDecompressing));
    if (!keepDecompressing)
      return S_OK;
  }

  for(;;)
  {
    bool keepDecompressing;
    if (_lzMode)
    {
      RINOK(DecodeLZ(keepDecompressing))
    }
    else
    {
      RINOK(DecodePPM(1 << 18, keepDecompressing))
    }
    UInt64 packSize = m_InBitStream.GetProcessedSize();
    RINOK(progress->SetRatioInfo(&packSize, &_writtenFileSize));
    if (!keepDecompressing)
      break;
  }
  RINOK(WriteBuf());
  if (_writtenFileSize < _unpackSize)
    return S_FALSE;
  // return m_OutWindowStream.Flush();
  return S_OK;
}

STDMETHODIMP CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  try
  {
    if (inSize == NULL || outSize == NULL)
      return E_INVALIDARG;

    if (_vmData == 0)
    {
      _vmData = (Byte *)::MidAlloc(kVmDataSizeMax + kVmCodeSizeMax);
      if (_vmData == 0)
        return E_OUTOFMEMORY;
      _vmCode = _vmData + kVmDataSizeMax;
    }
    
    if (_window == 0)
    {
      _window = (Byte *)::MidAlloc(kWindowSize);
      if (_window == 0)
        return E_OUTOFMEMORY;
    }
    if (!m_InBitStream.Create(1 << 20))
      return E_OUTOFMEMORY;
    if (!_vm.Create())
      return E_OUTOFMEMORY;

    
    m_InBitStream.SetStream(inStream);
    m_InBitStream.Init();
    _outStream = outStream;
   
    CCoderReleaser coderReleaser(this);
    _unpackSize = *outSize;
    return CodeReal(progress);
  }
  catch(const CInBufferException &e)  { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
  // CNewException is possible here. But probably CNewException is caused
  // by error in data stream.
}

STDMETHODIMP CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size)
{
  if (size < 1)
    return E_INVALIDARG;
  m_IsSolid = (data[0] != 0);
  return S_OK;
}

}}
