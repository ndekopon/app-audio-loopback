#pragma once

#include "common.hpp"

#include "sample_buffer.hpp"

#include <string>

#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

namespace app
{

	class render
	{
	private:
		uint8_t id_;
		std::vector<IMMDevice*> devices_;
		std::vector<std::wstring> names_;
		IAudioClient* client_;
		IAudioRenderClient* render_client_;
		ISimpleAudioVolume* volume_;
		WAVEFORMATEX format_;
		UINT32 buffersize_;
		UINT32 last_size_;
		UINT64 total_frame_;

		bool get_client_and_volume(const std::wstring&);

	public:
		render(uint8_t _id);
		~render();

		HANDLE event_;

		// コピー不可
		render(const render&) = delete;
		render& operator = (const render&) = delete;
		// ムーブ不可
		render(render&&) = delete;
		render& operator = (render&&) = delete;

		bool init();
		bool start(const std::wstring&, UINT32);
		void stop();

		bool proc_buffer(sample_buffer &);

		void set_volume(UINT32);

		const std::vector<std::wstring> get_names() const;

		UINT64 get_total_frame() const;
	};
}
