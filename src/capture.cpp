#include "capture.hpp"

#include "log.hpp"
#include "mmdevice-utils.hpp"

#include <audioclientactivationparams.h>

#pragma comment(lib, "mmdevapi.lib")

namespace app {

	class completion_handler : public IActivateAudioInterfaceCompletionHandler
	{
	private:
		volatile LONG count_;

	public:
		HANDLE event_;
		IAudioClient *client_;

		completion_handler()
			: count_(1)
			, event_(NULL)
			, client_(nullptr)
		{
			event_ = ::CreateEventW(NULL, FALSE, FALSE, NULL);
		}

		~completion_handler()
		{
			::CloseHandle(event_);
		}

		ULONG STDMETHODCALLTYPE AddRef(void)
		{
			return ::_InterlockedIncrement(&count_);
		}

		ULONG STDMETHODCALLTYPE Release(void)
		{
			return ::_InterlockedDecrement(&count_);
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(const IID& _riid, void** _v)
		{
			if (_v == nullptr) return E_POINTER;
			if (_riid == IID_IUnknown ||
				_riid == IID_IAgileObject ||
				_riid == __uuidof(IActivateAudioInterfaceCompletionHandler))
			{
				*_v = static_cast<IActivateAudioInterfaceCompletionHandler*>(this);
				AddRef();
				return S_OK;
			}

			*_v = nullptr;
			if (_riid == IID_IMarshal)
			{
				return S_OK;
			}

			return E_NOINTERFACE;
		}

		STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* _operation)
		{
			HRESULT hr;
			HRESULT activate_hr = E_FAIL;
			IUnknown *unknown = nullptr;

			hr = _operation->GetActivateResult(&activate_hr, &unknown);
			if (hr == S_OK && activate_hr == S_OK)
			{
				hr = unknown->QueryInterface(IID_PPV_ARGS(&client_));
				unknown->Release();
			}
			::SetEvent(event_);
			return S_OK;
		}
	};


	capture::capture(uint8_t _id)
		: id_(_id)
		, client_(nullptr)
		, capture_client_(nullptr)
		, format_({ WAVE_FORMAT_IEEE_FLOAT, COMMON_CHANNELS, COMMON_SAMPLES, COMMON_SAMPLES * COMMON_BYTES_PERFRAME, COMMON_BYTES_PERFRAME, COMMON_BYTES_PERFRAME * 4, 0 })
		, buffersize_(0)
		, last_size_(0)
		, total_frame_(0)
		, event_(NULL)
	{
	}

	capture::~capture()
	{
		if (capture_client_) capture_client_->Release();
		if (client_) client_->Release();
		if (event_) ::CloseHandle(event_);
	}

	bool capture::init()
	{
		wlog(id_, "capture::init");
		event_ = ::CreateEventW(NULL, FALSE, FALSE, NULL);
		if (event_ == NULL)
		{
			wlog(id_, "  CreateEvent() failed.");
			return false;
		}
		wlog(id_, "  success.");
		return true;
	}

	IAudioClient* capture::get_client(DWORD _pid)
	{
		HRESULT hr;
		IAudioClient* client = nullptr;
		AUDIOCLIENT_ACTIVATION_PARAMS param;
		PROPVARIANT param_wrapper;
		completion_handler handler;
		IActivateAudioInterfaceAsyncOperation* operation;

		param.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
		param.ProcessLoopbackParams.TargetProcessId = _pid;
		param.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

		param_wrapper.vt = VT_BLOB;
		param_wrapper.blob.cbSize = sizeof(param);
		param_wrapper.blob.pBlobData = reinterpret_cast<BYTE*>(&param);

		hr = ::ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient), &param_wrapper, &handler, &operation);
		if (hr == S_OK)
		{
			wlog(id_, "  wait ActivateCompleted()...");
			auto rc = ::WaitForSingleObject(handler.event_, INFINITE);
			wlog(id_, "  receive ActivateCompleted().");
			client = handler.client_;
			operation->Release();
		}
		else
		{
			wlog(id_, "  ActivateAudioInterfaceAsync() failed.");
		}

		return client;
	}

	bool capture::start(DWORD _pid)
	{
		HRESULT hr;

		wlog(id_, "capture::start");

		// クライアント取得
		client_ = get_client(_pid);
		if (client_ == nullptr)
		{
			wlog(id_, "  capture::get_client() failed.");
			return false;
		}

		// 初期化
		hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 0, 0, &format_, 0);
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
		hr = client_->GetService(__uuidof(IAudioCaptureClient), (void**)&capture_client_);
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
		wlog(id_, "  started.");
		return true;
	}

	void capture::stop()
	{
		if (capture_client_ && client_)
		{
			client_->Stop();
		}
	}


	bool capture::proc_buffer(sample_buffer &_buffer)
	{
		HRESULT hr;
		UINT32 packetsize;
		UINT32 readed;
		DWORD flags;
		BYTE *data;
		UINT64 pcposition;

		hr = capture_client_->GetNextPacketSize(&packetsize);
		if (hr != S_OK) return false;
		if (packetsize == 0) return false;

		hr = capture_client_->GetBuffer(&data, &readed, &flags, NULL, &pcposition);
		if (hr != S_OK) return false;

		if (readed != last_size_)
		{
			wlog(id_, "capturesize=" + std::to_string(readed));
			last_size_ = readed;
		}
		_buffer.set(data, readed);

		hr = capture_client_->ReleaseBuffer(readed);
		if (hr != S_OK) return false;

		total_frame_ += readed;

		return true;
	}

	UINT64 capture::get_total_frame() const
	{
		return total_frame_;
	}

	UINT32 capture::get_buffer_size()
	{
		HRESULT hr;
		if (client_)
		{
			hr = client_->GetBufferSize(&buffersize_);
			if (hr == S_OK)
			{
				return buffersize_;
			}
		}
		return 0;
	}
}
