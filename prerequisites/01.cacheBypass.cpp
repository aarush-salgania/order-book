#include <iostream>
#include <chrono>
#include <cstdlib>
#include <immintrin.h>

using namespace std;
using namespace chrono;

const size_t N = 256 * 1024 * 1024;   // 256 MB

int main() {

  // Allocate 256 MB, aligned to 64 bytes
  float* data = (float*)_aligned_malloc(N * sizeof(float), 64);

  if (!data) {
    cout << "Allocation failed\n";
    return 1;
  }

  // -------------------------------
  // NORMAL STORE
  // -------------------------------

  auto start = high_resolution_clock::now();

  for (size_t i = 0; i < N; i++) {
    data[i] = 1.0f;
  }

  auto end = high_resolution_clock::now();

  auto normal_time = duration_cast<milliseconds>(end - start).count();

  cout << "Normal store:  "<< normal_time << " ms\n";


  // -------------------------------
  // STREAMING STORE
  // -------------------------------

  start = high_resolution_clock::now();

  __m128 value = _mm_set1_ps(2.0f);

  for (size_t i = 0; i < N; i += 4) {
    _mm_stream_ps(&data[i], value);
  }

  // Make sure streaming stores have completed
  _mm_sfence();

  end = high_resolution_clock::now();

  auto stream_time = duration_cast<milliseconds>(end - start).count();

  cout << "Streaming store: " << stream_time << " ms\n";

  
  _aligned_free(data);
}