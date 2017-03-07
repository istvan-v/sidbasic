
#include <stdio.h>

int main(int argc, char **argv)
{
  unsigned char buf[8192];
  unsigned char buf2[4096 + 768];
  int i, j;
  FILE *f = fopen(argv[1], "rb");
  fread(buf, 1, 8192, f);
  fclose(f);
  for (i = 0; i < 4096; i++) {
    j = 0x0800 + ((i & 7) << 8) + ((i & 0x07F8) >> 3) + (i & 0x0800);
    buf2[i] = buf[j];
  }
  for (i = 0; i < 768; i++)
    buf2[i + 4096] = buf[i + 6144];
  f = fopen(argv[2], "wb");
  fwrite(buf2, 1, 4096 + 768, f);
  fclose(f);
  return 0;
}

