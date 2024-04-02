#include "render.hpp"

#include "log.hpp"
#include "mmdevice-utils.hpp"

#include <audiopolicy.h>

namespace app {


	render::render(uint8_t _id)
		: id_(_id)
		, devices_()
		, names_()
		, client_(nullptr)
		, render_client_(nullptr)
		, volume_(nullptr)
		, format_({ WAVE_FORMAT_IEEE_FLOAT, COMMON_CHANNELS, COMMON_SAMPLES, COMMON_SAMPLES * COMMON_BYTES_PERFRAME, COMMON_BYTES_PERFRAME, COMMON_BYTES_PERFRAME * 4, 0 })
		, event_(NULL)
		, buffersize_(0)
		, last_size_(0)
		, total_frame_(0)
	{
	}

	render::~render()
	{
		if (volume_) volume_->Release();
		if (render_client_) render_client_->Release();
		if (client_) client_->Release();
		for (auto& device : devices_) device->Release();
		if (event_) ::CloseHandle(event_);
	}

	bool render::init()
	{
		wlog(id_, "render::init");
		devices_ = get_mmdevices(eRender, format_);
		names_ = get_mmdevices_name(devices_);
		event_ = ::CreateEventW(NULL, FALSE, FALSE, NULL);
		if (event_ == NULL)
		{
			wlog(id_, "  CreateEvent() failed.");
			return false;
		}

		if (devices_.size() == 0)
		{
			wlog(id_, "  render device not found.");
			return false;
		}
		wlog(id_, "  success.");
		return true;
	}

	const std::vector<std::wstring> render::get_names() const
	{
		return names_;
	}

	bool render::get_client_and_volume(const std::wstring& _name)
	{
		bool rc = false;
		int index = -1;

		for (size_t i = 0; i < names_.size(); ++i)
		{
			if (names_.at(i) == _name)
			{
				wlog(id_, L"  Name: " + _name);
				index = i;
				break;
			}
		}
		if (index >= 0)
		{
			HRESULT hr;
			IAudioSessionManager* manager = nullptr;

			hr = devices_.at(index)->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&client_);
			if (hr != S_OK)
			{
				wlog(id_, "  IMMDevice::Activate(IAudioClient) failed.");
			}

			hr = devices_.at(index)->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL, NULL, (void**)&manager);
			if (hr == S_OK)
			{
				hr = manager->GetSimpleAudioVolume(NULL, FALSE, &volume_);
				if (hr != S_OK)
				{
					wlog(id_, "  IAudioSessionManager::GetSimpleAudioVolume() failed.");
				}
				manager->Release();
			}
			else
			{
				wlog(id_, "  IMMDevice::Activate(IAudioSessionManager) failed.");
			}

			if (client_ && volume_) rc = true;
		}
		else
		{
			wlog(id_, "  capture device is not matched or None.");
		}

		// 不要になったdevicesの開放
		for (auto device : devices_)
		{
			device->Release();
		}
		devices_.clear();

		return rc;
	}

	bool render::start(const std::wstring& _name, UINT32 _v)
	{
		HRESULT hr;
		wlog(id_, "render::start");

		// クライアントとボリューム取得
		if (!get_client_and_volume(_name))
		{
			wlog(id_, "  render::get_client_and_volume() failed.");
			return false;
		}

		// ボリューム設定
		set_volume(_v);

		// Period取得
		REFERENCE_TIME period_default;
		REFERENCE_TIME period_minimum;
		hr = client_->GetDevicePeriod(&period_default, &period_minimum);
		if (hr != S_OK)
		{
			wlog(id_, "  IAudioClient::GetDevicePeriod() failed.");
			return false;
		}
		wlog(id_, "  DefaultDevicePeriod: " + std::to_string(period_default));
		wlog(id_, "  MinimumDevicePeriod: " + std::to_string(period_minimum));

		// 初期化
		hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, &format_, 0);
		if (hr != S_OK)
		{
			wlog(id_, "  IAudioClient::Initialize() failed.");
			return false;
		}

		// イベントハンドル関連付け
		hr = client_->SetEventHandle(event_);
		if (hr != S_OK)
		{
			wlog(id_, "  IAudioClient::SetEventHandle() failed.");
			return false;
		}

		// バッファサイズの取得
		hr = client_->GetBufferSize(&buffersize_);
		if (hr != S_OK)
		{
			wlog(id_, "  IAudioClient::GetBufferSize() failed.");
			return false;
		}
		wlog(id_, "  BufferSize: " + std::to_string(buffersize_));

		// IAudioCaptureClient取得
		hr = client_->GetService(__uuidof(IAudioRenderClient), (void**)&render_client_);
		if (hr != S_OK)
		{
			wlog(id_, "  IAudioClient::GetService() failed.");
			return false;
		}

		// 開始
		hr = client_->Start();
		if (hr != S_OK)
		{
			wlog(id_, "  IAudioClient::Start() failed.");
			false;
		}

		// バッファを埋める
		UINT32 padding;
		UINT32 available;
		BYTE* data;
		hr = client_->GetCurrentPadding(&padding);
		if (hr != S_OK)
		{
			return false;
		}
		available = buffersize_ - padding;
		if (available)
		{
			hr = render_client_->GetBuffer(available, &data);
			if (hr != S_OK)
			{
				return false;
			}
			hr = render_client_->ReleaseBuffer(available, AUDCLNT_BUFFERFLAGS_SILENT);
			if (hr != S_OK)
			{
				return false;
			}
		}

		wlog(id_, "  started.");
		return true;
	}

	void render::stop()
	{
		if (render_client_ && client_)
		{
			client_->Stop();
		}
	}


	bool render::proc_buffer(sample_buffer &_buffer)
	{
		HRESULT hr;
		UINT32 padding;
		UINT32 available;
		BYTE* data;

		hr = client_->GetCurrentPadding(&padding);
		if (hr != S_OK) return false;

		available = buffersize_ - padding;
		if (available == 0)
		{
			wlog(id_, "render buffer is filled.");
			return true;
		}

		hr = render_client_->GetBuffer(available, &data);
		if (hr != S_OK) return false;

		if (available != last_size_)
		{
			wlog(id_, "rendersize=" + std::to_string(available));
			last_size_ = available;
		}
		_buffer.get(data, available);

		hr = render_client_->ReleaseBuffer(available, 0);
		if (hr != S_OK) return false;

		total_frame_ += available;

		return true;
	}

	void render::set_volume(UINT32 _v)
	{
		if (_v > 100) _v = 100;
		if (volume_) volume_->SetMasterVolume(_v / 100.0f, FALSE);
	}

	UINT64 render::get_total_frame() const
	{
		return total_frame_;
	}
}
