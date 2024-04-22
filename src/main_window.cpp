#include "main_window.hpp"

#include "resource.hpp"

#include <uxtheme.h>
#include <commctrl.h>

#include <filesystem>
#include <format>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Comctl32.lib")



namespace
{
	HWND callback_window = NULL;
	
	void CALLBACK win_event_proc(HWINEVENTHOOK _hook, DWORD _event, HWND _window, LONG _idobject, LONG _idchild, DWORD ideventthread, DWORD _mseventtime)
	{
		if (callback_window)
		{
			::PostMessageW(callback_window, app::CWM_ACTIVE_WINDOW_CHANGE, (WPARAM)_window, 0);
		}
	}

	bool is_alive(DWORD _id)
	{
		bool rc = false;
		HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, _id);
		if (process)
		{
			DWORD exit_code = 0;
			if (::GetExitCodeProcess(process, &exit_code))
			{
				if (exit_code == STILL_ACTIVE)
				{
					rc = true;
				}
			}
			::CloseHandle(process);
		}
		return rc;
	}

	DWORD get_process_id(HWND _window)
	{
		DWORD id;
		::GetWindowThreadProcessId(_window, &id);
		return id;
	}

	std::wstring get_process_exe_name(DWORD _id)
	{
		DWORD size = 32767;
		std::vector<WCHAR> fullbuff(size, L'\0');
		HANDLE process = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, _id);
		if (process)
		{
			::QueryFullProcessImageNameW(process, 0, fullbuff.data(), &size);
			::CloseHandle(process);

			// フルパスを分解
			return std::filesystem::path(fullbuff.data()).filename();
		}
		else
		{
			return L"";
		}
	}

	std::wstring get_window_title(HWND _window)
	{

		DWORD size = 32767;
		std::vector<WCHAR> fullbuff(size, L'\0');

		auto r = ::GetWindowTextW(_window, fullbuff.data(), size);
		if (r > 0)
		{
			return fullbuff.data();
		}
		else
		{
			return L"";
		}
	}

	std::wstring get_window_classname(HWND _window)
	{

		DWORD size = 32767;
		std::vector<WCHAR> fullbuff(size, L'\0');

		auto r = ::RealGetWindowClassW(_window, fullbuff.data(), size);
		if (r > 0)
		{
			return fullbuff.data();
		}
		else
		{
			return L"";
		}
	}

	bool is_match(const std::wstring &_target, const std::wstring &_search)
	{
		if (_target == L"") return false;
		if (_search == L"") return false;
		return _target == _search;
	}

	void menu_separator_create(HMENU _menu)
	{
		MENUITEMINFO mi = { 0 };

		mi.cbSize = sizeof(MENUITEMINFO);
		mi.fMask = MIIM_FTYPE;
		mi.fType = MFT_SEPARATOR;
		::InsertMenuItemW(_menu, -1, TRUE, &mi);
	}

	void pulldown_select(HWND _pulldown, const std::wstring& _selected)
	{
		::SendMessageW(_pulldown, CB_SETCURSEL, _selected == L"" ? 0 : 1, 0);
	}

	void pulldown_update(HWND _pulldown, const std::vector<std::wstring>& _items, const std::wstring& _item)
	{
		// クリア
		::SendMessageW(_pulldown, CB_RESETCONTENT, 0, 0);

		// 空を追加
		::SendMessageW(_pulldown, CB_ADDSTRING, 0, (LPARAM)L"(None)");

		// 現在の選択された要素を表示
		if (_item != L"")
		{
			::SendMessageW(_pulldown, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(_item.c_str()));
		}

		for (const auto& item : _items)
		{
			if (item != _item)
			{
				::SendMessageW(_pulldown, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
			}
		}
	}

	std::wstring pulldown_get_selected_text(HWND _pulldown, const std::wstring& _current)
	{
		auto pos = ::SendMessageW(_pulldown, CB_GETCURSEL, 0, 0);
		if (pos == 0 || pos == CB_ERR) return L"";
		if (pos == 1 && _current != L"") return _current;
		auto len = ::SendMessageW(_pulldown, CB_GETLBTEXTLEN, pos, 0);
		std::vector<WCHAR> buf(len + 1, L'\0');
		auto result = ::SendMessageW(_pulldown, CB_GETLBTEXT, pos, reinterpret_cast<LPARAM>(buf.data()));
		if (result != CB_ERR)
		{
			std::wstring r(buf.data());
			return r;
		}
		return L"";
	}
}

namespace app
{
	constexpr UINT MID_EXIT = 0;
	constexpr UINT MID_RESET = 1;
	constexpr UINT MID_SHOW_WINDOW = 2;
	constexpr UINT MID_TAB = 3;
	constexpr UINT MID_BUTTON_CANCEL = 4;
	constexpr UINT MID_BUTTON_APPLY = 5;
	constexpr UINT MID_BUTTON_OK = 6;

	/* ---------------------------------------------------------------------
	   capture_tab
	   --------------------------------------------------------------------- */

