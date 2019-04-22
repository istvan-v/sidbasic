
// sid_conv.cpp: utility to convert SID music to Enterprise 128 format
// Copyright (C) 2017-2019 Istvan Varga <istvanv@users.sourceforge.net>
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

#include "ep128emu.hpp"
#include "compress.hpp"

class SID_Channel {
 public:
  unsigned char   r[7];
  unsigned char   volume;
  unsigned short  envRate;
  unsigned short  envCounter1;
  unsigned char   envCounter2;
  unsigned char   envState;     // 0: attack, 1: decay/sustain, 2: release
  unsigned char   expRate;
  unsigned char   expCounter;
  bool            holdZero;
  bool            noADSRBug;
  // --------
  SID_Channel();
  unsigned char calculateVolume(int nCycles);
  unsigned char getFrequencyL() const
  {
    if (!(r[4] & 0x80))
      return r[0];
    else
      return (((((unsigned int) r[1] << 8) | r[0]) >> 4) & 0xFF);
  }
  unsigned char getFrequencyH() const
  {
    if (!(r[4] & 0x80))
      return r[1];
    else
      return (r[1] >> 4);
  }
  unsigned char getWaveform() const
  {
    unsigned char rmFlag = ((r[4] & 0x24) == 0x04 ? 0x80 : 0x00);
    if ((r[4] & 0x08) != 0 || (r[4] & 0xF0) == 0)
      return 0x40;                      // test / no sound
    if (r[4] & 0x80)
      return (rmFlag | 0x60);           // noise
    if (r[4] & 0x20)
      return (rmFlag | 0x20);           // sawtooth
    if (r[4] & 0x10)
      return rmFlag;                    // triangle
    return (rmFlag | 0x40);             // pulse
  }
  unsigned char getPulseWidth() const
  {
    if ((r[4] & 0xF8) != 0x40)
      return 0x00;                      // not pulse waveform
    return (unsigned char) ((((unsigned int) (r[3] & 0x0F) << 8) | r[2]) >> 4);
  }
};

static const unsigned short envRateTable[16] = {
  // from resid/envelope.cpp
      9,    32,    63,    95,   149,   220,   267,   313,
    392,   977,  1954,  3126,  3907, 11720, 19532, 31251
};

SID_Channel::SID_Channel()
  : volume(0),
    envRate(9),
    envCounter1(0),
    envCounter2(0),
    envState(2),
    expRate(1),
    expCounter(0),
    holdZero(true),
    noADSRBug(false)
{
  for (int i = 0; i < 7; i++)
    r[i] = 0x00;
}

unsigned char SID_Channel::calculateVolume(int nCycles)
{
  double        tmp = 0.0;
  unsigned char sustainLevel = (unsigned char) (((r[6] >> 4) & 0x0F) * 17);
  if ((r[4] & 0x01) != 0 && (envState == 2 || (r[3] & 0x80) != 0)) {
    envState = 0;
    holdZero = false;
    if (noADSRBug)
      envCounter1 = 0;
  }
  else if ((r[4] & 0x01) == 0) {
    envState = 2;
  }
  if (envState == 0)
    envRate = envRateTable[(r[5] >> 4) & 0x0F];
  else if (envState == 1)
    envRate = envRateTable[r[5] & 0x0F];
  else
    envRate = envRateTable[r[6] & 0x0F];
  for (int i = 0; i < nCycles; i++) {
    if (envCounter1 < envRate) {
      int     n = int(envRate - envCounter1);
      if ((nCycles - i) < n)
        n = nCycles - i;
      n--;
      envCounter1 = envCounter1 + (unsigned short) n;
      i = i + n;
      tmp += (double(int(envCounter2) * int(envCounter2)) * double(n));
    }
    envCounter1++;
    if (EP128EMU_UNLIKELY(envCounter1 >= 0x8000))
      envCounter1 = 1;
    if (envCounter1 == envRate) {
      envCounter1 = 0;
      if (envState == 0) {              // attack
        envCounter2 = (envCounter2 + 1) & 0xFF;
        expCounter = 0;
        if (envCounter2 == 0xFF) {
          envState = 1;
          envRate = envRateTable[r[5] & 0x0F];
        }
      }
      else {                            // decay, sustain, release
        expCounter = (expCounter + 1) & 0xFF;
        if (expCounter == expRate) {
          expCounter = 0;
          if (!holdZero && !(envState == 1 && envCounter2 == sustainLevel)) {
            envCounter2 = (envCounter2 - 1) & 0xFF;
            holdZero = !envCounter2;
          }
        }
      }
      switch (envCounter2) {
      case 0:
      case 255:
        expRate = 1;
        break;
      case 6:
        expRate = 30;
        break;
      case 14:
        expRate = 16;
        break;
      case 26:
        expRate = 8;
        break;
      case 54:
        expRate = 4;
        break;
      case 93:
        expRate = 2;
        break;
      }
    }
    tmp += double(int(envCounter2) * int(envCounter2));
  }
  if (!(r[4] & 0xF0))
    return 0x00;
  return (unsigned char) int(std::sqrt(tmp / double(nCycles)) * volume * 31.0
                             / (15.0 * 255.0) + 0.5);
}

