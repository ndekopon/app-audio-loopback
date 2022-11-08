#pragma once

#include "common.hpp"

#include "config_ini.hpp"
#include "enum_thread.hpp"
#include "worker_thread.hpp"

#include <shellapi.h>

namespace app
{
	class main_window
	{
	private:
		const HINSTANCE instance_;
		HWND window_;
		HWND listview_;
		HWINEVENTHOOK hook_winevent;
		NOTIFYICONDATAW nid_;
		UINT taskbar_created_;
		config_ini config_ini_;
		enum_thread enum_thread_;
		worker_thread worker_thread_;
		std::wstring render_name_;
		std::vector<std::wstring> render_names_;
		std::vector<window_info_t> data_;
		DWORD capture_pid_;
		UINT32 volume_;
		int select_line_;

		static const wchar_t* window_class_;
		static const wchar_t* window_title_;

		void set_dpi_awareness();
		ATOM register_window_class();
		bool create_window();
		UINT tasktray_add();
		void tasktray_reload();
		void tasktray_remove();
		void menu_create();
		void select_app_menu_create();
		void submenu_render_create(HMENU);
		void submenu_volume_create(HMENU);

		LRESULT window_proc(UINT, WPARAM, LPARAM);
		static LRESULT CALLBACK window_proc_common(HWND, UINT, WPARAM, LPARAM);

	public:
		main_window(HINSTANCE);
		~main_window();

		// コピー不可
		main_window(const main_window&) = delete;
		main_window& operator = (const main_window&) = delete;
		// ムーブ不可
		main_window(main_window&&) = delete;
		main_window& operator = (main_window&&) = delete;

		bool init();
		int  loop();
	};
}
