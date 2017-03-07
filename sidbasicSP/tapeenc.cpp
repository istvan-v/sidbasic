
#include <cstdio>
#include <cstdlib>
#include <vector>

static void tzxEncodeBlockN(std::vector< unsigned char >& outBuf,
                            const std::vector< unsigned char >& inBuf,
                            bool isHeader)
{
  outBuf.push_back(0x10);                       // standard speed data block
  outBuf.push_back((unsigned char) (1000 & 0xFF));      // pause ms after block
  outBuf.push_back((unsigned char) (1000 >> 8));
  size_t  n = inBuf.size() + 2;
  outBuf.push_back((unsigned char) (n & 0xFF));         // length of data
  outBuf.push_back((unsigned char) ((n >> 8) & 0xFF));
  unsigned char chkSum = (isHeader ? 0x00 : 0xFF);
  outBuf.push_back(chkSum);                             // block data
  for (size_t i = 0; i < inBuf.size(); i++) {
    chkSum = chkSum ^ inBuf[i];
    outBuf.push_back(inBuf[i]);
  }
  outBuf.push_back(chkSum);                     // XOR checksum of block data
}

static unsigned short calculateCRC(const std::vector< unsigned char >& buf)
{
  unsigned short  crcVal = 0xFFFF;
  for (size_t i = buf.size(); i-- > 0; ) {
    unsigned char b = buf[i];
    for (int j = 0; j < 8; j++) {
      if (crcVal & 0x0001)
        crcVal = ((crcVal ^ 0x1021) >> 1) | 0x8000;
      else
        crcVal = crcVal >> 1;
      crcVal = crcVal ^ ((unsigned short) (b & 0x01) << 15);
      b = b >> 1;
    }
  }
  return crcVal;
}

static void tzxEncodeBlockT(std::vector< unsigned char >& outBuf,
                            const std::vector< unsigned char >& inBuf,
                            unsigned short blockID)
{
  outBuf.push_back(0x14);                               // pure data block
  outBuf.push_back((unsigned char) (875 & 0xFF));       // bit 0 pulse length
  outBuf.push_back((unsigned char) (875 >> 8));
  outBuf.push_back((unsigned char) (583 & 0xFF));       // bit 1 pulse length
  outBuf.push_back((unsigned char) (583 >> 8));
  outBuf.push_back(0x08);                               // last byte bits
  outBuf.push_back((unsigned char) (1500 & 0xFF));      // pause ms after block
  outBuf.push_back((unsigned char) (1500 >> 8));
  size_t  n = inBuf.size() + 512;
  outBuf.push_back((unsigned char) (n & 0xFF));         // length of data
  outBuf.push_back((unsigned char) ((n >> 8) & 0xFF));
  outBuf.push_back((unsigned char) ((n >> 16) & 0xFF));
  for (int i = 0; i < 500; i++)                         // pilot sequence
    outBuf.push_back(0xD2);
  outBuf.push_back(0x8B);
  outBuf.push_back((unsigned char) (blockID & 0xFF));   // block ID
  outBuf.push_back((unsigned char) ((blockID >> 8) & 0xFF));
  n = inBuf.size();
  outBuf.push_back((unsigned char) (n & 0xFF));         // block size
  outBuf.push_back((unsigned char) ((n >> 8) & 0xFF));
  unsigned short  crcVal = calculateCRC(inBuf);
  outBuf.push_back((unsigned char) (crcVal & 0xFF));    // CRC
  outBuf.push_back((unsigned char) ((crcVal >> 8) & 0xFF));
  for (size_t i = 0; i < inBuf.size(); i++)             // block data
    outBuf.push_back(inBuf[i]);
  for (int i = 0; i < 5; i++)
    outBuf.push_back(0xD2);
}

int main(int argc, char **argv)
{
  if (argc < 4) {
    std::fprintf(stderr,
                 "Usage: %s OUTFILE.TZX NAME LOADER.BIN [ID1 INFILE1 [...]]\n",
                 argv[0]);
    return -1;
  }
  std::vector< unsigned char >  inBuf;
  std::vector< unsigned char >  outBuf;
  outBuf.push_back(0x5A);               // 'Z'
  outBuf.push_back(0x58);               // 'X'
  outBuf.push_back(0x54);               // 'T'
  outBuf.push_back(0x61);               // 'a'
  outBuf.push_back(0x70);               // 'p'
  outBuf.push_back(0x65);               // 'e'
  outBuf.push_back(0x21);               // '!'
  outBuf.push_back(0x1A);               // ^Z
  outBuf.push_back(0x01);               // major version
  outBuf.push_back(0x0D);               // minor version
  {
    inBuf.resize(21);
    std::FILE *f = std::fopen(argv[3], "rb");
    while (true) {
      int     c = std::fgetc(f);
      if (c == EOF)
        break;
      inBuf.push_back((unsigned char) (c & 0xFF));
    }
    std::fclose(f);
    inBuf.push_back(0x0D);
    inBuf[0] = 0x00;
    inBuf[1] = 0x0A;                    // line number
    inBuf[2] = (unsigned char) ((inBuf.size() - 4) & 0xFF);     // line length
    inBuf[3] = (unsigned char) ((inBuf.size() - 4) >> 8);
    inBuf[4] = 0x20;
    inBuf[5] = 0xF9;                    // RANDOMIZE
    inBuf[6] = 0xC0;                    // USR
    inBuf[7] = 0x32;                    // "23776" (= 0x5CE0)
    inBuf[8] = 0x33;
    inBuf[9] = 0x37;
    inBuf[10] = 0x37;
    inBuf[11] = 0x36;
    inBuf[12] = 0x0E;
    inBuf[13] = 0x00;
    inBuf[14] = 0x00;
    inBuf[15] = 0xE0;                   // loader start address = 0x5CE0
    inBuf[16] = 0x5C;
    inBuf[17] = 0x00;
    inBuf[18] = 0x3A;                   // ':'
    inBuf[19] = 0xEA;                   // REM
    inBuf[20] = 0x20;
    std::vector< unsigned char >  hdrBuf(17, 0x20);
    hdrBuf[0] = 0x00;
    for (size_t i = 0; i < 10 && argv[2][i] != '\0'; i++)
      hdrBuf[i + 1] = (unsigned char) argv[2][i];
    hdrBuf[11] = (unsigned char) (inBuf.size() & 0xFF);
    hdrBuf[12] = (unsigned char) (inBuf.size() >> 8);
    hdrBuf[13] = 0x0A;                  // line number to start program at
    hdrBuf[14] = 0x00;
    hdrBuf[15] = (unsigned char) (inBuf.size() & 0xFF);
    hdrBuf[16] = (unsigned char) (inBuf.size() >> 8);
    tzxEncodeBlockN(outBuf, hdrBuf, true);
    tzxEncodeBlockN(outBuf, inBuf, false);
  }
  for (int i = 4; (i + 1) < argc; i += 2) {
    std::FILE *f = std::fopen(argv[i + 1], "rb");
    inBuf.clear();
    while (true) {
      int     c = std::fgetc(f);
      if (c == EOF)
        break;
      inBuf.push_back((unsigned char) (c & 0xFF));
    }
    std::fclose(f);
    char    *endp = (char *) 0;
    unsigned short  blockID = (unsigned short) std::strtol(argv[i], &endp, 0);
    tzxEncodeBlockT(outBuf, inBuf, blockID);
  }
  std::FILE *f = std::fopen(argv[1], "wb");
  std::fwrite(&(outBuf.front()), sizeof(unsigned char), outBuf.size(), f);
  std::fflush(f);
  std::fclose(f);
  return 0;
}

