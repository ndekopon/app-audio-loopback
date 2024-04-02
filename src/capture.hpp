#pragma once

#include "common.hpp"

#include "sample_buffer.hpp"

#include <string>

#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

namespace app
{

	class capture
	{
	private:
		uint8_t id_;
		IAudioClient* client_;
		IAudioCaptureClient* capture_client_;
		WAVEFORMATEX format_;
		UINT32 buffersize_;
		UINT32 last_size_;
		UINT64 total_frame_;

		IAudioClient* get_client(DWORD);

	public:
		capture(uint8_t _id);
		~capture();

		HANDLE event_;

		// コピー不可
		capture(const capture&) = delete;
		capture& operator = (const capture&) = delete;
		// ムーブ不可
		capture(capture&&) = delete;
		capture& operator = (capture&&) = delete;

		bool init();
		bool start(DWORD);
		void stop();

		bool proc_buffer(sample_buffer &);

		UINT64 get_total_frame() const;

		UINT32 get_buffer_size();
	};
}