// ----------------------------------------------------------------------------

static size_t loadInputFile(std::vector< std::vector< unsigned char > >& inBufs,
                            const char *fileName, int& intFreq, int& irqPeriod)
{
  inBufs.resize(25);
  std::FILE *f = std::fopen(fileName, "rb");
  if (!f)
    throw Ep128Emu::Exception("error opening input file");
  try {
    std::vector< unsigned char >  buf1;
    int     c;
    while ((c = std::fgetc(f)) != EOF)
      buf1.push_back((unsigned char) (c & 0xFF));
    std::fclose(f);
    f = (std::FILE *) 0;
    if (!(buf1.size() >= 0x4000 && (buf1.size() & 0x3FFF) == 0 &&
          (buf1[0x3FF7] | buf1[0x3FF8]
           | buf1[0x3FF9] | buf1[0x3FFA]) == 0x00)) {
      std::vector< unsigned char >  buf2;
      try {
        Ep128Compress::decompressData(buf2, buf1, -1);
        buf1.clear();
        buf1.insert(buf1.end(), buf2.begin(), buf2.end());
      }
      catch (...) {
      }
    }
    if (buf1.size() < 0x4000 || (buf1.size() & 0x3FFF) != 0)
      throw Ep128Emu::Exception("invalid input file format");
    if (intFreq <= 0) {
      irqPeriod = int(buf1[0x3FFB]) | (int(buf1[0x3FFC]) << 8);
      intFreq = int(buf1[0x3FFD]) + 50;
    }
    size_t  blkFrames = 655;
    for (size_t i = 0; i < buf1.size(); i++) {
      size_t  blkPos = i & 0x3FFF;
      size_t  n = blkPos / 655;
      size_t  p = blkPos % 655;
      if (!blkPos) {
        blkFrames = size_t(buf1[i + 0x3FFE]) | (size_t(buf1[i + 0x3FFF]) << 8);
        if (blkFrames < 1 || blkFrames > 655)
          blkFrames = 655;
      }
      if (n < 25 && p < blkFrames)
        inBufs[n].push_back(buf1[i]);
    }
  }
  catch (...) {
    if (f)
      std::fclose(f);
    throw;
  }
  return inBufs[0].size();
}

