#include "log.hpp"

#include <ctime>
#include <vector>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace
{

	std::wstring get_log_filename()
	{
		auto t = std::time(nullptr);
		struct tm lt;
		auto error = localtime_s(&lt, &t);

		std::wostringstream oss;
		oss << L"log_";
		oss << std::put_time(&lt, L"%Y%m%d_%H%M%S");
		oss << L".txt";
		return oss.str();
	}

	std::wstring get_log_dir()
	{
		WCHAR drivebuff[_MAX_DRIVE];
		std::vector<WCHAR> fullbuff(32767, L'\0');

		// モジュールインスタンスからDLLパスを取得
		auto loaded = ::GetModuleFileNameW(::GetModuleHandleW(nullptr), fullbuff.data(), fullbuff.size());

		// フルパスを分解
		std::vector<WCHAR> dirbuff(loaded, L'\0');
		::_wsplitpath_s(fullbuff.data(), drivebuff, _MAX_DRIVE, dirbuff.data(), dirbuff.size(), nullptr, 0, nullptr, 0);

		// パスの合成
		std::wstring r;
		r = drivebuff;
		r += dirbuff.data();
		r += L"logs\\";
		return r;
	}

	std::string get_datestring()
	{
		auto t = std::time(nullptr);
		struct tm lt;
		auto error = localtime_s(&lt, &t);

		std::ostringstream oss;
		oss << std::put_time(&lt, "%Y/%m/%d %H:%M:%S");
		return oss.str();
	}

	std::string utf16_to_utf8(const std::wstring& _src)
	{
		// calc buffer size
		auto size = ::WideCharToMultiByte(CP_UTF8, 0, _src.c_str(), -1, nullptr, 0, nullptr, nullptr);

		// convert
		std::vector<char> buffer(size + 1, '\0');
		::WideCharToMultiByte(CP_UTF8, 0, _src.c_str(), -1, buffer.data(), size + 1, nullptr, nullptr);

		return buffer.data();
	}
}

namespace app
{

	class app_logger {
	private:
		bool enable_;
		std::wstring path_;
		HANDLE file_;
		std::mutex mtx_;

	public:
		app_logger()
			: enable_(true)
			, path_(L"")
			, file_(nullptr)
			, mtx_()
		{
			// logpathの確認
			auto logdir = get_log_dir();
			auto type = ::GetFileAttributesW(logdir.c_str());
			if (type == INVALID_FILE_ATTRIBUTES || (type | FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				enable_ = false;
			}
			path_ = logdir + get_log_filename();
		}

		~app_logger()
		{
			if (file_)
			{
				::CloseHandle(file_);
			}
		}

		void write_to_logfile(const std::string& _text)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			
			if (!enable_) return;
			if (path_ == L"") return;

			if (file_ == nullptr)
			{
				file_ = ::CreateFileW(path_.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			}

			if (file_ == INVALID_HANDLE_VALUE)
			{
				enable_ = false;
				return;
			}
			else
			{
				DWORD written = 0;
				::WriteFile(file_, _text.c_str(), (DWORD)_text.size(), &written, NULL);
			}
		}
	} logger;


	void wlog(uint8_t _id, const std::string& _text)
	{
		auto date = get_datestring();
		logger.write_to_logfile(date + " " + std::to_string(static_cast<uint16_t>(_id)) + " " + _text + "\r\n");
	}

	void wlog(uint8_t _id, const std::wstring& _text)
	{
		wlog(_id, utf16_to_utf8(_text));
	}
}
