#include "config_ini.hpp"

#include <vector>
#include <format>
#include <filesystem>

namespace {
	std::wstring get_ini_path()
	{
		std::vector<WCHAR> fullbuff(32767, L'\0');

		// モジュールインスタンスからDLLパスを取得
		auto loaded = ::GetModuleFileNameW(::GetModuleHandleW(nullptr), fullbuff.data(), fullbuff.size());
		std::filesystem::path p = fullbuff.data();
		p.replace_extension(L".ini");
		return p.c_str();
	}

	bool is_numeric(const std::wstring& _s)
	{
		for (const auto c : _s)
		{
			switch (c)
			{
			case L'0':
			case L'1':
			case L'2':
			case L'3':
			case L'4':
			case L'5':
			case L'6':
			case L'7':
			case L'8':
			case L'9':
				continue;
			default:
				return false;
			}
		}
		return true;
	}

	uint32_t get_digit(const std::wstring& _s)
	{
		uint32_t r = 0;
		for (const auto c : _s)
		{
			r *= 10;
			switch (c)
			{
			case L'1': r += 1; break;
			case L'2': r += 2; break;
			case L'3': r += 3; break;
			case L'4': r += 4; break;
			case L'5': r += 5; break;
			case L'6': r += 6; break;
			case L'7': r += 7; break;
			case L'8': r += 8; break;
			case L'9': r += 9; break;
			}
		}
		return r;
	}
}


namespace app {

	config_ini::config_ini()
		: path_(get_ini_path())
		, section_(L"MAIN")
	{
	}

	config_ini::config_ini(uint8_t _id)
		: path_(get_ini_path())
		, section_(L"CAPTURE" + std::format(L"{:02}", (uint16_t)_id))
	{
	}

	config_ini::~config_ini()
	{
	}

	bool config_ini::set_value(const std::wstring& _key, const std::wstring& _value)
	{
		auto r = ::WritePrivateProfileStringW(section_.c_str(), _key.c_str(), _value.c_str(), path_.c_str());
		return r == TRUE;
	}

	std::wstring config_ini::get_value(const std::wstring& _key)
	{
		std::vector<WCHAR> buffer(32767, L'\0');
		auto readed = ::GetPrivateProfileStringW(section_.c_str(), _key.c_str(), L"", buffer.data(), buffer.size(), path_.c_str());
		return buffer.data();
	}

	UINT config_ini::get_intvalue(const std::wstring& _key)
	{
		return ::GetPrivateProfileIntW(section_.c_str(), _key.c_str(), -1, path_.c_str());
	}

	bool config_ini::set_render_device(const std::wstring& _device)
	{
		return set_value(L"RENDER", _device);
	}

	std::wstring config_ini::get_render_device()
	{
		return get_value(L"RENDER");
	}

	std::wstring config_ini::get_capture_exe()
	{
		return get_value(L"CAPTUREEXE");
	}

	std::wstring config_ini::get_capture_title()
	{
		return get_value(L"CAPTURETITLE");
	}

	std::wstring config_ini::get_capture_classname()
	{
		return get_value(L"CAPTURECLASSNAME");
	}

	bool config_ini::set_capture_exe(const std::wstring& _exename)
	{
		return set_value(L"CAPTUREEXE", _exename);
	}

	bool config_ini::set_capture_title(const std::wstring& _title)
	{
		return set_value(L"CAPTURETITLE", _title);
	}

	bool config_ini::set_capture_classname(const std::wstring& _classname)
	{
		return set_value(L"CAPTURECLASSNAME", _classname);
	}

	bool config_ini::set_volume(UINT32 _v)
	{
		if (_v > 100) _v = 100;
		return set_value(L"VOLUME", std::to_wstring(_v));
	}

	UINT32 config_ini::get_volume()
	{
		auto s = get_value(L"VOLUME");

		// 例外は100
		if (s.length() >= 3) return 100;
		if (s.length() == 0) return 100;
		if (!is_numeric(s)) return 100;

		// 0から開始は0
		if (s.at(0) == L'0') return 0;

		return get_digit(s);
	}

	uint8_t config_ini::get_tabs()
	{
		auto s = get_value(L"TABS");

		// 例外はデフォルトの4
		if (s.length() >= 3) return 4;
		if (s.length() == 0) return 4;
		if (!is_numeric(s)) return 4;
		if (s.at(0) == L'0') return 4;

		auto n = get_digit(s);
		if (n > 16) n = 16;

		return n;
	}
}