static void saveOutputFile(std::vector< unsigned char >& outBuf,
                           const char *fileName, int intFreq,
                           size_t blockSize, size_t nFrames)
{
  Ep128Compress::Compressor *compressor = (Ep128Compress::Compressor *) 0;
  std::FILE *f = std::fopen(fileName, "wb");
  if (!f)
    throw Ep128Emu::Exception("error opening output file");
  try {
    std::vector< unsigned char >  tmpBuf;
    while (true) {
      compressor = Ep128Compress::createCompressor(2, tmpBuf);
      Ep128Compress::Compressor::CompressionParameters  cfg;
      compressor->setCompressionLevel(9);
      compressor->getCompressionParameters(cfg);
      cfg.maxOffset = blockSize << 1;
      cfg.blockSize = blockSize;
      compressor->setCompressionParameters(cfg);
      compressor->compressData(outBuf, 0xFFFFFFFFU, true, true);
      delete compressor;
      compressor = (Ep128Compress::Compressor *) 0;
      if (tmpBuf.size() <= 0x5F00)
        break;
      size_t  newSize = (outBuf.size() / blockSize) * (0x5F00 + (0x5F00 >> 1))
                        / (tmpBuf.size() + (0x5F00 >> 1));
      nFrames = newSize * ((blockSize - 2) / 12);
      newSize = newSize * blockSize;
      tmpBuf.clear();
      outBuf.resize(newSize);
      std::fprintf(stderr,
                   "WARNING: output file size exceeds maximum, "
                   "truncating data to %lu frames\n",
                   (unsigned long) nFrames);
    }
    unsigned char hdrBuf[16];
    for (int i = 0; i < 16; i++)
      hdrBuf[i] = 0x00;
    hdrBuf[1] = 0x4F;
    hdrBuf[2] = (unsigned char) (tmpBuf.size() & 0xFF);
    hdrBuf[3] = (unsigned char) (tmpBuf.size() >> 8);
    hdrBuf[4] = (unsigned char) (intFreq & 0xFF);
    hdrBuf[5] = (unsigned char) (intFreq >> 8);
    hdrBuf[6] = (unsigned char) (nFrames & 0xFF);
    hdrBuf[7] = (unsigned char) ((nFrames >> 8) & 0xFF);
    hdrBuf[8] = (unsigned char) (nFrames >> 16);
    if (std::fwrite(&(hdrBuf[0]), sizeof(unsigned char), 16, f) != 16 ||
        std::fwrite(&(tmpBuf.front()), sizeof(unsigned char), tmpBuf.size(), f)
        != tmpBuf.size() ||
        std::fflush(f) != 0) {
      throw Ep128Emu::Exception("error writing output file");
    }
    int     err = std::fclose(f);
    f = (std::FILE *) 0;
    if (err != 0)
      throw Ep128Emu::Exception("error closing output file");
  }
  catch (...) {
    if (compressor)
      delete compressor;
    if (f) {
      std::fclose(f);
      std::remove(fileName);
    }
    throw;
  }
}

// ----------------------------------------------------------------------------

