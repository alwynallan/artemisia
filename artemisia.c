// artemisia.c

// Copyright 2022 alwynallan@gmail.com, MIT License

//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// detects defects in f_cker, old_rand, most compressed files

// $ sudo apt install libgsl-dev
// $ gcc -Wall -O3 -o artemisia artemisia.c -l gsl
// $ ln -s artemisia artemisia8
// $ ln -s artemisia artemisia16
// $ ln -s artemisia artemisia24
// $ ln -s artemisia artemisia32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h> // basename()
#include <math.h> // M_E
#include <assert.h>
#include <gsl/gsl_cdf.h> // $ sudo apt install libgsl-dev

void usage() {
  fprintf(stderr,
"\n"
"Usage: artemisia8, artemisia16, artemisia24, artemisia32, artemisia <n>\n"
"\n"
"Tests random data on stdin by constructing a directed graph with 2^n nodes\n"
"and 2^n edges. Each node as outdegree=1 and its target is determined by the\n"
"data. For random data, the nodes are observed to have indegree following a\n"
"distribution with\n"
"    P(indegree = d) = 1 / (e d!)\n"
"for 2^n large. Indegree is efficiently counted up to 3 for each node, expecting\n"
"    Pe0  = 1 / e        ~ 0.3679\n"
"    Pe1  = 1 / e        ~ 0.3679\n"
"    Pe2  = 1 / 2e       ~ 0.1839\n"
"    Pe3+ = 1 - (5 / 2e) ~ 0.0803\n"
"Four different bit lengths n are supported\n"
"    n  2^n            data required\n"
"    =  ===            =============\n"
"    8  256            256 B\n"
"    16 65,536         128 KB\n"
"    24 16,777,216     48 MB\n"
"    32 4,294,967,296  16 GB\n"
"Indegree counting is capped at 3 so that only two bits are used for each node,\n"
"allowing the 32-bit case to use just 1GB of memory. Once the required data is\n"
"read, the count for each expected value is totalled, and the totals compared\n"
"with the expected values using Pearsons's Chi Squared test. The p-value is\n"
"reported, and a test pass or fail is indicated at the 1%% confidence level.\n"
"\n"
  );
}

int main(int argc, char *argv[]) {
  assert(4 == sizeof(unsigned));
  assert(*(char *)(int[]){1}); // little-endian https://stackoverflow.com/a/8981006/5660198
  int bits=0;
  switch(argc) {
    case 1:
      if(!strncmp("artemisia8", basename(argv[0]),11)) bits = 8;
      else if(!strncmp("artemisia16", basename(argv[0]),12)) bits = 16;
      else if(!strncmp("artemisia24", basename(argv[0]),12)) bits = 24;
      else if(!strncmp("artemisia32", basename(argv[0]),12)) bits = 32;
      break;
    case 2:
      if(!strncmp("artemisia", basename(argv[0]),10)) switch(atoi(argv[1])) {
        case 8: bits = 8; break;
        case 16: bits = 16; break;
        case 24: bits = 24; break;
        case 32: bits = 32; break;
      }
      break;
  }
  if(!bits) {usage(); return 0;}
  
  unsigned char *indegree = calloc(1U<<(bits-2), 1);
  assert(NULL != indegree);
  
  unsigned char LUT[4][256], i=0U;
  do {
    LUT[0][i] = ((i&3U) < 3U) ? i+1 : i;
    LUT[1][i] = (LUT[0][i] << 2) | (LUT[0][i] >> 6);
    LUT[2][i] = (LUT[0][i] << 4) | (LUT[0][i] >> 4);
    LUT[3][i] = (LUT[0][i] << 6) | (LUT[0][i] >> 2);
  } while(++i);
  
  unsigned state=0U, stream;
  switch(bits) {
    case 8:
      do {
        assert((stream = getchar()) < 256U);
        indegree[stream>>2] = LUT[stream%4][indegree[stream>>2]];
      } while(state++ < 0xFF);
      break;
    case 16:
      do {
        assert(1 == fread(&stream, 2, 1, stdin)); // only works for little-endian
        indegree[stream>>2] = LUT[stream%4][indegree[stream>>2]];
      } while(state++ < 0xFFFF);
      break;
    case 24:
      do {
        assert(1 == fread(&stream, 3, 1, stdin)); // only works for little-endian
        indegree[stream>>2] = LUT[stream%4][indegree[stream>>2]];
      } while(state++ < 0xFFFFFF);
      break;
    case 32:
      do {
        assert(1 == fread(&stream, 4, 1, stdin));
        indegree[stream>>2] = LUT[stream%4][indegree[stream>>2]];
      } while(++state);
      break;
  }
  
  unsigned long long counts[4] = {0U, 0U, 0U, 0U};
  state = 0U;
  switch(bits) {
    case 8:
      do {
        counts[(indegree[state>>2] >> (state%4)*2) & 3U]++;
      } while(state++ < 0xFF);
      break;
    case 16:
      do {
        counts[(indegree[state>>2] >> (state%4)*2) & 3U]++;
      } while(state++ < 0xFFFF);
      break;
    case 24:
      do {
        counts[(indegree[state>>2] >> (state%4)*2) & 3U]++;
      } while(state++ < 0xFFFFFF);
      break;
    case 32:
      do {
        counts[(indegree[state>>2] >> (state%4)*2) & 3U]++;
      } while(++state);
      break;
  }
  
  assert(counts[0] + counts[1] + counts[2] + counts[3] == (unsigned long long)1U<<bits);
  double expecteds[4];
  expecteds[0] = (float)((unsigned long long)1U<<bits) / M_E;
  expecteds[1] = expecteds[0];
  expecteds[2] = expecteds[1] / 2.;
  expecteds[3] = (float)((unsigned long long)1U<<bits) - expecteds[0] - expecteds[1] - expecteds[2];
  double chi2 = 0.;
  printf("indegree, count, expected\n");
  for(i=0; i<4; i++) {
    double a = counts[i] - expecteds[i];
    chi2 += a * a / expecteds[i];
    printf("%d%s, %llu, %.1lf\n", i, i<3?" ":"+", counts[i], expecteds[i]);
  }
  double pval = gsl_cdf_chisq_P(chi2, 2.0);
  printf("p-value: %lf\n", pval);
  
  const double empirical_limits[4][2] = {{0.0135050, 0.994718},  // 8-bit for 1% false negative
                                         {0.0107605, 0.995029},  // 16-bit
                                         {0.0111855, 0.995167},  // 24-bit
                                         {0.005,     0.995   }}; // 32-bit, not empirical
  const int idx = bits / 8 - 1;
  printf("Test (1%% false rejection): %s\n",empirical_limits[idx][0]<pval&&pval<empirical_limits[idx][1]?"Pass":"Fail");
  return 0;
}
