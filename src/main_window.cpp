#include "main_window.hpp"

#include <uxtheme.h>
#include <commctrl.h>

#include <filesystem>

#pragma comment(lib, "uxtheme.lib")


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

	void menu_checkeditem_create(HMENU _menu, UINT _id, const std::wstring& _str, bool _checked)
	{
		MENUITEMINFO mi = { 0 };
		std::vector<wchar_t> sz(_str.c_str(), _str.c_str() + _str.size() + 1);

		mi.cbSize = sizeof(MENUITEMINFO);
		mi.fMask = MIIM_ID | MIIM_STATE | MIIM_STRING | MIIM_CHECKMARKS;
		mi.wID = _id;
		mi.dwTypeData = sz.data();
		if (_checked)
			mi.fState = MFS_CHECKED;
		else
			mi.fState = MFS_UNCHECKED;
		::InsertMenuItemW(_menu, -1, TRUE, &mi);
	}

	void menu_separator_create(HMENU _menu)
	{
		MENUITEMINFO mi = { 0 };

		mi.cbSize = sizeof(MENUITEMINFO);
		mi.fMask = MIIM_FTYPE;
		mi.fType = MFT_SEPARATOR;
		::InsertMenuItemW(_menu, -1, TRUE, &mi);
	}

	void listview_insert(HWND _listview, UINT _id, const std::wstring& _title, const std::wstring& _classname, const std::wstring &_exename)
	{
		MENUITEMINFO mi = { 0 };
		std::vector<wchar_t> title(_title.c_str(), _title.c_str() + _title.size() + 1);
		std::vector<wchar_t> classname(_classname.c_str(), _classname.c_str() + _classname.size() + 1);
		std::vector<wchar_t> exename(_exename.c_str(), _exename.c_str() + _exename.size() + 1);


		LVITEM item = { 0 };
		item.iItem = _id;
		item.mask = LVIF_TEXT;
		item.pszText = exename.data();
		item.iSubItem = 0;
		ListView_InsertItem(_listview, &item);
		item.pszText = classname.data();
		item.iSubItem = 1;
		ListView_SetItem(_listview, &item);
		item.pszText = title.data();
		item.iSubItem = 2;
		ListView_SetItem(_listview, &item);
	}
}

namespace app
{
	constexpr UINT MID_EXIT = 0;
	constexpr UINT MID_RESET = 1;
	constexpr UINT MID_SHOW_WINDOW = 2;
	constexpr UINT MID_LISTVIEW_MAIN = 3;
	constexpr UINT MID_MATCH_EXE = 4;
	constexpr UINT MID_MATCH_CLASSNAME = 5;
	constexpr UINT MID_MATCH_TITLE = 6;
	constexpr UINT MID_REMOVE_MATCH_EXE = 7;
	constexpr UINT MID_REMOVE_MATCH_CLASSNAME = 8;
	constexpr UINT MID_REMOVE_MATCH_TITLE = 9;
	constexpr UINT MID_RENDER_NONE = 31;
	constexpr UINT MID_RENDER_0 = 32;
	constexpr UINT MID_SET_VOLUME_0 = 96;

	const wchar_t* main_window::window_class_ = L"app-audio-loopback-mainwindow";
	const wchar_t* main_window::window_title_ = L"app-audio-loopback";


	main_window::main_window(HINSTANCE _instance)
		: instance_(_instance)
		, window_(nullptr)
		, listview_(nullptr)
		, hook_winevent(NULL)
		, nid_({ 0 })
		, taskbar_created_(0)
		, config_ini_()
		, enum_thread_()
		, worker_thread_()
		, render_name_()
		, render_names_()
		, data_()
		, capture_pid_(0)
		, volume_(100)
		, select_line_(0)
	{
	}

	main_window::~main_window()
	{
	}

