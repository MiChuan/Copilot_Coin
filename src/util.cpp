#include "util.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace util {

std::string hmac_sha256_hex(const std::string &key, const std::string &data) {
	unsigned char* result;
	unsigned int len = EVP_MAX_MD_SIZE;
	result = (unsigned char*)malloc(len);
	HMAC(EVP_sha256(), key.c_str(), (int)key.length(), (unsigned char*)data.c_str(), data.length(), result, &len);
	std::ostringstream oss;
	oss<<std::hex<<std::setfill('0');
	for(unsigned int i=0;i<len;i++) {
		oss<<std::setw(2)<<(int)result[i];
	}
	free(result);
	return oss.str();
}

long long current_timestamp_ms(){
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

}
