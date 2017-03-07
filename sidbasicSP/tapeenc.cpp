
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void tapEncodeBlock(std::vector< unsigned char >& outBuf,
                           const std::vector< unsigned char >& inBuf,
                           bool isHeader)
{
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

static void loadFile(std::vector< unsigned char >& buf, const char *fileName)
{
  std::FILE *f = std::fopen(fileName, "rb");
  if (!f) {
    std::fprintf(stderr, " *** error opening input file \"%s\"\n", fileName);
    std::exit(-1);
  }
  int     c;
  while ((c = std::fgetc(f)) != EOF)
    buf.push_back((unsigned char) (c & 0xFF));
  std::fclose(f);
  if (buf.size() < 1) {
    std::fprintf(stderr, " *** empty input file \"%s\"\n", fileName);
    std::exit(-1);
  }
}

int main(int argc, char **argv)
{
  std::string   outFileName;
  std::string   tapeName;
  std::string   loaderFileName;
  std::vector< std::string >  inFileNames;
  std::vector< std::string >  inFileIDs;
  bool    tapFormat = false;
  bool    noLoader = false;
  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "-tzx") == 0) {
      tapFormat = false;
    }
    else if (std::strcmp(argv[i], "-tap") == 0) {
      tapFormat = true;
    }
    else if (std::strcmp(argv[i], "-ldr") == 0) {
      noLoader = false;
    }
    else if (std::strcmp(argv[i], "-noldr") == 0) {
      noLoader = true;
    }
    else if (std::strcmp(argv[i], "-h") == 0 ||
             std::strcmp(argv[i], "-help") == 0 ||
             std::strcmp(argv[i], "--help") == 0) {
      break;
    }
    else {
      for ( ; i < argc; i++) {
        const char  *s = argv[i];
        if (*s == '\0')
          s = " ";
        if (outFileName.empty())
          outFileName = s;
        else if (tapeName.empty() && !(noLoader && !tapFormat))
          tapeName = s;
        else if (loaderFileName.empty() && !noLoader)
          loaderFileName = s;
        else if (inFileIDs.size() <= inFileNames.size() && !tapFormat)
          inFileIDs.push_back(std::string(s));
        else
          inFileNames.push_back(std::string(s));
      }
      break;
    }
  }
  if (outFileName.empty() || inFileNames.size() < inFileIDs.size() ||
      (loaderFileName.empty() && inFileNames.size() < 1)) {
    std::fprintf(stderr,
                 "Usage: %s OUTFILE.TZX NAME LOADER.BIN [ID1 INFILE1 [...]]\n"
                 "       %s -tap OUTFILE.TAP NAME LOADER.BIN [INFILE1 [...]]\n"
                 "       %s -noldr OUTFILE.TZX ID1 INFILE1 [ID2 INFILE2...]\n"
                 "       %s -tap -noldr OUTFILE.TAP NAME INFILE1 [...]\n",
                 argv[0], argv[0], argv[0], argv[0]);
    return -1;
  }
  std::vector< unsigned char >  loaderData;
  std::vector< std::vector< unsigned char > > inFileData;
  if (!loaderFileName.empty()) {
    loaderData.resize(21, 0x00);
    loadFile(loaderData, loaderFileName.c_str());
    loaderData.push_back(0x0D);
    loaderData[0] = 0x00;
    loaderData[1] = 0x0A;               // line number
    size_t  n = loaderData.size() - 4;  // line length
    loaderData[2] = (unsigned char) (n & 0xFF);
    loaderData[3] = (unsigned char) (n >> 8);
    loaderData[4] = 0x20;
    loaderData[5] = 0xF9;               // RANDOMIZE
    loaderData[6] = 0xC0;               // USR
    loaderData[7] = 0x32;               // "23776" (= 0x5CE0)
    loaderData[8] = 0x33;
    loaderData[9] = 0x37;
    loaderData[10] = 0x37;
    loaderData[11] = 0x36;
    loaderData[12] = 0x0E;
    loaderData[13] = 0x00;
    loaderData[14] = 0x00;
    loaderData[15] = 0xE0;              // loader start address = 0x5CE0
    loaderData[16] = 0x5C;
    loaderData[17] = 0x00;
    loaderData[18] = 0x3A;              // ':'
    loaderData[19] = 0xEA;              // REM
    loaderData[20] = 0x20;
  }
  if (inFileNames.size() > 0) {
    inFileData.resize(inFileNames.size());
    for (size_t i = 0; i < inFileNames.size(); i++)
      loadFile(inFileData[i], inFileNames[i].c_str());
  }
  std::vector< unsigned char >  outBuf;
  if (!tapFormat) {
    outBuf.push_back(0x5A);             // 'Z'
    outBuf.push_back(0x58);             // 'X'
    outBuf.push_back(0x54);             // 'T'
    outBuf.push_back(0x61);             // 'a'
    outBuf.push_back(0x70);             // 'p'
    outBuf.push_back(0x65);             // 'e'
    outBuf.push_back(0x21);             // '!'
    outBuf.push_back(0x1A);             // ^Z
    outBuf.push_back(0x01);             // major version
    outBuf.push_back(0x0D);             // minor version
  }
  if (!tapeName.empty()) {
    std::vector< unsigned char >  hdrBuf(17, 0x20);
    hdrBuf[0] = 0x00;
    const char  *p = tapeName.c_str();
    for (size_t i = 0; i < 10; i++) {
      char    c = *p;
      if (c != '\0')
        p++;
      if (c >= 'a' && c <= 'z')
        c = c - ('a' - 'A');
      else if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '_'))
        c = ' ';
      hdrBuf[i + 1] = (unsigned char) c;
    }
    size_t  dataSize = loaderData.size();
    if (dataSize < 1)
      dataSize = inFileData[0].size();
    hdrBuf[11] = (unsigned char) (dataSize & 0xFF);
    hdrBuf[12] = (unsigned char) (dataSize >> 8);
    if (!loaderFileName.empty()) {
      hdrBuf[13] = 0x0A;                // line number to start program at
      hdrBuf[14] = 0x00;
    }
    else {
      hdrBuf[13] = 0x00;
      hdrBuf[14] = 0x80;
    }
    hdrBuf[15] = (unsigned char) (dataSize & 0xFF);
    hdrBuf[16] = (unsigned char) (dataSize >> 8);
    if (!tapFormat)
      tzxEncodeBlockN(outBuf, hdrBuf, true);
    else
      tapEncodeBlock(outBuf, hdrBuf, true);
  }
  if (loaderData.size() > 0) {
    if (!tapFormat)
      tzxEncodeBlockN(outBuf, loaderData, false);
    else
      tapEncodeBlock(outBuf, loaderData, false);
  }
  for (size_t i = 0; i < inFileNames.size(); i++) {
    unsigned short  blockID = 0;
    if (i < inFileIDs.size()) {
      char    *endp = (char *) 0;
      blockID = (unsigned short) std::strtol(inFileIDs[i].c_str(), &endp, 0);
    }
    if (!tapFormat)
      tzxEncodeBlockT(outBuf, inFileData[i], blockID);
    else
      tapEncodeBlock(outBuf, inFileData[i], false);
  }
  std::FILE *f = std::fopen(outFileName.c_str(), "wb");
  if (!f) {
    std::fprintf(stderr, " *** error opening output file \"%s\"\n",
                 outFileName.c_str());
    return -1;
  }
  if (std::fwrite(&(outBuf.front()), sizeof(unsigned char), outBuf.size(), f)
      != outBuf.size() || std::fflush(f) != 0) {
    std::fprintf(stderr, " *** error writing output file \"%s\"\n",
                 outFileName.c_str());
    return -1;
  }
  std::fclose(f);
  return 0;
}

