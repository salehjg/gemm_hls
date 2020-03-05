/// @author    Johannes de Fine Licht (definelicht@inf.ethz.ch)
/// @date      June 2017 
/// @copyright This software is copyrighted under the BSD 3-Clause License. 

#include "Utility.h"
#include "MatrixMultiplication.h"
#include "Conv2Helper.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <type_traits>
#include <vector>

int main(int argc, char **argv) {

#ifdef MM_DYNAMIC_SIZES
  if (argc < 6 || argc > 6) {
    std::cerr << "Usage: ./TestSimulation Conv_B Conv_N Conv_K Conv_Din Conv_Dout" << std::endl;
    return 1;
  }
  const unsigned conv_b = std::stoul(argv[1]);
  const unsigned conv_n = std::stoul(argv[2]);
  const unsigned conv_k = std::stoul(argv[3]);
  const unsigned conv_din = std::stoul(argv[4]);
  const unsigned conv_dout = std::stoul(argv[5]);



  const unsigned size_n = conv_b*conv_n*conv_k;
  const unsigned size_k = conv_din;
  const unsigned size_m = conv_dout;
  if (size_k % kMemoryWidthK != 0) {
    std::cerr << "K must be divisable by memory width." << std::endl;
    return 1;
  }
  if (size_m % kMemoryWidthM != 0) {
    std::cerr << "M must be divisable by memory width." << std::endl;
    return 1;
  }
  if (size_n % kOuterTileSizeN != 0) {
    std::cerr << "N must be divisable by the outer tile size in N."
              << std::endl;
    return 1;
  }
  if (size_m % kOuterTileSizeM != 0) {
    std::cerr << "M must be divisable by the outer tile size in M" << std::endl;
    return 1;
  }
#else
  constexpr auto size_n = kSizeN;
  constexpr auto size_k = kSizeK;
  constexpr auto size_m = kSizeM;
#endif

  std::vector<Data_t> a(size_n * size_k);
  std::vector<Data_t> b(size_k * size_m);
  std::vector<Data_t> cReference(size_n * size_m, 0);
    std::vector<Data_t> cReferenceConv2(size_n * size_m, 0);

  std::default_random_engine rng(kSeed);
  typename std::conditional<
      std::is_integral<Data_t>::value, std::uniform_int_distribution<unsigned long>,
      std::uniform_real_distribution<double>>::type dist(1, 10);

  std::for_each(a.begin(), a.end(),
                [&dist, &rng](Data_t &in) { in = Data_t(dist(rng)); });
  std::for_each(b.begin(), b.end(),
                [&dist, &rng](Data_t &in) { in = Data_t(dist(rng)); });

  const auto aKernel = Pack<kMemoryWidthA>(a);
  const auto bKernel = Pack<kMemoryWidthM>(b);
  auto cKernel = Pack<kMemoryWidthM>(cReference);

  ReferenceImplementation(a.data(), b.data(), cReference.data(), size_n, size_k,
                          size_m);

  Conv2Kernel1x1CPU<Data_t >(a.data(), b.data(), cReferenceConv2.data(), conv_b, conv_n, conv_k, conv_din, conv_dout);

  std::cout << "Running simulation...\n" << std::flush;
#ifdef MM_DYNAMIC_SIZES
  MatrixMultiplicationKernel(aKernel.data(), bKernel.data(), cKernel.data(),
                             size_n, size_k, size_m);
#else
  MatrixMultiplicationKernel(aKernel.data(), bKernel.data(), cKernel.data());
#endif
  std::cout << "Verifying results...\n" << std::flush;

  const auto cTest = Unpack<kMemoryWidthM>(cKernel);

  for (unsigned i = 0; i < size_n; ++i) {
    for (unsigned j = 0; j < size_m; ++j) {
      const auto testVal = make_signed<Data_t>(cTest[i * size_m + j]);
      const auto refVal = make_signed<Data_t>(cReference[i * size_m + j]);
      const auto refValConv2 = make_signed<Data_t>(cReferenceConv2[i * size_m + j]);
      const Data_t diff = std::abs(testVal - refVal);
      const Data_t diff2 = std::abs(testVal - refValConv2);
      if (diff / refVal > static_cast<Data_t>(1e-3)) {
        std::cerr << "Mismatch detected(Kernel vs. CPU MM) at (" << i << ", " << j
                  << "): " << testVal << " vs. " << refVal << "\n";
        return 1;
      }
      if (diff2 / refValConv2 > static_cast<Data_t>(1e-3)) {
        std::cerr << "Mismatch detected(Kernel vs. CPU Conv2) at (" << i << ", " << j
                  << "): " << testVal << " vs. " << refValConv2 << "\n";
        return 2;
      }
    }
  }
  std::cout << "Matrix-matrix multiplication successfully verified.\n";
  std::cout << "Conv2D 1x1 successfully verified.\n";

  return 0;
}
