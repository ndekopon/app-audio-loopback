#pragma once

#include "common.hpp"

#include <string>

namespace app
{
	void wlog(uint8_t _id, const std::string&);
	void wlog(uint8_t _id, const std::wstring&);
}