	bool main_window::init()
	{

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
			::TranslateMessage(&message);
			::DispatchMessageW(&message);
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
		WNDCLASSEXW wcex;

		wcex.cbSize = sizeof(WNDCLASSEXW);

		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = window_proc_common;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = instance_;
		wcex.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
		wcex.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = window_class_;
		wcex.hIconSm = ::LoadIconW(nullptr, IDI_APPLICATION);

		return ::RegisterClassExW(&wcex);
	}

	bool main_window::create_window()
	{
		window_ = ::CreateWindowExW(0, window_class_, window_title_, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, 0, 640, 480, nullptr, nullptr, instance_, this);

		if (window_ == nullptr)
		{
			return false;
		}

		return true;
	}

	UINT main_window::tasktray_add()
	{
		nid_.cbSize = sizeof(NOTIFYICONDATAW);
		nid_.uFlags = (NIF_ICON | NIF_MESSAGE | NIF_TIP);
		nid_.hWnd = window_;
		nid_.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
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

	void main_window::submenu_render_create(HMENU _menu)
	{
		int index = -1;
		for (size_t i = 0; i < render_names_.size() && i < 16; ++i)
		{
			if (render_name_ == render_names_.at(i)) index = i;
		}

		for (size_t i = 0; i < render_names_.size() && i < 16; ++i)
		{
			menu_checkeditem_create(_menu, MID_RENDER_0 + i, render_names_.at(i), index == i);
		}
		menu_checkeditem_create(_menu, MID_RENDER_NONE, L"None", index == -1);
	}

	void main_window::submenu_volume_create(HMENU _menu)
	{
		for (int i = 10; i >= 0; --i)
		{
			menu_checkeditem_create(_menu, MID_SET_VOLUME_0 + i, std::to_wstring(i * 10) + L"%", volume_ == i * 10);
		}
	}

	void main_window::menu_create()
	{
		HMENU menu;
		HMENU menu_render;
		HMENU menu_volume;
		POINT pt;
		MENUITEMINFO mi = { 0 };
		mi.cbSize = sizeof(MENUITEMINFO);

		// メニューハンドル作成
		menu = ::CreatePopupMenu();
		menu_render = ::CreatePopupMenu();
		menu_volume = ::CreatePopupMenu();

		// サブメニュー作成
		submenu_render_create(menu_render);
		submenu_volume_create(menu_volume);

		// アプリ選択画面表示
		WCHAR menu_selectapp_string[] = L"select application";
		mi.fMask = MIIM_ID | MIIM_STRING;
		mi.wID = MID_SHOW_WINDOW;
		mi.dwTypeData = menu_selectapp_string;
		::InsertMenuItemW(menu, -1, TRUE, &mi);

		// セパレーター
		menu_separator_create(menu);

		auto exename = config_ini_.get_capture_exe();
		if (exename != L"")
		{
			// exename
			menu_checkeditem_create(menu, MID_REMOVE_MATCH_EXE, (L"remove exe name match - " + exename), false);
		}
		auto classname = config_ini_.get_capture_classname();
		if (classname != L"")
		{
			// classname
			menu_checkeditem_create(menu, MID_REMOVE_MATCH_CLASSNAME, (L"remove window class name match - " + classname), false);
		}
		auto title = config_ini_.get_capture_title();
		if (title != L"")
		{
			// title
			menu_checkeditem_create(menu, MID_REMOVE_MATCH_TITLE, (L"remove window title match - " + title), false);
		}

		// セパレーター
		menu_separator_create(menu);

		// Render
		WCHAR menu_render_string[] = L"render";
		mi.fMask = MIIM_SUBMENU | MIIM_STRING;
		mi.hSubMenu = menu_render;
		mi.dwTypeData = menu_render_string;
		::InsertMenuItemW(menu, -1, TRUE, &mi);

		// Volume
		WCHAR menu_volume_string[] = L"volume";
		mi.fMask = MIIM_SUBMENU | MIIM_STRING;
		mi.hSubMenu = menu_volume;
		mi.dwTypeData = menu_volume_string;
		::InsertMenuItemW(menu, -1, TRUE, &mi);

		// セパレーター
		menu_separator_create(menu);

		// リセット
		WCHAR menu_reset_string[] = L"reset";
		mi.fMask = MIIM_ID | MIIM_STRING;
		mi.wID = MID_RESET;
		mi.dwTypeData = menu_reset_string;
		::InsertMenuItemW(menu, -1, TRUE, &mi);

		// セパレーター
		menu_separator_create(menu);

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
		::DestroyMenu(menu_volume);
		::DestroyMenu(menu_render);
		::DestroyMenu(menu);
	}

	void main_window::select_app_menu_create()
	{
		HMENU menu;
		POINT pt;
		MENUITEMINFO mi = { 0 };
		mi.cbSize = sizeof(MENUITEMINFO);

		if (select_line_ < data_.size())
		{
			auto d = data_.at(select_line_);
			auto title = std::get<0>(d);
			auto classname = std::get<1>(d);
			auto exename = std::get<2>(d);

			// メニューハンドル作成
			menu = ::CreatePopupMenu();

			// タイトル
			if (title != L"" && title != config_ini_.get_capture_title())
			{
				menu_checkeditem_create(menu, MID_MATCH_TITLE, (L"match title - " + title), false);
			}

			// クラス名
			if (classname != L"" && classname != config_ini_.get_capture_classname())
			{
				menu_checkeditem_create(menu, MID_MATCH_CLASSNAME, (L"match window class name - " + classname), false);
			}

			// exeファイル名
			if (exename != L"" && exename != config_ini_.get_capture_exe())
			{
				menu_checkeditem_create(menu, MID_MATCH_EXE, (L"match exe name - " + exename), false);
			}

			::GetCursorPos(&pt);
			::SetForegroundWindow(window_);
			::TrackPopupMenu(menu, TPM_TOPALIGN, pt.x, pt.y, 0, window_, NULL);
		}
	}

	bool main_window::is_target_window(HWND _window)
	{
		auto name = get_window_exe_name(_window);
		auto title = get_window_title(_window);
		auto classname = get_window_classname(_window);
		auto req_name = config_ini_.get_capture_exe();
		auto req_title = config_ini_.get_capture_title();
		auto req_classname = config_ini_.get_capture_classname();
		if (req_name != L"" && !is_match(name, req_name)) return false;
		if (req_title != L"" && !is_match(title, req_title)) return false;
		if (req_classname != L"" && !is_match(classname, req_classname)) return false;
		if (req_name == L"" && req_classname == L"" && req_title == L"") return false;
		return true;
	}

	LRESULT main_window::window_proc(UINT _message, WPARAM _wparam, LPARAM _lparam)
	{
		switch (_message)
		{
		case WM_CREATE:
			// リストビューの追加
			listview_ = ::CreateWindowExW(WS_EX_CONTROLPARENT | WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 0, 0, 10, 10, window_, (HMENU)MID_LISTVIEW_MAIN, instance_, NULL);

			if (listview_)
			{
				// 拡張スタイルの適用
				::SendMessageW(listview_, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);

				LVCOLUMNW col;
				col.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM | LVCF_WIDTH;
				col.fmt = LVCFMT_LEFT;

				// 1列目
				WCHAR colstr_exe[] = L"exe";
				col.pszText = colstr_exe;
				col.cx = 120;
				col.iSubItem = 0;
				ListView_InsertColumn(listview_, 0, &col);

				// 2列目
				WCHAR colstr_class[] = L"ClassName";
				col.pszText = colstr_class;
				col.cx = 120;
				col.iSubItem = 1;
				ListView_InsertColumn(listview_, 1, &col);

				// 3列目
				WCHAR colstr_title[] = L"Title";
				col.pszText = colstr_title;
				col.cx = 120;
				col.iSubItem = 2;
				ListView_InsertColumn(listview_, 2, &col);
			}

			// タスクトレイ追加
			taskbar_created_ = tasktray_add();

			// configから読み出し
			render_name_ = config_ini_.get_render_device();
			volume_ = config_ini_.get_volume();

			// フック開始
			hook_winevent = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, win_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
			if (hook_winevent == NULL) return -1;

			// スレッド開始
			if (!worker_thread_.run(window_, render_name_, capture_pid_, volume_)) return -1;
			if (!enum_thread_.run(window_)) return -1;

			// タイマー設定
			::SetTimer(window_, 1, 1000, nullptr);

			enum_thread_.start();

			return 0;

		case WM_PARENTNOTIFY:
			switch (LOWORD(_wparam))
			{
			case WM_CREATE:
				/* set font */
				::SendMessageW((HWND)_lparam, WM_SETFONT, (LPARAM)::GetStockObject(DEFAULT_GUI_FONT), true);
				break;
			}
			break;

		case WM_SIZE:
		{
			auto width = LOWORD(_lparam);
			auto height = HIWORD(_lparam);
			WORD posx = 8;
			WORD posy = 8;

			// リストビュー
			::MoveWindow(listview_, posx, posy, width - 16, height - 16, TRUE);
		}
		return 0;

		case CWM_INIT_COMPLETE:
			render_names_ = worker_thread_.get_render_names();
			return 0;

		case CWM_ENUMWINDOW_FINISHED:
		{
			data_ = enum_thread_.get_window_info();
			::SendMessageW(listview_, LVM_DELETEALLITEMS, 0, 0);

			bool found = false;
			UINT row = 0;
			for (const auto& [title, classname, exename, window] : data_)
			{
				listview_insert(listview_, row, title, classname, exename);
				if (!found)
				{
					auto id = get_process_id(window);
					if (id > 0 && is_target_window(window))
					{
						found = true;
						if (capture_pid_ != id)
						{
							capture_pid_ = id;
							worker_thread_.reset(render_name_, capture_pid_, volume_);
						}
					}
				}
				++row;
			}

			// 該当プロセスが見当たらなかった場合
			if (!found)
			{
				if (capture_pid_ > 0)
				{
					capture_pid_ = 0;
					worker_thread_.reset(render_name_, capture_pid_, volume_);
				}
			}

			// サイズ変更
			::SendMessageW(listview_, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
			::SendMessageW(listview_, LVM_SETCOLUMNWIDTH, 1, LVSCW_AUTOSIZE_USEHEADER);
			::SendMessageW(listview_, LVM_SETCOLUMNWIDTH, 2, LVSCW_AUTOSIZE_USEHEADER);
			return 0;
		}

		case WM_CLOSE:
			::ShowWindow(window_, SW_HIDE);
			return 0;

		case WM_DESTROY:
			// スレッド停止
			enum_thread_.stop();
			worker_thread_.stop();

			// フック停止
			if (hook_winevent) ::UnhookWinEvent(hook_winevent);

			// タスクトレイ削除
			tasktray_remove();

			::PostQuitMessage(0);

			return 0;

		case CWM_TASKTRAY:
			switch (_lparam)
			{
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
					worker_thread_.reset(render_name_, capture_pid_, volume_);
				}
				else if (id == MID_SHOW_WINDOW)
				{
					enum_thread_.start();
					::ShowWindow(window_, SW_SHOWNORMAL);
				}
				else if (id == MID_MATCH_EXE)
				{
					config_ini_.set_capture_exe(std::get<2>(data_.at(select_line_)));
					capture_pid_ = 0;
					worker_thread_.reset(render_name_, capture_pid_, volume_);
					enum_thread_.start();
				}
				else if (id == MID_MATCH_CLASSNAME)
				{
					config_ini_.set_capture_classname(std::get<1>(data_.at(select_line_)));
					capture_pid_ = 0;
					worker_thread_.reset(render_name_, capture_pid_, volume_);
					enum_thread_.start();
				}
				else if (id == MID_MATCH_TITLE)
				{
					config_ini_.set_capture_title(std::get<0>(data_.at(select_line_)));
					capture_pid_ = 0;
					worker_thread_.reset(render_name_, capture_pid_, volume_);
					enum_thread_.start();
				}
				else if (id == MID_REMOVE_MATCH_EXE)
				{
					config_ini_.set_capture_exe(L"");
					capture_pid_ = 0;
					worker_thread_.reset(render_name_, capture_pid_, volume_);
					enum_thread_.start();
				}
				else if (id == MID_REMOVE_MATCH_CLASSNAME)
				{
					config_ini_.set_capture_classname(L"");
					capture_pid_ = 0;
					worker_thread_.reset(render_name_, capture_pid_, volume_);
					enum_thread_.start();
				}
				else if (id == MID_REMOVE_MATCH_TITLE)
				{
					config_ini_.set_capture_title(L"");
					capture_pid_ = 0;
					worker_thread_.reset(render_name_, capture_pid_, volume_);
					enum_thread_.start();
				}
				else if (MID_RENDER_NONE <= id && id < MID_RENDER_0 + render_names_.size())
				{
					int index = id - MID_RENDER_0;
					std::wstring new_device = L"";
					if (index >= 0)
					{
						new_device = render_names_.at(index);
					}
					if (render_name_ != new_device)
					{
						render_name_ = new_device;
						config_ini_.set_render_device(render_name_);
						worker_thread_.reset(render_name_, capture_pid_, volume_);
					}
				}
				else if (MID_SET_VOLUME_0 <= id && id <= MID_SET_VOLUME_0 + 10)
				{
					UINT32 volume = (id - MID_SET_VOLUME_0) * 10;
					if (volume_ != volume)
					{
						volume_ = volume;
						config_ini_.set_volume(volume_);
						worker_thread_.set_volume(volume_);
					}
				}
			}
			break;
		
		case WM_NOTIFY:
		{
			auto hdr = reinterpret_cast<LPNMHDR>(_lparam);
			if (hdr->code == NM_RCLICK)
			{
				auto lv = reinterpret_cast<NMITEMACTIVATE*>(_lparam);
				if (lv->iItem >= 0)
				{
					select_line_ = lv->iItem;
					select_app_menu_create();
				}
			}
			break;
		}

		case WM_TIMER:

			if (_wparam == 1)
			{
				worker_thread_.stats();
				return 0;
			}
			break;

		case CWM_STATS_UPDATE:
		{
			auto stats = worker_thread_.get_stats();
			std::wstring tip = L"";
			tip += L"skip " + std::to_wstring(stats.at(0)) + L"\r\n";
			tip += L"duplicate " + std::to_wstring(stats.at(1));
			nid_.uFlags = NIF_TIP;
			::lstrcpyW(nid_.szTip, tip.c_str());
			::Shell_NotifyIconW(NIM_MODIFY, &nid_);
			break;
		}

		case CWM_ACTIVE_WINDOW_CHANGE:
		{
			if (capture_pid_ > 0)
			{
				if (!is_alive(capture_pid_))
				{
					capture_pid_ = 0;
					worker_thread_.reset(render_name_, capture_pid_, volume_);
				}
			}

			HWND active_window = (HWND)_wparam;
			if (active_window != NULL)
			{
				auto id = get_process_id(active_window);
				if (id > 0 && is_target_window(active_window))
				{
					if (capture_pid_ != id)
					{
						capture_pid_ = id;
						worker_thread_.reset(render_name_, capture_pid_, volume_);
					}
				}
			}
			break;
		}

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

		// 既にデータが格納されていたらインスタンスのプロシージャを呼び出す
		if (auto ptr = reinterpret_cast<main_window*>(::GetWindowLongPtrW(_window, GWLP_USERDATA)))
		{
			return ptr->window_proc(_message, _wparam, _lparam);
		}

		return ::DefWindowProcW(_window, _message, _wparam, _lparam);
	}
}