int main(int argc, char **argv)
{
  if (argc < 3 || argc > 6) {
    std::fprintf(stderr,
                 "Usage: %s INFILE OUTFILE [INTFREQ [BLKSIZE [NOADSRBUG]]]\n",
                 argv[0]);
    return -1;
  }
  try {
    std::vector< std::vector< unsigned char > >   buf;
    size_t  blockSize = 8192;
    int     intFreq = -1;
    int     irqPeriod = -1;
    if (argc >= 4) {
      intFreq = int(std::atoi(argv[3]));
      if (intFreq > 0 && (intFreq < 50 || intFreq > 300)) {
        throw Ep128Emu::Exception("invalid interrupt frequency, "
                                  "must be in the range 50 to 300");
      }
      if (argc >= 5) {
        blockSize = size_t(std::atoi(argv[4]));
        if (blockSize <= 0) {
          blockSize = 8192;
        }
        else if (blockSize < 256 || blockSize > 16384 ||
                 (blockSize & (blockSize - 1)) != 0) {
          throw Ep128Emu::Exception("invalid block size");
        }
      }
    }
    std::vector< unsigned char >  outBuf;
    std::vector< unsigned char >  blockBuf(blockSize, 0x00);
    int     intFreqMult = int(intFreq < -1) + 1;
    size_t  nFrames = loadInputFile(buf, argv[1], intFreq, irqPeriod);

    std::fprintf(stderr, "Converting file...\n");
    SID_Channel sid[3];
    size_t  blkFrames = (blockSize - 2) / 12;
    size_t  blkPos = 0;
    if (irqPeriod < 1000) {
      if (intFreq == 50)
        irqPeriod = 63 * 312;           // PAL
      else if (intFreq == 60)
        irqPeriod = 65 * 262;           // NTSC
      else
        irqPeriod = 1000000 / intFreq;
    }
    if (intFreqMult > 1) {
      intFreq = intFreq << 1;
      irqPeriod = irqPeriod >> 1;
    }
    if (argc >= 6) {
      sid[0].noADSRBug = bool(std::atoi(argv[5]));
      sid[1].noADSRBug = sid[0].noADSRBug;
      sid[2].noADSRBug = sid[0].noADSRBug;
    }
    int     prvPercentage = -1;
    for (size_t i = 0; i < nFrames; i++) {
      {
        int     tmp = int((i + 1) * 100 / nFrames);
        if (tmp != prvPercentage) {
          prvPercentage = tmp;
          std::fprintf(stderr, "\r%5d%%", tmp);
          if (tmp == 100)
            std::fputc('\r', stderr);
        }
      }
      for (size_t j = 0; j < 21; j++)
        sid[j / 7].r[j % 7] = buf[j][i];
      sid[0].volume = buf[24][i] & 0x0F;
      sid[1].volume = sid[0].volume;
      sid[2].volume = sid[0].volume;
      for (int k = intFreqMult; k--; ) {
        for (size_t j = 0; j < 12; j++) {
          switch (j & 3) {
          case 0:
            blockBuf[j * blkFrames + blkPos] = sid[j >> 2].getFrequencyL();
            break;
          case 1:
            blockBuf[j * blkFrames + blkPos] = sid[j >> 2].getFrequencyH();
            break;
          case 2:
            blockBuf[j * blkFrames + blkPos] =
                sid[j >> 2].getWaveform()
                | sid[j >> 2].calculateVolume(irqPeriod);
            break;
          case 3:
            if (!(blockBuf[(j - 1) * blkFrames + blkPos] & 0x1F)) {
              // volume = 0
              blockBuf[(j - 1) * blkFrames + blkPos] = 0x40;
              blockBuf[j * blkFrames + blkPos] = 0x00;
            }
            else {
              blockBuf[j * blkFrames + blkPos] = sid[j >> 2].getPulseWidth();
            }
            break;
          }
        }
        sid[0].r[3] = sid[0].r[3] & 0x0F;
        sid[1].r[3] = sid[1].r[3] & 0x0F;
        sid[2].r[3] = sid[2].r[3] & 0x0F;
        blkPos++;
        if (blkPos >= blkFrames || ((i + 1) >= nFrames && !k)) {
          if (blkPos < size_t((intFreq + 1) >> 1))
            blkPos = size_t(intFreq + 1) >> 1;
          blockBuf[blockSize - 2] =
              (unsigned char) ((blkFrames - blkPos) & 0xFF);
          blockBuf[blockSize - 1] =
              (unsigned char) ((blkFrames - blkPos) >> 8);
          outBuf.insert(outBuf.end(), blockBuf.begin(), blockBuf.end());
          std::memset(&(blockBuf.front()), 0x00, blockBuf.size());
          blkPos = 0;
        }
      }
    }
    std::fputc('\n', stderr);

    saveOutputFile(outBuf, argv[2], intFreq, blockSize, nFrames * intFreqMult);
  }
  catch (std::exception& e) {
    std::fprintf(stderr, " *** %s: %s\n", argv[0], e.what());
    return -1;
  }
  return 0;
}

