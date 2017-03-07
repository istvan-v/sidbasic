
// Utility to extract SID register data from PSID files
// Copyright (C) 2017 Istvan Varga <istvanv@users.sourceforge.net>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#if 0
#include <map>
#endif

#include "plus4emu.hpp"
#include "cpu.hpp"

static const uint16_t playerCodeStartAddr = 0x0420;

static const uint8_t  playerCode[32] = {
  // INC $0437      BPL +19     NOP   PHA   TXA
  0xEE, 0x37, 0x04, 0x10, 0x13, 0xEA, 0x48, 0x8A,
  // PHA  TYA PHA   JMP ($0314)       JSR $0000
  0x48, 0x98, 0x48, 0x6C, 0x14, 0x03, 0x20, 0x00,
  //    PLA   TAY   PLA   TAX   PLA   RTI   data
  0x00, 0x68, 0xA8, 0x68, 0xAA, 0x68, 0x40, 0xFF,
  // CLI  NOP NOP   NOP   NOP   CLC   BCC -8
  0x58, 0xEA, 0xEA, 0xEA, 0xEA, 0x18, 0x90, 0xF8
};

class SIDPlayVM {
 protected:
  Plus4::M7501  cpu;
  std::vector< uint64_t >   memBuffer;
  uint8_t       *memory;                // 65536 bytes
  bool          irqFlag;
  bool          prvGateBit[3];
  bool          gateBitEdgeFlag[3];
  unsigned int  irqPeriod;              // in cycles
  double        irqFrequency;           // in Hz
  // --------
  static PLUS4EMU_REGPARM2 uint8_t readMemory(void *userData, uint16_t addr);
  static PLUS4EMU_REGPARM3 void writeMemory(void *userData,
                                            uint16_t addr, uint8_t value);
  static PLUS4EMU_REGPARM2 uint8_t readMemory_FFFE(void *userData,
                                                   uint16_t addr);
  // D400-D41F
  static PLUS4EMU_REGPARM3 void writeMemory_SID(void *userData,
                                                uint16_t addr, uint8_t value);
  // DC00-DC0F
  static PLUS4EMU_REGPARM3 void writeMemory_CIA1(void *userData,
                                                 uint16_t addr, uint8_t value);
  // DD00-DD0F
  static PLUS4EMU_REGPARM3 void writeMemory_CIA2(void *userData,
                                                 uint16_t addr, uint8_t value);
  // returns false if trackNum is out of range
  bool loadPSIDFile(const std::vector< uint8_t >& inBuf, int trackNum);
 public:
  SIDPlayVM();
  virtual ~SIDPlayVM();
  bool convertPSIDFile(std::vector< uint8_t >& outBuf,
                       const std::vector< uint8_t >& inBuf,
                       int trackNum, int nSeconds = -1);
};

PLUS4EMU_REGPARM2 uint8_t SIDPlayVM::readMemory(void *userData, uint16_t addr)
{
  SIDPlayVM&  this_ = *(reinterpret_cast< SIDPlayVM * >(userData));
  return this_.memory[addr];
}

PLUS4EMU_REGPARM3 void SIDPlayVM::writeMemory(void *userData,
                                              uint16_t addr, uint8_t value)
{
  SIDPlayVM&  this_ = *(reinterpret_cast< SIDPlayVM * >(userData));
#if 0
  if (addr >= playerCodeStartAddr && addr <= (playerCodeStartAddr + 0x001F) &&
      addr != (playerCodeStartAddr + 0x0017)) {
    throw Plus4Emu::Exception("player code overwritten");
  }
#endif
  this_.memory[addr] = value;
}

PLUS4EMU_REGPARM2 uint8_t SIDPlayVM::readMemory_FFFE(void *userData,
                                                     uint16_t addr)
{
  SIDPlayVM&  this_ = *(reinterpret_cast< SIDPlayVM * >(userData));
  this_.irqFlag = false;
  return this_.memory[addr];
}

