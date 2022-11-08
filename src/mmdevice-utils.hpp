#pragma once

#include "common.hpp"

#include <string>
#include <vector>

#include <initguid.h>
#include <mmdeviceapi.h>
#include <mmeapi.h>

namespace app
{
	std::vector<IMMDevice*> get_mmdevices(EDataFlow, const WAVEFORMATEX &);
	std::vector<std::wstring> get_mmdevices_name(const std::vector<IMMDevice*>&);
}
