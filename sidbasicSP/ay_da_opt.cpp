
#include <cstdio>
#include <cstdlib>
#include <cmath>

static const int ayLevelTable[16] = {
#if 0
#  if !(defined(USE_DACTABLE_AY) || defined(USE_DACTABLE_YM))
  // FUSE
      0,   300,   447,   635,   925,  1351,  1851,  2991,
   3695,  5782,  7705,  9829, 12460, 15014, 18528, 21845
#  elif defined(USE_DACTABLE_AY)
  // ayumi / AY
      0,   218,   316,   460,   671,   995,  1409,  2345,
   2765,  4478,  6383,  8145, 10759, 13879, 17598, 21845
#  else
  // ayumi / YM
      0,   169,   305,   437,   649,   882,  1274,  1699,
   2427,  3244,  4621,  6141,  8747, 11675, 16559, 21845
#  endif
#endif
#if 0
#  ifndef USE_DACTABLE_YM
  // based on tables used in Unreal Speccy (SNDRENDER/sndchip.cpp)
      0,   277,   405,   593,   876,  1286,  1797,  2943,
   3466,  5573,  7776,  9757, 12319, 15472, 18400, 21845
#  else
      0,   155,   270,   411,   641,   871,  1255,  1675,
   2386,  3207,  4565,  6090,  8715, 11628, 16470, 21845
#  endif
#endif
#ifndef USE_DACTABLE_YM
  // average levels from FUSE and Unreal Speccy
      0,   289,   426,   614,   901,  1319,  1824,  2967,
   3581,  5677,  7741,  9793, 12390, 15243, 18464, 21845
#else
  // average levels from ayumi and Unreal Speccy
      0,   162,   288,   424,   645,   877,  1265,  1687,
   2406,  3225,  4593,  6116,  8731, 11652, 16515, 21845
#endif
};

static void fft(float *buf, size_t n, bool isInverse)
{
  // check FFT size
  if (n < 16 || n > 65536 || (n & (n - 1)) != 0)
    return;
  if (!isInverse) {
    // convert real data to interleaved real/imaginary format
    size_t  i = n;
    do {
      i--;
      buf[(i << 1) + 0] = buf[i];
      buf[(i << 1) + 1] = 0.0f;
    } while (i);
  }
  else {
    buf[1] = 0.0f;
    buf[n + 1] = 0.0f;
    for (size_t i = 1; i < (n >> 1); i++) {
      buf[((n - i) << 1) + 0] = buf[(i << 1) + 0];
      buf[((n - i) << 1) + 1] = -(buf[(i << 1) + 1]);
    }
  }
  // pack data in reverse bit order
  size_t  i, j;
  for (i = 0, j = 0; i < n; i++) {
    if (i < j) {
      float   tmp1 = buf[(i << 1) + 0];
      float   tmp2 = buf[(i << 1) + 1];
      buf[(i << 1) + 0] = buf[(j << 1) + 0];
      buf[(i << 1) + 1] = buf[(j << 1) + 1];
      buf[(j << 1) + 0] = tmp1;
      buf[(j << 1) + 1] = tmp2;
    }
    for (size_t k = (n >> 1); k > 0; k >>= 1) {
      j ^= k;
      if ((j & k) != 0)
        break;
    }
  }
  // calculate FFT
  for (size_t k = 1; k < n; k <<= 1) {
    double  ph_inc_re = std::cos(4.0 * std::atan(1.0) / double(long(k)));
    double  ph_inc_im = std::sin((isInverse ? 4.0 : -4.0)
                                 * std::atan(1.0) / double(long(k)));
    for (j = 0; j < n; j += (k << 1)) {
      double  ph_re = 1.0;
      double  ph_im = 0.0;
      for (i = j; i < (j + k); i++) {
        double  re1 = buf[(i << 1) + 0];
        double  im1 = buf[(i << 1) + 1];
        double  re2 = buf[((i + k) << 1) + 0];
        double  im2 = buf[((i + k) << 1) + 1];
        buf[(i << 1) + 0] = float(re1 + re2 * ph_re - im2 * ph_im);
        buf[(i << 1) + 1] = float(im1 + re2 * ph_im + im2 * ph_re);
        buf[((i + k) << 1) + 0] = float(re1 - re2 * ph_re + im2 * ph_im);
        buf[((i + k) << 1) + 1] = float(im1 - re2 * ph_im - im2 * ph_re);
        double  tmp = ph_re * ph_inc_re - ph_im * ph_inc_im;
        ph_im = ph_re * ph_inc_im + ph_im * ph_inc_re;
        ph_re = tmp;
      }
    }
  }
  if (!isInverse) {
    buf[1] = 0.0f;
    buf[n + 1] = 0.0f;
  }
  else {
    // convert from interleaved real/imaginary format to pure real data
    for (i = 0; i < n; i++)
      buf[i] = buf[(i << 1) + 0];
    for (i = n; i < (n << 1); i++)
      buf[i] = 0.0f;
  }
}