PLUS4EMU_REGPARM3 void SIDPlayVM::writeMemory_SID(void *userData,
                                                  uint16_t addr, uint8_t value)
{
  SIDPlayVM&  this_ = *(reinterpret_cast< SIDPlayVM * >(userData));
  switch (addr & 0x001F) {
  case 0x04:
  case 0x0B:
  case 0x12:
    {
      int     n = int(addr & 0x001F) >> 3;
      if ((value & 0x01) > (this_.memory[addr] & 0x01) && this_.prvGateBit[n])
        this_.gateBitEdgeFlag[n] = true;
      else if (!(value & 0x01))
        this_.gateBitEdgeFlag[n] = false;
    }
    break;
  }
  this_.memory[addr] = value;
}

PLUS4EMU_REGPARM3 void SIDPlayVM::writeMemory_CIA1(void *userData,
                                                   uint16_t addr, uint8_t value)
{
  SIDPlayVM&  this_ = *(reinterpret_cast< SIDPlayVM * >(userData));
  this_.memory[addr] = value;
}

PLUS4EMU_REGPARM3 void SIDPlayVM::writeMemory_CIA2(void *userData,
                                                   uint16_t addr, uint8_t value)
{
  SIDPlayVM&  this_ = *(reinterpret_cast< SIDPlayVM * >(userData));
  this_.memory[addr] = value;
}

SIDPlayVM::SIDPlayVM()

  : memory((uint8_t *) 0),
    irqFlag(false),
    irqPeriod(20000U),
    irqFrequency(50.0)
{
  for (int i = 0; i < 3; i++) {
    prvGateBit[i] = false;
    gateBitEdgeFlag[i] = false;
  }
  memBuffer.resize(8192, 0UL);
  memory = reinterpret_cast< uint8_t * >(&(memBuffer.front()));
  cpu.setMemoryCallbackUserData(this);
  for (uint32_t i = 0; i <= 0xFFFF; i++) {
    if (i >= 0xD400 && i <= 0xD41F) {
      cpu.setMemoryReadCallback(uint16_t(i), &readMemory);
      cpu.setMemoryWriteCallback(uint16_t(i), &writeMemory_SID);
    }
    else if (i >= 0xDC00 && i <= 0xDC0F) {
      cpu.setMemoryReadCallback(uint16_t(i), &readMemory);
      cpu.setMemoryWriteCallback(uint16_t(i), &writeMemory_CIA1);
    }
    else if (i >= 0xDD00 && i <= 0xDD0F) {
      cpu.setMemoryReadCallback(uint16_t(i), &readMemory);
      cpu.setMemoryWriteCallback(uint16_t(i), &writeMemory_CIA2);
    }
    else if (i == 0xFFFE) {
      cpu.setMemoryReadCallback(uint16_t(i), &readMemory_FFFE);
      cpu.setMemoryWriteCallback(uint16_t(i), &writeMemory);
    }
    else {
      cpu.setMemoryReadCallback(uint16_t(i), &readMemory);
      cpu.setMemoryWriteCallback(uint16_t(i), &writeMemory);
    }
  }
}

SIDPlayVM::~SIDPlayVM()
{
}

