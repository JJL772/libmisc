
#pragma once

#include "bits/libmisc.hpp"

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace LIBMISC_NAMESPACE {

	template<size_t N>
	char* strcpy(char(&buf)[N], const char* src) {
		auto p = std::strncpy(buf, src, N);
		buf[N-1] = 0;
		return p;
	}

	template<size_t N>
	char* strcat(char(&buf)[N], const char* src) {
		return std::strncat(buf, src, N);
	}

}