	HWND capture_tab::static_status_ = nullptr;
	HWND capture_tab::static_pid_ = nullptr;
	HWND capture_tab::static_skip_ = nullptr;
	HWND capture_tab::static_duplicate_ = nullptr;
	HWND capture_tab::static_render_ = nullptr;
	HWND capture_tab::static_exename_ = nullptr;
	HWND capture_tab::static_windowtitle_ = nullptr;
	HWND capture_tab::static_windowclass_ = nullptr;
	HWND capture_tab::static_volume_ = nullptr;

	capture_tab::capture_tab(const HINSTANCE _instance, uint8_t _id)
		: instance_(_instance)
		, window_(nullptr)
		, combo_render_(nullptr)
		, combo_exename_(nullptr)
		, combo_windowtitle_(nullptr)
		, combo_windowclass_(nullptr)
		, trackbar_volume_(nullptr)
		, items_({})
		, id_(_id)
		, ini_(id_)
		, worker_thread_(id_)
		, render_name_()
		, exename_()
		, windowtitle_()
		, windowclass_()
		, render_names_({})
		, windows_({})
		, capture_pid_(0)
		, capturing_(false)
		, volume_(100)
		, active_(false)
	{
	}

	capture_tab::~capture_tab()
	{
	}

	bool capture_tab::init(HWND _window, uint32_t _startup_delay, uint32_t _duplicate_threshold, uint32_t _threshold_interval)
	{
		window_ = _window;


		// configから読み出し
		render_name_ = ini_.get_render_device();
		exename_ = ini_.get_capture_exe();
		windowtitle_ = ini_.get_capture_title();
		windowclass_ = ini_.get_capture_classname();
		volume_ = ini_.get_volume();

		create_control();

		if (!worker_thread_.run(window_, render_name_, capture_pid_, volume_, _startup_delay, _duplicate_threshold, _threshold_interval)) return false;

		return true;
	}