bool SIDPlayVM::loadPSIDFile(const std::vector< uint8_t >& inBuf, int trackNum)
{
  if (inBuf.size() < 0x0080 || inBuf.size() > 0xFFFF)
    throw Plus4Emu::Exception("invalid input file size");
  uint32_t  magicID = (uint32_t(inBuf[0]) << 24) | (uint32_t(inBuf[1]) << 16)
                      | (uint32_t(inBuf[2]) << 8) | uint32_t(inBuf[3]);
  uint16_t  version = (uint16_t(inBuf[4]) << 8) | uint16_t(inBuf[5]);
  uint16_t  dataOffset = (uint16_t(inBuf[6]) << 8) | uint16_t(inBuf[7]);
  uint16_t  loadAddress = (uint16_t(inBuf[8]) << 8) | uint16_t(inBuf[9]);
  uint16_t  initAddress = (uint16_t(inBuf[10]) << 8) | uint16_t(inBuf[11]);
  uint16_t  playAddress = (uint16_t(inBuf[12]) << 8) | uint16_t(inBuf[13]);
  int       nTracks = (int(inBuf[14]) << 8) | int(inBuf[15]);
  int       startSong = (int(inBuf[16]) << 8) | int(inBuf[17]);
  uint32_t  speed = (uint32_t(inBuf[18]) << 24) | (uint32_t(inBuf[19]) << 16)
                    | (uint32_t(inBuf[20]) << 8) | uint32_t(inBuf[21]);
  bool      ntscFlag = false;
  if (magicID == 0x52534944)                    // "RSID"
    throw Plus4Emu::Exception("RSID format is not supported");
  if (magicID != 0x50534944)                    // "PSID"
    throw Plus4Emu::Exception("invalid input file format");
  if (!(version >= 0x0001 && version <= 0x0004))
    throw Plus4Emu::Exception("unsupported PSID format version");
  if (dataOffset < 0x0076 || (size_t(dataOffset) + 2) > inBuf.size())
    throw Plus4Emu::Exception("invalid data offset in PSID header");
  if (nTracks < 1)
    throw Plus4Emu::Exception("invalid number of tracks in PSID header");
  if (startSong < 1)
    startSong = 1;
  if (trackNum < 1)
    trackNum = startSong;
  if (trackNum >= 32 && nTracks > 32)
    std::fprintf(stderr, "WARNING: PSID file contains more than 32 songs\n");
  if (trackNum > nTracks || trackNum > 32)
    return false;
  // -1: video interrupt, 0: CIA 1 (DC04-DC05)
  int     intFreq = int(bool(speed & (1U << (trackNum - 1)))) - 1;
  if (trackNum == 1) {
    for (int i = 0; i < 3; i++) {
      if (i == 0)
        std::fprintf(stderr, "Name:           ");
      else if (i == 1)
        std::fprintf(stderr, "Author:         ");
      else
        std::fprintf(stderr, "Released:       ");
      for (int j = 0; j < 32; j++) {
        uint8_t c = inBuf[i * 32 + j + 22];
        if (!c)
          break;
        if (c < 0x20 || c >= 0x7F)
          c = 0x2E;
        std::fputc(c, stderr);
      }
      std::fputc('\n', stderr);
    }
    if (version > 0x0001 && dataOffset >= 0x007C) {
      if (inBuf[0x0077] & 0x01)
        throw Plus4Emu::Exception("MusPlayer format is not supported");
      if (inBuf[0x0077] & 0x02)
        throw Plus4Emu::Exception("PlaySID samples are not supported");
      std::fprintf(stderr, "Video standard: ");
      switch ((inBuf[0x0077] & 0x0C) >> 2) {
      case 0:
        std::fprintf(stderr, "unknown\n");
        intFreq = intFreq & 50;
        break;
      case 1:
        std::fprintf(stderr, "PAL\n");
        intFreq = intFreq & 50;
        break;
      case 2:
        std::fprintf(stderr, "NTSC\n");
        ntscFlag = true;
        intFreq = intFreq & 60;
        break;
      case 3:
        std::fprintf(stderr, "PAL and NTSC\n");
        intFreq = intFreq & 50;
        break;
      }
      std::fprintf(stderr, "SID model:      ");
      switch ((inBuf[0x0077] & 0x30) >> 4) {
      case 0:
        std::fprintf(stderr, "unknown\n");
        break;
      case 1:
        std::fprintf(stderr, "MOS6581\n");
        break;
      case 2:
        std::fprintf(stderr, "MOS8580\n");
        break;
      case 3:
        std::fprintf(stderr, "MOS6581 and MOS8580\n");
        break;
      }
    }
  }
  std::memset(&(memBuffer.front()), 0x00, sizeof(uint64_t) * memBuffer.size());
  if (!loadAddress) {
    loadAddress = uint16_t(inBuf[dataOffset])
                  | (uint16_t(inBuf[dataOffset + 1]) << 8);
    dataOffset = dataOffset + 2;
  }
  if (!initAddress)
    initAddress = loadAddress;
  memory[0xD418] = 0x0F;
  for (size_t i = 0; i < (sizeof(playerCode) / sizeof(uint8_t)); i++)
    memory[0xEA20 + i] = playerCode[i];
  for (size_t i = dataOffset; i < inBuf.size(); i++) {
    memory[loadAddress] = inBuf[i];
    loadAddress = (loadAddress + 1) & 0xFFFF;
  }
  memory[0xFFFC] = uint8_t(initAddress & 0xFF);
  memory[0xFFFD] = uint8_t(initAddress >> 8);
  memory[0xFFFE] = uint8_t((playerCodeStartAddr + 6) & 0xFF);
  memory[0xFFFF] = uint8_t((playerCodeStartAddr + 6) >> 8);
  cpu.reset(true);
  {
    Plus4::M7501Registers r;
    cpu.getRegisters(r);
    r.reg_AC = uint8_t(trackNum - 1);
    r.reg_SP = 0x00;
    cpu.setRegisters(r);
  }
  memory[0x01FE] = uint8_t((playerCodeStartAddr - 1) & 0xFF);
  memory[0x01FF] = uint8_t((playerCodeStartAddr - 1) >> 8);
  memory[0x02A6] = uint8_t(!ntscFlag);
  memory[0x0314] = uint8_t((playerCodeStartAddr + 14) & 0xFF);
  memory[0x0315] = uint8_t((playerCodeStartAddr + 14) >> 8);
  for (size_t i = 0; i < (sizeof(playerCode) / sizeof(uint8_t)); i++)
    memory[playerCodeStartAddr + i] = playerCode[i];
  memory[playerCodeStartAddr + 15] = uint8_t(playAddress & 0xFF);
  memory[playerCodeStartAddr + 16] = uint8_t(playAddress >> 8);
  for (size_t i = 0; memory[playerCodeStartAddr + 23] != 0x00; i++) {
    if (i >= 4000000)
      throw Plus4Emu::Exception("PSID init routine did not return");
    cpu.runOneCycle_RDYHigh();
  }
  if (playAddress) {
    memory[0xFFFE] = uint8_t((playerCodeStartAddr + 6) & 0xFF);
    memory[0xFFFF] = uint8_t((playerCodeStartAddr + 6) >> 8);
  }
  uint16_t  ciaTimer =
      uint16_t(memory[0xDC04]) | (uint16_t(memory[0xDC05]) << 8);
  if (ciaTimer)
    irqPeriod = (unsigned int) ciaTimer + 1U;
  else if (!intFreq)
    irqPeriod = (!ntscFlag ? 16421U : 17045U);          // 60 Hz
  else
    irqPeriod = (!ntscFlag ? (63U * 312U) : (65U * 262U));
  irqFrequency = (!ntscFlag ? (17734475.0 / 18.0) : (14318180.0 / 14.0))
                 / double(int(irqPeriod));
  return true;
}

