#pragma once

#include <string>
#include <vector>

namespace util {
std::string hmac_sha256_hex(const std::string &key, const std::string &data);
long long current_timestamp_ms();
std::vector<std::string> split(const std::string &s, char delim);
}
