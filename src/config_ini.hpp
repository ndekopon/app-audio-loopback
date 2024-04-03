#pragma once

#include "common.hpp"

#include <string>

namespace app
{

	class config_ini
	{
	private:
		std::wstring path_;
		std::wstring section_;

		bool set_value(const std::wstring& _key, const std::wstring& _value);
		std::wstring get_value(const std::wstring& _key);
		UINT get_intvalue(const std::wstring& _key);
		uint32_t get_msec(const std::wstring& _key, uint32_t _default);

	public:
		config_ini();
		config_ini(uint8_t _id);
		~config_ini();

		bool set_render_device(const std::wstring& _device);
		std::wstring get_render_device();

		std::wstring get_capture_exe();
		std::wstring get_capture_title();
		std::wstring get_capture_classname();


		bool set_capture_exe(const std::wstring &);
		bool set_capture_title(const std::wstring &);
		bool set_capture_classname(const std::wstring &);

		bool set_volume(uint32_t);
		uint32_t get_volume();

		uint8_t get_tabs();

		uint32_t get_startup_delay();
		uint32_t get_duplicate_threshold();
		uint32_t get_threshold_interval();
	};
}