bool SIDPlayVM::convertPSIDFile(std::vector< uint8_t >& outBuf,
                                const std::vector< uint8_t >& inBuf,
                                int trackNum, int nSeconds)
{
  outBuf.clear();
  std::vector< uint8_t >  blockBuf(16384, 0x00);
  if (!loadPSIDFile(inBuf, trackNum))
    return false;
  std::fprintf(stderr, "IRQ frequency:  %.2f Hz\n", irqFrequency);
  int     intFreq = int(irqFrequency + 0.5);
  if (!(intFreq >= 50 && intFreq <= 300))
    throw Plus4Emu::Exception("interrupt frequency is out of range");
  for (int i = 0; i < 3; i++) {
    prvGateBit[i] = false;
    gateBitEdgeFlag[i] = false;
  }
  if (irqPeriod != (63U * 312U) && irqPeriod != (65U * 262U)) {
    blockBuf[0x3FFB] = uint8_t(irqPeriod & 0xFFU);
    blockBuf[0x3FFC] = uint8_t(irqPeriod >> 8);
  }
  blockBuf[0x3FFD] = uint8_t((intFreq - 50) & 0xFF);
  int     nFrames = -1;
  int     maxFrames =
      int(double(nSeconds > 0 ? nSeconds : 600) * irqFrequency + 0.5);
  irqFlag = false;
#if 0
  std::map< uint64_t, int > chkSums;
#endif
  do {
    unsigned int  irqTimer = irqPeriod;
    while (irqTimer != 0U && irqFlag) {
      cpu.interruptRequest();
      cpu.runOneCycle_RDYHigh();
      irqTimer--;
    }
    if (irqTimer) {
      cpu.clearInterruptRequest();
      cpu.run_RDYHigh(int(irqTimer));
    }
    irqFlag = true;
    nFrames++;
    if (!(nFrames & 127)) {
      std::fprintf(stderr, "\rConverting track %d: %d frames",
                   trackNum, nFrames);
    }
    if (nFrames > 0) {
      int     blockPos = (nFrames - 1) % 655;
      for (int i = 0; i < 25; i++)
        blockBuf[i * 655 + blockPos] = memory[0xD400 + i];
      for (int i = 0; i < 3; i++) {
        blockBuf[(i * 7 + 3) * 655 + blockPos] =
            (blockBuf[(i * 7 + 3) * 655 + blockPos] & 0x0F)
            | (uint8_t(gateBitEdgeFlag[i]) << 7);
      }
      if (blockPos == 654) {
        outBuf.insert(outBuf.end(), blockBuf.begin(), blockBuf.end());
        std::memset(&(blockBuf.front()), 0, sizeof(uint8_t) * blockBuf.size());
      }
    }
    for (int i = 0; i < 3; i++) {
      prvGateBit[i] = bool(memory[0xD400 + (i * 7 + 4)] & 0x01);
      gateBitEdgeFlag[i] = false;
    }
#if 0
    if (nSeconds <= 0) {
      if (nFrames >= 1) {
        uint32_t  h0 = 1U;
        uint32_t  h1 = 0U;
        for (int i = 0; i < 8192; i++) {
          if (i == 0x0030)
            i = 0x0040;                 // 0x0180 -> 0x0200
          else if (i == 0x1A80)
            i = 0x1C00;                 // 0xD400 -> 0xE000
          uint32_t  w0 = uint32_t(memBuffer[i] & 0xFFFFFFFFUL);
          uint32_t  w1 = uint32_t(memBuffer[i] >> 32);
          uint64_t  tmp = (h0 ^ w0) * uint64_t(0xC2B0C3CCU);
          h0 = uint32_t((tmp ^ (tmp >> 32)) & 0xFFFFFFFFUL);
          tmp = (h0 ^ w1) * uint64_t(0xC2B0C3CCU);
          h0 = uint32_t((tmp ^ (tmp >> 32)) & 0xFFFFFFFFUL);
          h1 = h1 + (w0 * uint32_t(i + i) * 113213U)
                  + (w1 * uint32_t(i + i + 1) * 267419U);
        }
        uint64_t  h = uint64_t(h0) | (uint64_t(h1) << 32);
        if (chkSums.find(h) != chkSums.end()) {
          if ((nFrames - chkSums[h]) >= 10)
            break;
        }
        chkSums.insert(std::pair< uint64_t, int >(h, nFrames));
      }
    }
#endif
  } while (nFrames < maxFrames);
  {
    int     blkFrames = nFrames % 655;
    if (!blkFrames) {
      blkFrames = 655;
    }
    else {
      outBuf.insert(outBuf.end(), blockBuf.begin(), blockBuf.end());
      if (blkFrames < ((intFreq + 1) >> 1))
        blkFrames = (intFreq + 1) >> 1;
    }
    outBuf[outBuf.size() - 2] = uint8_t(blkFrames & 0xFF);
    outBuf[outBuf.size() - 1] = uint8_t(blkFrames >> 8);
  }
  std::fprintf(stderr, "\rDone converting track %d: %d frames\n",
               trackNum, nFrames);
  return true;
}

