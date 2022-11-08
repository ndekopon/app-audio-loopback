#include "enum_thread.hpp"

#include <dwmapi.h>

#include <filesystem>

#pragma comment(lib, "Dwmapi.lib")

namespace
{
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
}

namespace app {

	std::wstring get_window_title(HWND _window)
	{
		auto len = ::GetWindowTextLengthW(_window);
		if (len > 0)
		{
			std::vector<WCHAR> buff(len + 1, L'\0');
			auto r = ::GetWindowTextW(_window, buff.data(), len + 1);
			if (r > 0)
			{
				return buff.data();
			}
		}
		return L"";
	}

	std::wstring get_window_class_name(HWND _window)
	{
		std::vector<WCHAR> buff(256, L'\0');
		auto len = ::GetClassNameW(_window, buff.data(), 256);
		if (len > 0)
		{
			return buff.data();
		}
		return L"";
	}

	std::wstring get_window_exe_name(HWND _window)
	{
		auto id = get_process_id(_window);
		if (id > 0)
		{
			return get_process_exe_name(id);
		}
		return L"";
	}

	enum_thread::enum_thread()
		: window_(NULL)
		, thread_(NULL)
		, mtx_()
		, event_enum_(NULL)
		, event_close_(NULL)
		, windows_()
		, data_()
	{
	}

	enum_thread::~enum_thread()
	{
		stop();
		if (event_enum_) ::CloseHandle(event_enum_);
		if (event_close_) ::CloseHandle(event_close_);
	}

	BOOL CALLBACK enum_thread::enum_proc_common(HWND _window, LPARAM _p)
	{
		auto p = reinterpret_cast<enum_thread*>(_p);
		return p->enum_proc(_window);
	}

	DWORD WINAPI enum_thread::proc_common(LPVOID _p)
	{
		auto p = reinterpret_cast<enum_thread*>(_p);
		return p->proc();
	}

	BOOL enum_thread::enum_proc(HWND _window)
	{
		if (_window != NULL) windows_.push_back(_window);
		return TRUE;
	}

	DWORD enum_thread::proc()
	{
		while (true)
		{
			HANDLE events[] = {
				event_close_,
				event_enum_
			};

			auto id = WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE, INFINITE);

			if (id == WAIT_OBJECT_0)
			{
				break;
			}
			else if (id == WAIT_OBJECT_0 + 1)
			{
				// 列挙開始
				windows_.clear();
				auto rc = ::EnumWindows(enum_proc_common, reinterpret_cast<LPARAM>(this));

				// 情報取得
				std::vector<window_info_t> data;
				for (const auto window : windows_)
				{
					// 自分自身はリストに追加しない
					if (window_ == window) continue;

					// 見えてるウィンドウのみ
					if (::IsWindowVisible(window) == FALSE) continue;

					// 隠されたウィンドウは表示しない
					INT cloaked = 0;
					auto hr = ::DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(INT));
					if (hr == S_OK && cloaked > 0) continue;

					// 情報を格納
					const auto title = get_window_title(window);
					const auto classname = get_window_class_name(window);
					const auto exename = get_window_exe_name(window);
					data.push_back({ title, classname, exename });
				}

				// 情報格納
				{
					std::lock_guard<std::mutex> lock(mtx_);
					data_ = data;
				}

				// メインウィンドウへ通知
				::PostMessageW(window_, CWM_ENUMWINDOW_FINISHED, NULL, NULL);
			}
		}

		return 0;
	}

	bool enum_thread::run(HWND _window)
	{
		window_ = _window;

		// イベント生成
		if (event_close_ == NULL) event_close_ = ::CreateEventW(NULL, FALSE, FALSE, NULL);
		if (event_close_ == NULL) return false;
		if (event_enum_ == NULL) event_enum_ = ::CreateEventW(NULL, FALSE, FALSE, NULL);
		if (event_enum_ == NULL) return false;
		
		// スレッド起動
		thread_ = ::CreateThread(NULL, 0, proc_common, this, 0, NULL);
		return thread_ != NULL;
	}

	void enum_thread::stop()
	{
		if (thread_ != NULL)
		{
			::SetEvent(event_close_);
			::WaitForSingleObject(thread_, INFINITE);
			thread_ = NULL;
		}
	}

	void enum_thread::start()
	{
		if (thread_ != NULL)
		{
			::SetEvent(event_enum_);
		}
	}

	std::vector<window_info_t> enum_thread::get_window_info()
	{
		std::lock_guard<std::mutex> lock(mtx_);
		return data_;
	}
}
