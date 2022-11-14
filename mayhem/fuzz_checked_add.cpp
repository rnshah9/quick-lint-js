#include "quick-lint-js/util/math-overflow.h"
#include <climits>
#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>
#include <stdio.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider provider(data, size);
  int a = provider.ConsumeIntegral<int>();
  int b = provider.ConsumeIntegral<int>();

  quick_lint_js::checked_add(a, b);

  return 0;
}