// ----------------------------------------------------------------------------

static void loadInputFile(std::vector< uint8_t >& inBuf, const char *fileName)
{
  std::FILE *f = std::fopen(fileName, "rb");
  if (!f)
    throw Plus4Emu::Exception("error opening input file");
  try {
    int     c;
    while ((c = std::fgetc(f)) != EOF)
      inBuf.push_back(uint8_t(c & 0xFF));
    std::fclose(f);
    f = (std::FILE *) 0;
  }
  catch (...) {
    if (f)
      std::fclose(f);
    throw;
  }
}

static void saveOutputFile(const std::vector< uint8_t > & outBuf,
                           const char *fileName)
{
  std::FILE *f = std::fopen(fileName, "wb");
  if (!f)
    throw Plus4Emu::Exception("error opening output file");
  try {
    if (std::fwrite(&(outBuf.front()), sizeof(unsigned char), outBuf.size(), f)
        != outBuf.size() ||
        std::fflush(f) != 0) {
      throw Plus4Emu::Exception("error writing output file");
    }
    int     err = std::fclose(f);
    f = (std::FILE *) 0;
    if (err != 0)
      throw Plus4Emu::Exception("error closing output file");
  }
  catch (...) {
    if (f) {
      std::fclose(f);
      std::remove(fileName);
    }
    throw;
  }
}

