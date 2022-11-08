#pragma once

#include "common.hpp"

#include <array>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace app
{

	/* Utility */
	std::wstring get_window_title(HWND);
	std::wstring get_window_class_name(HWND);
	std::wstring get_window_exe_name(HWND);

	using window_info_t = std::tuple<std::wstring, std::wstring, std::wstring, HWND>;

	class enum_thread
	{
	private:
		HWND window_;
		HANDLE thread_;
		std::mutex mtx_;
		HANDLE event_enum_;
		HANDLE event_close_;
		std::vector<HWND> windows_;
		std::vector<window_info_t> data_;
		static BOOL CALLBACK enum_proc_common(HWND, LPARAM);
		static DWORD WINAPI proc_common(LPVOID);
		BOOL enum_proc(HWND);
		DWORD proc();
	public:
		enum_thread();
		~enum_thread();

		// コピー不可
		enum_thread(const enum_thread&) = delete;
		enum_thread& operator = (const enum_thread&) = delete;
		// ムーブ不可
		enum_thread(enum_thread&&) = delete;
		enum_thread& operator = (enum_thread&&) = delete;

		bool run(HWND);
		void stop();

		void start();

		std::vector<window_info_t> get_window_info();
	};
}