	void capture_tab::create_control()
	{
		RECT rc;
		::GetClientRect(window_, &rc);

		int status_top = 0;
		int config_top = 0;
		int settings_top = 0;
		int group_left = 7 + 32 + 8;
		int group_width = rc.right - group_left - 7;
		int group_padding = 8;
		int label_height = 16;
		DWORD label_style = WS_CHILD | WS_OVERLAPPED | WS_VISIBLE;
		int margin = 4;

		// グループボックス
		{
			int left = group_left;
			int top = group_padding;
			int width = group_width;
			int height = 0;
			DWORD group_style = WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | BS_GROUPBOX;

			// status
			status_top = top;
			height = 96;
			if (id_ == 0)
				::CreateWindowExW(0, WC_BUTTONW, L"status", 
					group_style, left, top, width, height, window_, 0, instance_, NULL);

			// config
			top += height;
			config_top = top;
			height = 116;
			if (id_ == 0)
				::CreateWindowExW(0, WC_BUTTONW, L"current setting",
					group_style, left, top, width, height, window_, 0, instance_, NULL);

			// settings
			top += height;
			settings_top = top;
			height = 250;
			if (id_ == 0)
				::CreateWindowExW(0, WC_BUTTONW, L"change setting",
					group_style, left, top, width, height, window_, 0, instance_, NULL);
		}

		{
			// statusグループ
			int left = group_left + group_padding;
			int top = status_top + label_height;
			int width = group_width - group_padding * 2;
			int height = label_height;
			int title_width = 110;
			int title_left = left;
			int value_width = width - title_width - 8;
			int value_left = left + title_width + 8;

			// status
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"status:",
				label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_status_ = ::CreateWindowExW(0, WC_STATICW, L"stopped",
					label_style, left, top, width, height, window_, 0, instance_, NULL);


			// pid
			top += height + margin;
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"target pid:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);

			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_pid_ = ::CreateWindowExW(0, WC_STATICW, L"0",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			// skip
			top += height + margin;
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"skip count:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);

			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_skip_ = ::CreateWindowExW(0, WC_STATICW, L"0",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			// duplicate
			top += height + margin;
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"dulicate count:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);

			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_duplicate_ = ::CreateWindowExW(0, WC_STATICW, L"0",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
		}

		{
			// configグループ
			int left = group_left + group_padding;
			int top = config_top + label_height;
			int width = group_width - group_padding * 2;
			int height = label_height;
			int title_width = 110;
			int title_left = left;
			int value_width = width - title_width - 8;
			int value_left = left + title_width + 8;

			// render
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"render device:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);

			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_render_ = ::CreateWindowExW(0, WC_STATICW, L"",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			// exename
			top += height + margin;
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"executable file name:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);

			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_exename_ = ::CreateWindowExW(0, WC_STATICW, L"",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			// windowtitle
			top += height + margin;
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"window title:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);

			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_windowtitle_ = ::CreateWindowExW(0, WC_STATICW, L"",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			// windowclass
			top += height + margin;
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"window class:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);

			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_windowclass_ = ::CreateWindowExW(0, WC_STATICW, L"",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			// volume
			top += height + margin;
			left = title_left;
			width = title_width;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"render volume:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);

			left = value_left;
			width = value_width;
			if (id_ == 0)
				static_volume_ = ::CreateWindowExW(0, WC_STATICW, L"",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			update_current_config();
		}

		{
			// settingsグループ
			int left = group_left + group_padding;
			int top = settings_top + label_height;
			int width = group_width - group_padding * 2;
			int height = 0;
			int dropdown_height = 23;
			int trackbar_height = 23;
			DWORD dropdown_style = CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | WS_TABSTOP;
			DWORD trackbar_style = TBS_NOTICKS | TBS_TOOLTIPS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE | WS_TABSTOP;

			// render
			height = label_height;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"render device:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			top += height;
			height = dropdown_height;
			combo_render_ = ::CreateWindowExW(0, WC_COMBOBOXW, L"",
				dropdown_style, left, top, width * 3 / 4, height, window_, 0, instance_, NULL);
			if (combo_render_) items_.push_back(combo_render_);

			// exename
			top += height + 8;
			height = label_height;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"executable file name:",
				label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			top += height;
			height = dropdown_height;
			combo_exename_ = ::CreateWindowExW(0, WC_COMBOBOXW, L"",
				dropdown_style, left, top, width / 2, height, window_, 0, instance_, NULL);
			if (combo_exename_) items_.push_back(combo_exename_);

			// window title
			top += height + 8;
			height = label_height;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"window title:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			top += height;
			height = dropdown_height;
			combo_windowtitle_ = ::CreateWindowExW(0, WC_COMBOBOXW, L"",
				dropdown_style, left, top, width, height, window_, 0, instance_, NULL);
			if (combo_windowtitle_) items_.push_back(combo_windowtitle_);

			// window class
			top += height + 8;
			height = label_height;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"window class:",
					label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			top += height;
			height = dropdown_height;
			combo_windowclass_ = ::CreateWindowExW(0, WC_COMBOBOXW, L"",
				dropdown_style, left, top, width, height, window_, 0, instance_, NULL);
			if (combo_windowclass_) items_.push_back(combo_windowclass_);

			// volume
			top += height + 8;
			height = label_height;
			if (id_ == 0)
				::CreateWindowExW(0, WC_STATICW, L"render volume:",
				label_style, left, top, width, height, window_, 0, instance_, NULL);
			
			top += height;
			height = trackbar_height;
			trackbar_volume_ = ::CreateWindowExW(0, TRACKBAR_CLASSW, L"",
				trackbar_style, left, top, width / 2, height, window_, 0, instance_, NULL);
			if (trackbar_volume_)
			{
				items_.push_back(trackbar_volume_);
				
				// MIN,MAXの設定(0～100)
				::SendMessageW(trackbar_volume_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));

				reset_position_volume();
			}
		}
	}

	void capture_tab::update_capture_pid()
	{
		if (active_)
			::SetWindowTextW(static_pid_, std::to_wstring(capture_pid_).c_str());
	}

	void capture_tab::update_status()
	{
		if (active_)
			::SetWindowTextW(static_status_, capturing_ ? L"capturing" : L"stopped");
	}

	void capture_tab::update_status(bool _capturing)
	{
		capturing_ = _capturing;
		update_status();
	}

	void capture_tab::update_stats()
	{
		if (active_)
		{
			auto stats = worker_thread_.get_stats();
			::SetWindowTextW(static_skip_, std::to_wstring(stats.at(0)).c_str());
			::SetWindowTextW(static_duplicate_, std::to_wstring(stats.at(1)).c_str());
		}
	}

	void capture_tab::update_current_config()
	{
		if (active_)
		{
			::SetWindowTextW(static_render_, render_name_ != L"" ? render_name_.c_str() : L"(None)");
			::SetWindowTextW(static_exename_, exename_ != L"" ? exename_.c_str() : L"(None)");
			::SetWindowTextW(static_windowtitle_, windowtitle_ != L"" ? windowtitle_.c_str() : L"(None)");
			::SetWindowTextW(static_windowclass_, windowclass_ != L"" ? windowclass_.c_str() : L"(None)");
			::SetWindowTextW(static_volume_, std::to_wstring(volume_).c_str());
		}
	}

	void capture_tab::reset_position()
	{
		reset_position_render_device();
		reset_position_exename();
		reset_position_windowtitle();
		reset_position_windowclass();
		reset_position_volume();
	}

	void capture_tab::reset_position_render_device()
	{
		pulldown_select(combo_render_, render_name_);
	}

	void capture_tab::reset_position_exename()
	{
		pulldown_select(combo_exename_, exename_);
	}

	void capture_tab::reset_position_windowtitle()
	{
		pulldown_select(combo_windowtitle_, windowtitle_);
	}

	void capture_tab::reset_position_windowclass()
	{
		pulldown_select(combo_windowclass_, windowclass_);
	}

	void capture_tab::reset_position_volume()
	{
		set_trackbar_volume(volume_);
	}

	void capture_tab::resize(WORD _width, WORD _height)
	{
		// nothing to do
	}

	void capture_tab::stop()
	{
		worker_thread_.stop();
	}

	void capture_tab::reset()
	{
		worker_thread_.reset(render_name_, capture_pid_, volume_);
	}

	void capture_tab::update_pulldown_render_device()
	{
		pulldown_update(combo_render_, render_names_, render_name_);
		reset_position_render_device();
	}

	void capture_tab::update_pulldown_exename(const std::vector<std::wstring>& _exenames)
	{
		pulldown_update(combo_exename_, _exenames, exename_);
		reset_position_exename();
	}

	void capture_tab::update_pulldown_windowtitle(const std::vector<std::wstring>& _titles)
	{
		pulldown_update(combo_windowtitle_, _titles, windowtitle_);
		reset_position_windowtitle();
	}

	void capture_tab::update_pulldown_windowclass(const std::vector<std::wstring>& _classes)
	{
		pulldown_update(combo_windowclass_, _classes, windowclass_);
		reset_position_windowclass();
	}

	void capture_tab::init_complete()
	{
		// renderデバイス取得＆ソート
		render_names_ = worker_thread_.get_render_names();
		std::sort(render_names_.begin(), render_names_.end());

		update_pulldown_render_device();
	}

	void capture_tab::enumwindow_finished(const std::vector<window_info_t>& _windows, const std::vector<std::wstring>& _exenames, const std::vector<std::wstring>& _titles, const std::vector<std::wstring>& _classes)
	{
		windows_ = _windows;

		update_pulldown_exename(_exenames);
		update_pulldown_windowtitle(_titles);
		update_pulldown_windowclass(_classes);

		search_and_reset();
	}

	bool capture_tab::search_and_reset()
	{
		for (const auto& [title, classname, exename, window] : windows_)
		{
			auto id = get_process_id(window);
			if (id > 0 && is_target_window(window, exename, title, classname))
			{
				if (capture_pid_ != id)
				{
					// capture対象のpidが変更された場合
					capture_pid_ = id;
					update_capture_pid();
					reset();
					return true;
				}
				return false;
			}
		}

		// 該当プロセスが見当たらなかった場合
		if (capture_pid_ > 0)
		{
			capture_pid_ = 0;
			update_capture_pid();
			reset();
			return true;
		}

		return false;
	}

	void capture_tab::active_window_change(HWND _window)
	{
		// 生存確認
		if (capture_pid_ > 0)
		{
			if (!is_alive(capture_pid_))
			{
				capture_pid_ = 0;
				update_capture_pid();
				reset();
			}
		}

		if (_window != NULL)
		{
			auto id = get_process_id(_window);
			if (id > 0 && is_target_window(_window))
			{
				if (capture_pid_ != id)
				{
					capture_pid_ = id;
					update_capture_pid();
					reset();
				}
			}
		}
	}

	bool capture_tab::is_target_window(HWND _window)
	{
		auto name = get_window_exe_name(_window);
		auto title = get_window_title(_window);
		auto classname = get_window_classname(_window);
		return is_target_window(_window, name, title, classname);
	}

	bool capture_tab::is_target_window(HWND _window, const std::wstring& _exename, const std::wstring& _windowtitle, const std::wstring& _windowclass) const
	{
		if (exename_ != L"" && !is_match(_exename, exename_)) return false;
		if (windowtitle_ != L"" && !is_match(_windowtitle, windowtitle_)) return false;
		if (windowclass_ != L"" && !is_match(_windowclass, windowclass_)) return false;
		if (exename_ == L"" && windowtitle_ == L"" && windowclass_ == L"") return false;

		return true;
	}

	void capture_tab::query_stats()
	{
		worker_thread_.stats();
	}

	std::wstring capture_tab::get_pulldown_render_device()
	{
		return pulldown_get_selected_text(combo_render_, render_name_);
	}

	std::wstring capture_tab::get_pulldown_exename()
	{
		return pulldown_get_selected_text(combo_exename_, exename_);
	}

	std::wstring capture_tab::get_pulldown_windowtitle()
	{
		return pulldown_get_selected_text(combo_windowtitle_, windowtitle_);
	}

	std::wstring capture_tab::get_pulldown_windowclass()
	{
		return pulldown_get_selected_text(combo_windowclass_, windowclass_);
	}

	void capture_tab::save()
	{
		auto render_name = get_pulldown_render_device();
		auto exename = get_pulldown_exename();
		auto windowtitle = get_pulldown_windowtitle();
		auto windowclass = get_pulldown_windowclass();
		auto volume = get_trackbar_volume();

		// 更新があったらiniに保存
		bool need_reset = false;
		bool need_research = false;
		bool need_volumeupdate = false;
		if (render_name != render_name_)
		{
			render_name_ = render_name;
			ini_.set_render_device(render_name);
			need_reset = true;
		}
		if (exename != exename_)
		{
			exename_ = exename;
			ini_.set_capture_exe(exename);
			need_research = true;
		}
		if (windowtitle != windowtitle_)
		{
			windowtitle_ = windowtitle;
			ini_.set_capture_title(windowtitle);
			need_research = true;
		}
		if (windowclass != windowclass_)
		{
			windowclass_ = windowclass;
			ini_.set_capture_classname(windowclass_);
			need_research = true;
		}

		// volume更新
		if (volume != volume_)
		{
			volume_ = volume;
			ini_.set_volume(volume);
			need_volumeupdate = true;
		}

		// 更新があったら設定表示を更新
		if (need_reset || need_research || need_volumeupdate)
		{
			update_current_config();
		}


		// 検索対象に変更があった場合
		auto resetted = false;
		if (need_research)
		{
			resetted = search_and_reset();
		}

		// リセットが必要な場合
		if (!resetted && need_reset)
		{
			reset();
			resetted = true;
		}

		// リセットせずに音量だけ変更する
		if (!resetted && need_volumeupdate)
		{
			worker_thread_.set_volume(volume);
		}

	}

	void capture_tab::cancel()
	{
		// nothing to do
	}

	void capture_tab::hide()
	{
		active_ = false;

		for (auto& item : items_)
		{
			::ShowWindow(item, SW_HIDE);
		}
	}

	void capture_tab::show()
	{
		active_ = true;

		update_capture_pid();
		update_status();
		update_stats();
		update_current_config();

		for (auto& item : items_)
		{
			::ShowWindow(item, SW_SHOW);
		}
	}

	uint32_t capture_tab::get_trackbar_volume()
	{
		uint32_t volume = 100;
		if (trackbar_volume_)
		{
			volume = ::SendMessageW(trackbar_volume_, TBM_GETPOS, 0, 0);
			if (volume > 100) volume = 100;
		}
		return volume;
	}

	void capture_tab::set_trackbar_volume(uint32_t _volume)
	{
		if (trackbar_volume_)
		{
			::SendMessageW(trackbar_volume_, TBM_SETPOS, TRUE, _volume);
		}
	}

	void capture_tab::save_volume()
	{
		ini_.set_volume(volume_);
	}


	/* ---------------------------------------------------------------------
	   main_window
	   --------------------------------------------------------------------- */

	const wchar_t* main_window::window_class_ = L"app-audio-loopback-mainwindow";
	const wchar_t* main_window::window_title_ = L"app-audio-loopback";
	const wchar_t* main_window::window_mutex_ = L"app-audio-loopback-mutex";

	const LONG main_window::window_width_ = 480;
	const LONG main_window::window_height_ = 640 - 130;

	main_window::main_window(HINSTANCE _instance)
		: instance_(_instance)
		, window_(nullptr)
		, tab_control_(nullptr)
		, button_cancel_(nullptr)
		, button_ok_(nullptr)
		, button_apply_(nullptr)
		, ini_()
		, hook_winevent_(NULL)
		, nid_({ 0 })
		, taskbar_created_(0)
		, enum_thread_()
		, tabs_()
		, active_tab_(0)
	{
		INITCOMMONCONTROLSEX ex = { 0 };
		ex.dwSize = sizeof(ex);
		ex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
		::InitCommonControlsEx(&ex);

		auto n = ini_.get_tabs();
		for (uint8_t i = 0; i < n; ++i)
		{
			tabs_.push_back(std::unique_ptr<capture_tab>(new capture_tab(_instance, i)));
		}
	}

	main_window::~main_window()
	{
	}

	bool main_window::init()
	{
		HANDLE mutex = ::CreateMutexW(NULL, TRUE, window_mutex_);
		if (::GetLastError() == ERROR_ALREADY_EXISTS)
		{
			return false;
		}

		set_dpi_awareness();

		// create window
		register_window_class();
		if (!create_window())
			return false;

		return true;
	}


	int main_window::loop()
	{
		MSG message;

		while (::GetMessageW(&message, nullptr, 0, 0))
		{
			if (!::IsDialogMessageW(window_, &message))
			{
				::TranslateMessage(&message);
				::DispatchMessageW(&message);
			}
		}
		return (int)message.wParam;
	}

	void main_window::set_dpi_awareness()
	{
		auto desired_context = DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED;
		if (::IsValidDpiAwarenessContext(desired_context))
		{
			auto hr = ::SetProcessDpiAwarenessContext(desired_context);
			if (hr)
				return;
		}
	}

	ATOM main_window::register_window_class()
	{
		WNDCLASSEXW wcex = {0};

		wcex.cbSize = sizeof(WNDCLASSEXW);

		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = window_proc_common;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = instance_;
		wcex.hIcon = ::LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPAUDIOLOOPBACKICON));
		wcex.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = window_class_;
		wcex.hIconSm = ::LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPAUDIOLOOPBACKICON));

		return ::RegisterClassExW(&wcex);
	}

	bool main_window::create_window()
	{
		DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
		window_ = ::CreateWindowExW(0, window_class_, window_title_, style,
			CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, instance_, this);

		if (window_ == nullptr)
		{
			return false;
		}

		return true;
	}

	void adjust_listbox_height(HWND _listbox)
	{
		int itemheight = ::SendMessageW(_listbox, LB_GETITEMHEIGHT, 0, 0);
		int count = ::SendMessageW(_listbox, LB_GETCOUNT, 0, 0);

		RECT rc;
		::SetRect(&rc, 0, 0, 32, itemheight * count);
		rc.bottom += GetSystemMetrics(SM_CXBORDER) * 2;
		::SetWindowPos(_listbox, 0, 0, 0, rc.right, rc.bottom, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void main_window::tab_control_create()
	{
		tab_control_ = ::CreateWindowExW(0, WC_LISTBOXW, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_TABSTOP | LBS_STANDARD,
			7, 14, 32, 211, window_, reinterpret_cast<HMENU>(MID_TAB), instance_, NULL);

		if (tab_control_)
		{
			for (UINT i = 0; i < tabs_.size(); ++i)
			{
				tab_control_add_item(i);
			}
			adjust_listbox_height(tab_control_);
		}
	}

	void main_window::tab_control_add_item(UINT _id)
	{
		auto text = L"00" + std::to_wstring(_id);
		text = text.substr(text.size() - 2);

		auto pos = ::SendMessageW(tab_control_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
		::SendMessageW(tab_control_, LB_SETITEMDATA, pos, (LPARAM)_id);
	}
	
	void main_window::tab_control_select(UINT _id)
	{
		if (_id < tabs_.size())
		{
			active_tab_ = _id;
			::SendMessageW(tab_control_, LB_SETCURSEL, active_tab_, 0);

			// 表示
			tabs_.at(active_tab_)->show();
			
			// 他を非表示
			for (uint8_t i = 0; i < tabs_.size(); ++i)
			{
				if (i != active_tab_) tabs_.at(i)->hide();
			}
		}
	}

	UINT main_window::tab_control_get_position()
	{
		auto hr = ::SendMessageW(tab_control_, LB_GETCURSEL, 0, 0);
		if (hr < 0) hr = 0;
		return static_cast<UINT>(hr);
	}

	void main_window::tab_control_change_text(UINT _id, const std::wstring& _text)
	{
		auto current = tab_control_get_position();
		::SendMessageW(tab_control_, LB_DELETESTRING, _id, 0);
		::SendMessageW(tab_control_, LB_INSERTSTRING, _id, reinterpret_cast<LPARAM>(_text.c_str()));
		if (current == _id)
		{
			tab_control_select(current);
		}
	}

	UINT main_window::tasktray_add()
	{
		nid_.cbSize = sizeof(NOTIFYICONDATAW);
		nid_.uFlags = (NIF_ICON | NIF_MESSAGE | NIF_TIP);
		nid_.hWnd = window_;
		nid_.hIcon = ::LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPAUDIOLOOPBACKICON));
		nid_.uID = 1;
		nid_.uCallbackMessage = CWM_TASKTRAY;
		nid_.uTimeout = 10000;
		nid_.dwState = NIS_HIDDEN;
		::lstrcpyW(nid_.szTip, window_title_);

		::Shell_NotifyIconW(NIM_ADD, &nid_);

		return ::RegisterWindowMessageW(L"TaskbarCreated");
	}

	void main_window::tasktray_reload()
	{
		::Shell_NotifyIconW(NIM_DELETE, &nid_);
		::Shell_NotifyIconW(NIM_ADD, &nid_);
	}

	void main_window::tasktray_remove()
	{
		::Shell_NotifyIconW(NIM_DELETE, &nid_);
	}

	void main_window::menu_create()
	{
		HMENU menu = ::CreatePopupMenu();
		POINT pt = { 0 };
		MENUITEMINFO mi = { 0 };
		mi.cbSize = sizeof(MENUITEMINFO);

		// 設定画面表示
		WCHAR menu_selectapp_string[] = L"setting";
		mi.fMask = MIIM_ID | MIIM_STRING;
		mi.wID = MID_SHOW_WINDOW;
		mi.dwTypeData = menu_selectapp_string;
		::InsertMenuItemW(menu, -1, TRUE, &mi);

		// リセット
		WCHAR menu_reset_string[] = L"reset";
		mi.fMask = MIIM_ID | MIIM_STRING;
		mi.wID = MID_RESET;
		mi.dwTypeData = menu_reset_string;
		::InsertMenuItemW(menu, -1, TRUE, &mi);

		// 終了
		WCHAR menu_exit_string[] = L"exit";
		mi.fMask = MIIM_ID | MIIM_STRING;
		mi.wID = MID_EXIT;
		mi.dwTypeData = menu_exit_string;
		::InsertMenuItemW(menu, -1, TRUE, &mi);

		::GetCursorPos(&pt);
		::SetForegroundWindow(window_);
		::TrackPopupMenu(menu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, window_, NULL);

		// ハンドルは削除
		::DestroyMenu(menu);
	}

	void main_window::disable_minmaxresize()
	{
		auto menu = ::GetSystemMenu(window_, FALSE);
		::RemoveMenu(menu, SC_MINIMIZE, FALSE);
		::RemoveMenu(menu, SC_MAXIMIZE, FALSE);
		::RemoveMenu(menu, SC_SIZE, FALSE);
		::RemoveMenu(menu, SC_RESTORE, FALSE);
	}

	void main_window::button_create()
	{
		button_apply_ = ::CreateWindowExW(0, WC_BUTTONW, L"&Apply", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_TABSTOP,
			window_width_ - 7 - 86, window_height_ - 9 - 24, 86, 24, window_, reinterpret_cast<HMENU>(MID_BUTTON_APPLY), instance_, NULL);

		button_cancel_ = ::CreateWindowExW(0, WC_BUTTONW, L"&Cancel", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | WS_TABSTOP,
			window_width_ - 7 - 8 * 1 - 86 * 2, window_height_ - 9 - 24, 86, 24, window_, reinterpret_cast<HMENU>(MID_BUTTON_CANCEL), instance_, NULL);

		button_ok_ = ::CreateWindowExW(0, WC_BUTTONW, L"&OK", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
			window_width_ - 7 - 8 * 2 - 86 * 3, window_height_ - 9 - 24, 86, 24, window_, reinterpret_cast<HMENU>(MID_BUTTON_OK), instance_, NULL);
	}

	void main_window::window_show()
	{
		enum_thread_.start();

		// 描画停止(ちらつき防止)
		::ShowWindow(window_, SW_SHOWNORMAL);

		// プルダウン等の選択状況を戻す
		for (auto& tab : tabs_)
		{
			tab->reset_position();
		}

		// 再描画
		::SetPropW(window_, L"SysSetRedraw", 0);
		::RedrawWindow(window_, nullptr, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);

		// フォーカスをOKボタンに戻す
		::SetFocus(button_ok_);
	}

	LRESULT main_window::window_proc(UINT _message, WPARAM _wparam, LPARAM _lparam)
	{
		switch (_message)
		{
		case WM_CREATE:
			// タスクトレイ追加
			taskbar_created_ = tasktray_add();

			disable_minmaxresize();

			tab_control_create();

			// タブの要素の初期化
			{
				auto startup_delay = ini_.get_startup_delay();
				auto duplicate_threshold = ini_.get_duplicate_threshold();
				auto threshold_interval = ini_.get_threshold_interval();

				for (auto& tab : tabs_)
				{
					if (!tab->init(window_, startup_delay, duplicate_threshold, threshold_interval))
					{
						return -1;
					}
				}
			}

			button_create();
			
			// タブの0を選択
			tab_control_select(0);

			// フック開始
			hook_winevent_ = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, win_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
			if (hook_winevent_ == NULL) return -1;

			// スレッド開始
			if (!enum_thread_.run(window_)) return -1;

			// タイマー設定
			::SetTimer(window_, 1, 1000, nullptr);

			// ウィンドウを列挙開始
			enum_thread_.start();

			return 0;

		case WM_PARENTNOTIFY:
			switch (LOWORD(_wparam))
			{
			case WM_CREATE:
				/* フォント設定 */
				::SendMessageW((HWND)_lparam, WM_SETFONT, (LPARAM)::GetStockObject(DEFAULT_GUI_FONT), true);
				break;
			}
			break;

		case CWM_INIT_COMPLETE:
		{
			const auto id = (uint8_t)_wparam;
			if (id < tabs_.size())
			{
				tabs_.at(id)->init_complete();
			}
			return 0;
		}

		case CWM_ENUMWINDOW_FINISHED:
		{
			// ウィンドウリストの更新
			auto windows = enum_thread_.get_window_info();

			// 重複＆空文字排除
			std::vector<std::wstring> exenames;
			std::vector<std::wstring> titles;
			std::vector<std::wstring> classes;
			for (const auto& [title, classname, exename, window] : windows)
			{
				if (std::find(exenames.begin(), exenames.end(), exename) == std::end(exenames) &&
					exename != L"")
				{
					exenames.push_back(exename);
				}
				if (std::find(titles.begin(), titles.end(), title) == std::end(titles) &&
					title != L"")
				{
					titles.push_back(title);
				}
				if (std::find(classes.begin(), classes.end(), classname) == std::end(classes) &&
					classname != L"")
				{
					classes.push_back(classname);
				}
			}

			// 要素のソート
			std::sort(exenames.begin(), exenames.end());
			std::sort(titles.begin(), titles.end());
			std::sort(classes.begin(), classes.end());

			for (auto& tab : tabs_)
			{
				tab->enumwindow_finished(windows, exenames, titles, classes);
			}

			return 0;
		}

		case WM_CLOSE:

			for (auto& tab : tabs_)
			{
				tab->cancel();
			}

			// タブの選択状況を0に戻す
			tab_control_select(0);

			::ShowWindow(window_, SW_HIDE);
			return 0;

		case WM_DESTROY:
			// スレッド停止
			enum_thread_.stop();

			// タブのスレッドも停止
			for (auto& tab : tabs_)
			{
				tab->stop();
			}

			// フック停止
			if (hook_winevent_) ::UnhookWinEvent(hook_winevent_);

			// タスクトレイ削除
			tasktray_remove();

			::PostQuitMessage(0);

			return 0;

		case CWM_TASKTRAY:
			switch (_lparam)
			{
			case WM_LBUTTONDBLCLK:
				window_show();
				break;
			case WM_RBUTTONUP:
				menu_create();
				break;
			}
			break;

		case WM_COMMAND:
			if (HIWORD(_wparam) == 0 && _lparam == 0)
			{
				WORD id = LOWORD(_wparam);
				if (id == MID_EXIT)
				{
					::DestroyWindow(window_);
				}
				else if (id == MID_RESET)
				{
					for (auto& tab : tabs_)
					{
						tab->reset();
					}
				}
				else if (id == MID_SHOW_WINDOW)
				{
					window_show();
				}
			}

			// APPLYボタン
			if (HIWORD(_wparam) == BN_CLICKED && (HWND)_lparam == button_apply_)
			{
				if (LOWORD(_wparam) == MID_BUTTON_APPLY)
				{
					for (auto& tab : tabs_)
					{
						tab->save();
					}
					window_show();
				}
			}

			// OKボタン
			if (HIWORD(_wparam) == BN_CLICKED && (HWND)_lparam == button_ok_)
			{
				if (LOWORD(_wparam) == MID_BUTTON_OK)
				{
					for (auto& tab : tabs_)
					{
						tab->save();
					}

					// タブの選択状況を0に戻す
					tab_control_select(0);

					::ShowWindow(window_, SW_HIDE);
				}
			}

			// Cancelボタン
			if (HIWORD(_wparam) == BN_CLICKED && (HWND)_lparam == button_cancel_)
			{
				if (LOWORD(_wparam) == MID_BUTTON_CANCEL)
				{
					for (auto& tab : tabs_)
					{
						tab->cancel();
					}

					// タブの選択状況を0に戻す
					tab_control_select(0);

					::ShowWindow(window_, SW_HIDE);
				}
			}

			// タブの選択
			if (HIWORD(_wparam) == LBN_SELCHANGE && (HWND)_lparam == tab_control_)
			{
				if (LOWORD(_wparam) == MID_TAB)
				{
					::SendMessageW(window_, WM_SETREDRAW, FALSE, 0); // 描画停止

					auto selected = tab_control_get_position();
					if (selected != LB_ERR) tab_control_select(selected);

					// 再描画
					::SendMessageW(window_, WM_SETREDRAW, TRUE, 0);
					::SetPropW(window_, L"SysSetRedraw", 0);
					::RedrawWindow(window_, nullptr, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
				}
			}
			break;

		case WM_TIMER:

			if (_wparam == 1)
			{
				for (auto& tab : tabs_)
				{
					tab->query_stats();
				}
				return 0;
			}
			break;

		case CWM_STATUS_UPDATE:
		{
			uint8_t id = (uint8_t)_wparam;
			if (id < tabs_.size())
			{
				if (_lparam == 1)
				{
					tab_control_change_text(id, std::format(L"{:02}*", id));
					tabs_.at(id)->update_status(true);
				}
				else
				{
					tab_control_change_text(id, std::format(L"{:02}", id));
					tabs_.at(id)->update_status(false);
				}
			}
			break;
		}

		case CWM_STATS_UPDATE:
		{
			uint8_t id = (uint8_t)_wparam;
			if (id < tabs_.size())
			{
				tabs_.at(id)->update_stats();
			}
			break;
		}

		case CWM_ACTIVE_WINDOW_CHANGE:
			for (auto& tab : tabs_)
			{
				tab->active_window_change((HWND)_wparam);
			}
			break;

		default:
			if (_message == taskbar_created_)
			{
				tasktray_reload();
			}
		}

		return ::DefWindowProcW(window_, _message, _wparam, _lparam);
	}

	LRESULT CALLBACK main_window::window_proc_common(HWND _window, UINT _message, WPARAM _wparam, LPARAM _lparam)
	{
		if (_message == WM_NCCREATE)
		{
			// createwindowで指定したポイントからインスタンスを取得
			auto cs = reinterpret_cast<CREATESTRUCTW*>(_lparam);
			auto instance = reinterpret_cast<main_window*>(cs->lpCreateParams);

			instance->window_ = _window;
			callback_window = _window;

			// USERDATAにポインタ格納
			::SetWindowLongPtrW(_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(instance));
		}
		else if (_message == WM_GETMINMAXINFO)
		{
			RECT r = { 0, 0, window_width_, window_height_ };
			if (::AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE) == TRUE)
			{
				auto width = (r.right - r.left);
				auto height = (r.bottom - r.top);
				MINMAXINFO* mminfo = (MINMAXINFO*)_lparam;
				mminfo->ptMaxSize.x = width;
				mminfo->ptMaxSize.y = height;
				mminfo->ptMaxPosition.x = 0;
				mminfo->ptMaxPosition.y = 0;
				mminfo->ptMinTrackSize.x = width;
				mminfo->ptMinTrackSize.y = height;
				mminfo->ptMaxTrackSize.x = width;
				mminfo->ptMaxTrackSize.y = height;
				return 0;
			}
		}

		// 既にデータが格納されていたらインスタンスのプロシージャを呼び出す
		if (auto ptr = reinterpret_cast<main_window*>(::GetWindowLongPtrW(_window, GWLP_USERDATA)))
		{
			return ptr->window_proc(_message, _wparam, _lparam);
		}

		return ::DefWindowProcW(_window, _message, _wparam, _lparam);
	}
}
