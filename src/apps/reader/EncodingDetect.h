#pragma once
#include <stddef.h>
#include <stdint.h>

namespace paperos {

enum class Encoding : uint8_t { Utf8, Cp1251, Koi8R };

Encoding detectEncoding(const uint8_t* data, size_t len);

} // namespace paperos
