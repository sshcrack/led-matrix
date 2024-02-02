#ifndef MAIN_RESOURCES_H
#define MAIN_RESOURCES_H
#include <string_view>

static std::string hex_to_string(const std::string& input) {
  static const char* const lut = "0123456789abcdef";
  size_t len = input.length();
  if (len & 1) throw;
  std::string output;
  output.reserve(len / 2);
  for (size_t i = 0; i < len; i += 2) {
    char a = input[i];
    const char* p = std::lower_bound(lut, lut + 16, a);
    if (*p != a) throw;
    char b = input[i + 1];
    const char* q = std::lower_bound(lut, lut + 16, b);
    if (*q != b) throw;
    output.push_back(((p - lut) << 4) | (q - lut));
  }
  return output;
}


static std::string jsonDefault = hex_to_string("7b0a20202267726f757073223a207b0a202020202244454641554c54223a207b0a202020202020226e616d65223a202252616e646f6d205061676573222c0a202020202020227061676573223a20205b312c202d315d0a202020207d0a20207d2c0a20202263757272223a202244454641554c54220a7d");

#endif //MAIN_RESOURCES_H
