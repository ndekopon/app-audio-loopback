﻿#pragma once

#include "common.hpp"

#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace app
{

	class worker_thread
	{
	private:
		uint8_t id_;
		HWND window_;
		HANDLE thread_;
		std::mutex mtx_;
		std::mutex cfg_mtx_;
		std::mutex stats_mtx_;
		std::wstring render_name_;
		DWORD capture_pid_;
		std::vector<std::wstring> render_names_;
		UINT32 volume_;
		HANDLE event_reset_;
		HANDLE event_close_;
		HANDLE event_stats_;
		HANDLE event_volume_;
		UINT64 stats_total_skip_;
		UINT64 stats_total_duplicate_;
		UINT32 startup_delay_;
		UINT32 duplicate_threshold_;
		UINT32 threshold_interval_;

		static DWORD WINAPI proc_common(LPVOID);
		DWORD proc();
		DWORD proc_render_and_capture();
		void set_render_names(const std::vector<std::wstring>&);
	public:
		worker_thread(uint8_t _id);
		~worker_thread();

		// コピー不可
		worker_thread(const worker_thread&) = delete;
		worker_thread& operator = (const worker_thread&) = delete;
		// ムーブ不可
		worker_thread(worker_thread&&) = delete;
		worker_thread& operator = (worker_thread&&) = delete;

		bool run(HWND, const std::wstring &, DWORD, UINT32, UINT32, UINT32, UINT32);
		void stop();

		void reset(const std::wstring &, DWORD, UINT32);

		void stats();
		std::array<UINT64, 2> get_stats();

		std::vector<std::wstring> get_render_names();
		std::wstring get_render_name();

		DWORD get_capture_pid();

		void set_volume(UINT32);
		UINT32 get_volume();
	};
}