// ----------------------------------------------------------------------------

static int parseTime(const char *s)
{
  if (!s)
    return -1;
  while ((unsigned char) *s <= 0x20 && *s != '\0')
    s++;
  if (*s == '\0')
    return -1;
  int     n = 0;
  while (*s >= '0' && *s <= '9') {
    n = n * 10 + int(*s - '0');
    s++;
  }
  while ((unsigned char) *s <= 0x20 && *s != '\0')
    s++;
  if (*s == ':') {
    s++;
    while ((unsigned char) *s <= 0x20 && *s != '\0')
      s++;
    if (*s == '\0')
      return -1;
    int     tmp = 0;
    while (*s >= '0' && *s <= '9') {
      tmp = tmp * 10 + int(*s - '0');
      s++;
    }
    n = n * 60 + tmp;
    while ((unsigned char) *s <= 0x20 && *s != '\0')
      s++;
  }
  if (*s == '\0')
    return n;
  if (*s != '(')
    return -1;
  s++;
  while (*s != ')') {
    if (!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')))
      return -1;
    if (*s == 'G' || *s == 'g')
      n++;
    s++;
  }
  s++;
  while ((unsigned char) *s <= 0x20 && *s != '\0')
    s++;
  if (*s == '\0')
    return n;
  return -1;
}

static void parseSongLengths(std::vector< int >& trackLengths,
                             int argc, char **argv)
{
  if (argc != 5 || parseTime(argv[3]) >= 0) {
    for (int i = 3; i < argc && (i - 3) < int(trackLengths.size()); i++) {
      int     nSeconds = parseTime(argv[i]);
      if (nSeconds < 0 || nSeconds > 43200)
        throw Plus4Emu::Exception("invalid track length");
      trackLengths[i - 3] = nSeconds;
    }
    return;
  }
  std::FILE *f = std::fopen(argv[3], "rb");
  if (!f)
    throw Plus4Emu::Exception("error opening songlengths.txt file");
  try {
    std::string lineBuf;
    std::string fileName;
    bool    fileFound = false;
    {
      const char  *s = argv[4];
      if (*s == '.')
        s++;
      if (*s == '/' || *s == '\\')
        s++;
      fileName = "; /";
      while (*s != '\0') {
        char    c = *(s++);
        if (c == '\\')
          c = '/';
        else if (c >= 'A' && c <= 'Z')
          c = c + ('a' - 'A');
        fileName += c;
      }
    }
    int     c;
    while ((c = std::fgetc(f)) != EOF) {
      c = c & 0xFF;
      if (c == '\0' || c == '\r' || c == '\n') {
        size_t  len = lineBuf.length();
        while (len > 0 && (unsigned char) lineBuf[len - 1] <= 0x20)
          len--;
        if (len < lineBuf.length())
          lineBuf.resize(len);
        if (!fileFound) {
          fileFound = (lineBuf == fileName);
        }
        else if (lineBuf.length() >= 33 && lineBuf[32] == '=') {
          bool    isMD5 = true;
          for (int i = 0; i < 32; i++) {
            if (!((lineBuf[i] >= '0' && lineBuf[i] <= '9') ||
                  (lineBuf[i] >= 'a' && lineBuf[i] <= 'f'))) {
              isMD5 = false;
              break;
            }
          }
          if (isMD5) {
            const char  *s = lineBuf.c_str() + 33;
            std::string t;
            for (int i = 0; i < int(trackLengths.size()); i++) {
              while ((unsigned char) *s <= 0x20 && *s != '\0')
                s++;
              if (*s == '\0')
                break;
              while ((unsigned char) *s > 0x20) {
                t += *s;
                s++;
              }
              int     nSeconds = parseTime(t.c_str());
              t.clear();
              if (nSeconds < 0)
                throw Plus4Emu::Exception("invalid track length");
              trackLengths[i] = nSeconds;
            }
            return;
          }
        }
        lineBuf.clear();
      }
      if (lineBuf.empty() && c <= ' ')
        continue;
      if (c >= 'A' && c <= 'Z')
        c = c + ('a' - 'A');
      lineBuf.push_back(char(c));
    }
    throw Plus4Emu::Exception("file not found in songlengths.txt");
  }
  catch (...) {
    std::fclose(f);
    throw;
  }
  std::fclose(f);
}

// ----------------------------------------------------------------------------

int main(int argc, char **argv)
{
  if (argc < 3) {
    std::fprintf(stderr, "Usage: %s INFILE OUTFILE [LENGTH1 [LENGTH2...]]\n"
                         "       %s INFILE OUTFILE [SONGLENGTHS FILENAME]\n",
                 argv[0], argv[0]);
    return -1;
  }
  try {
    SIDPlayVM   vm;
    std::vector< uint8_t >  inBuf;
    std::vector< uint8_t >  outBuf;
    std::vector< int >      trackLengths(32, 240);
    std::string nameBuf;
    if (argc > 3)
      parseSongLengths(trackLengths, argc, argv);
    loadInputFile(inBuf, argv[1]);
    for (int i = 1; i <= 32; i++) {
      int     nSeconds = trackLengths[i - 1];
      if (!nSeconds)
        nSeconds++;
      if (!vm.convertPSIDFile(outBuf, inBuf, i, nSeconds))
        break;
      nameBuf = argv[2];
      if (i > 1) {
        nameBuf += '-';
        nameBuf += ('0' + char(i / 10));
        nameBuf += ('0' + char(i % 10));
      }
      saveOutputFile(outBuf, nameBuf.c_str());
    }
  }
  catch (std::exception& e) {
    std::fprintf(stderr, " *** %s: %s\n", argv[0], e.what());
    return -1;
  }
  return 0;
}

