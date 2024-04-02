#pragma once

#include "common.hpp"

#include "config_ini.hpp"
#include "enum_thread.hpp"
#include "worker_thread.hpp"

#include <shellapi.h>

#include <vector>
#include <memory>

namespace app
{
	class capture_tab
	{
	private:
		static HWND static_status_;
		static HWND static_pid_;
		static HWND static_skip_;
		static HWND static_duplicate_;
		static HWND static_render_;
		static HWND static_exename_;
		static HWND static_windowtitle_;
		static HWND static_windowclass_;
		static HWND static_volume_;

		const HINSTANCE instance_;
		HWND window_;
		HWND combo_render_;
		HWND combo_exename_;
		HWND combo_windowtitle_;
		HWND combo_windowclass_;
		HWND trackbar_volume_;
		std::vector<HWND> items_;
		uint8_t id_;
		config_ini ini_;
		worker_thread worker_thread_;
		std::wstring render_name_;
		std::wstring exename_;
		std::wstring windowtitle_;
		std::wstring windowclass_;
		std::vector<std::wstring> render_names_;
		std::vector<window_info_t> windows_;
		DWORD capture_pid_;
		bool capturing_;
		UINT32 volume_;
		bool active_;

		bool is_target_window(HWND _window);
		bool is_target_window(HWND _window, const std::wstring& _exename, const std::wstring& _windowtitle, const std::wstring& _windowclass) const;
		void create_control();

	public:
		capture_tab(const HINSTANCE _instance, uint8_t _id);
		~capture_tab();

		// コピー不可
		capture_tab(const capture_tab&) = delete;
		capture_tab& operator = (const capture_tab&) = delete;


		bool init(HWND _window);
		void stop();
		void reset();

		void hide();
		void show();

		void save();
		void cancel();

		void update_capture_pid();
		void update_status();
		void update_status(bool capturing);
		void update_stats();

		void update_current_config();

		void update_pulldown_render_device();
		void update_pulldown_exename(const std::vector<std::wstring>& _exenames);
		void update_pulldown_windowtitle(const std::vector<std::wstring>& _titles);
		void update_pulldown_windowclass(const std::vector<std::wstring>& _classes);

		bool search_and_reset();

		std::wstring get_pulldown_render_device();
		std::wstring get_pulldown_exename();
		std::wstring get_pulldown_windowtitle();
		std::wstring get_pulldown_windowclass();


		void reset_position();
		void reset_position_render_device();
		void reset_position_exename();
		void reset_position_windowtitle();
		void reset_position_windowclass();
		void reset_position_volume();

		void init_complete();
		void active_window_change(HWND _window);
		void resize(WORD _width, WORD _height);
		void enumwindow_finished(const std::vector<window_info_t>& _windows, const std::vector<std::wstring>& _exenames, const std::vector<std::wstring>& _titles, const std::vector<std::wstring>& _classes);

		uint32_t get_trackbar_volume();
		void set_trackbar_volume(uint32_t _volume);
		void save_volume();

		void query_stats();

	};

	class main_window
	{
	private:
		const HINSTANCE instance_;
		HWND window_;
		HWND tab_control_;
		HWND button_cancel_;
		HWND button_ok_;
		config_ini ini_;
		HWINEVENTHOOK hook_winevent_;
		NOTIFYICONDATAW nid_;
		UINT taskbar_created_;
		enum_thread enum_thread_;
		std::vector<std::unique_ptr<capture_tab>> tabs_;
		UINT active_tab_;

		static const wchar_t* window_class_;
		static const wchar_t* window_title_;
		static const LONG window_width_;
		static const LONG window_height_;

		void set_dpi_awareness();
		ATOM register_window_class();
		bool create_window();
		void disable_minmaxresize();
		UINT tasktray_add();
		void tasktray_reload();
		void tasktray_remove();
		void menu_create();
		void tab_control_create();
		void tab_control_add_item(UINT _id);
		void tab_control_select(UINT _id);
		void tab_control_change_text(UINT _id, const std::wstring& _text);
		UINT tab_control_get_position();
		void button_create();

		void window_show();


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