static double calculateDistortion(const unsigned char *dacTable,
                                  const char *fileName = (char *) 0)
{
  float   fftBuf[16384];
  std::FILE *f = (std::FILE *) 0;
  if (fileName)
    f = std::fopen(fileName, "wb");
  for (int i = 0; i < 16384; i++)
    fftBuf[i] = 0.0f;
  int     minVal = 32767;
  int     maxVal = -32768;
  int     a = 0;
  int     b = 0;
  int     c = 0;
  int     y = 0;
  for (int i = 0; i < 16384; i++) {
    switch (i & 7) {
    case 0:
      y = int(std::sin(double(i >> 3) * (8.0 * std::atan(1.0) / 1024.0)) * 95.5
              + 96.0);
      a = ayLevelTable[dacTable[y]];
      break;
    case 2:
      b = ayLevelTable[dacTable[y + 257]];
      break;
    case 4:
      c = ayLevelTable[dacTable[y + 512]];
      break;
    }
    int     tmp = a + b + c;
    if (i < 8192) {
      if (i >= 8) {
        minVal = (tmp < minVal ? tmp : minVal);
        maxVal = (tmp > maxVal ? tmp : maxVal);
      }
    }
    else {
      tmp = int((double(tmp) + (double(minVal - maxVal) * 0.5))
                * (65534.0 / double(maxVal - minVal)) + 32768.5) - 32768;
      fftBuf[i & 8191] = float(tmp);
      if (f) {
        std::fputc(tmp & 0xFF, f);
        std::fputc((tmp & 0xFF00) >> 8, f);
      }
    }
  }
  if (f)
    std::fclose(f);
  fft(&(fftBuf[0]), 8192, false);
  double  r = 0.0;
  double  d = 0.0;
  for (int i = 1; i <= 4096; i++) {
    float   re = fftBuf[i << 1] * (1.0f / 8192.0f);
    float   im = fftBuf[(i << 1) + 1] * (1.0f / 8192.0f);
    if (i == 1) {
      r = double(re * re) + double(im * im);
    }
    else {
      d = d + ((double(re * re) + double(im * im))
               * (double(i < 15 ? i : 15) * 0.33333333));
    }
  }
  d = std::sqrt(d / r) * 100.0;
  return d;
}

int main(int argc, char **argv)
{
  unsigned char dacTable[768];
  for (int i = 0; i < 768; i++)
    dacTable[i] = 0;
  std::FILE *f = std::fopen("dactable.txt", "w");
#ifndef USE_DACTABLE_YM
  int     lvlMult = 249;
#else
  int     lvlMult = 247;
#endif
  if (argc > 1)
    lvlMult = std::atoi(argv[1]);
  int     bestErrMult = 1;
  double  bestDistortion = 1000000.0;
  const char  *fileName = (char *) 0;
  for (int m = 8; true; m++) {
    if (fileName)
      m = bestErrMult;
    int     prvB = 0;
    int     prvC = 0;
    for (int i = 0; i < 192; i++) {
      int     v = i * lvlMult;
      int     bestA = 0;
      int     bestB = 0;
      int     bestC = 0;
      double  minErr = 1.0e24;
      for (int c = prvC; c < 16; c++) {
        for (int b = 0; b < 16; b++) {
          for (int a = 0; a < 16; a++) {
            int     v_, v1, v2, v3;
            v_ = (i - int(bool(i))) * lvlMult;
            v1 = ayLevelTable[a] + ayLevelTable[prvB] + ayLevelTable[prvC];
            v2 = ayLevelTable[a] + ayLevelTable[b] + ayLevelTable[prvC];
            v3 = ayLevelTable[a] + ayLevelTable[b] + ayLevelTable[c];
            double  err = double(v3 - v);
            double  err1 = double(v1) - (double(v + v_ + v_) * (1.0 / 3.0));
            double  err2 = double(v2) - (double(v + v + v_) * (1.0 / 3.0));
            err = err * (1.0 / double(191 * lvlMult));
            err1 = err1 * (1.0 / double(191 * lvlMult));
            err2 = err2 * (1.0 / double(191 * lvlMult));
            err = (err * err * double(m)) + (err1 * err1) + (err2 * err2);
            if (err < minErr) {
              minErr = err;
              bestA = a;
              bestB = b;
              bestC = c;
            }
          }
        }
      }
      dacTable[i] = (unsigned char) bestA;
      dacTable[i + 257] = (unsigned char) bestB;
      dacTable[i + 512] = (unsigned char) bestC;
      prvB = bestB;
      prvC = bestC;
      if (f && fileName) {
        std::fprintf(f,
                     "%3d: %2d %2d %2d (%5d %5d %5d), %6.4f (%6.4f), "
                     "err = %6.4f\n",
                     i, bestA, bestB, bestC, ayLevelTable[bestA],
                     ayLevelTable[bestB], ayLevelTable[bestC],
                     double(i) / 191.0,
                     double(ayLevelTable[bestA] + ayLevelTable[bestB]
                            + ayLevelTable[bestC]) / double(191 * lvlMult),
                     std::sqrt(minErr));
      }
    }
    double  d = calculateDistortion(&(dacTable[0]), fileName);
    if (!fileName) {
      if (d < bestDistortion) {
        bestDistortion = d;
        bestErrMult = m;
      }
      if (m >= 78)
        fileName = "dac.raw";
      continue;
    }
    std::printf("Distortion = %.3f%%\n", d);
    break;
  }
  if (f)
    std::fclose(f);
#ifndef USE_DACTABLE_YM
  f = std::fopen("dactable.s", "w");
#else
  f = std::fopen("dactableYM.s", "w");
#endif
  if (f) {
    std::fprintf(f, "\n        align 256\n\ndacTable:\n");
    for (int i = 0; i < 768; i++) {
      if (!(i & 15))
        std::fprintf(f, "        defb  ");
      std::fprintf(f, "%2d", int(dacTable[i]));
      if ((i & 15) != 15)
        std::fprintf(f, ", ");
      else
        std::fprintf(f, "\n");
    }
    std::fprintf(f, "\n");
    std::fclose(f);
  }
  return 0;
}

