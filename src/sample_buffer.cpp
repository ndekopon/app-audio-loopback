#include "sample_buffer.hpp"

#include "log.hpp"

namespace
{
	constexpr UINT32 COMMON_LOWER = app::COMMON_SAMPLES / 4 * 1;
	constexpr UINT32 COMMON_HIGHER = app::COMMON_SAMPLES / 4 * 3;

	bool lesser(uint64_t _a, uint64_t _b)
	{
		if (_b < COMMON_LOWER && COMMON_HIGHER < _a) return true;
		if (_a < COMMON_LOWER && COMMON_HIGHER < _b) return false;
		return _a < _b;
	}
	bool greater(uint64_t _a, uint64_t _b)
	{
		if (_a < COMMON_LOWER && COMMON_HIGHER < _b) return true;
		if (_b < COMMON_LOWER && COMMON_HIGHER < _a) return false;
		return _a > _b;
	}
}


namespace app
{
	sample_buffer::sample_buffer(UINT32 _duplicate_threshold, UINT32 _threashold_interval)
		: buffer_(static_cast<size_t>(COMMON_SAMPLES * COMMON_BYTES_PERFRAME), 0)
		, last_set_(0)
		, last_get_(COMMON_SAMPLES - (COMMON_SAMPLES / 1000) * 10) // 初回は10ms遅延入れておく
		, skip_count_(0)
		, duplicate_count_(0)
		, skip_threshold_(_duplicate_threshold + _threashold_interval)
		, duplicate_threshold_(_duplicate_threshold)
		, set_count_(0)
		, get_count_(0)
		, manual_skip_(0)
	{
	}

	sample_buffer::~sample_buffer()
	{
	}

	void sample_buffer::set(const BYTE* _data, UINT32 _frames)
	{
		auto m = last_set_;

		if (m + _frames > COMMON_SAMPLES)
		{
			auto f2 = (m + _frames) - COMMON_SAMPLES;
			auto f1 = _frames - f2;
			std::memcpy(&buffer_.at(m * COMMON_BYTES_PERFRAME), _data +  0 * COMMON_BYTES_PERFRAME, f1 * COMMON_BYTES_PERFRAME);
			std::memcpy(&buffer_.at(0 * COMMON_BYTES_PERFRAME), _data + f1 * COMMON_BYTES_PERFRAME, f2 * COMMON_BYTES_PERFRAME);
			last_set_ = f2;
		}
		else
		{
			std::memcpy(&buffer_.at(m * COMMON_BYTES_PERFRAME), _data, _frames * COMMON_BYTES_PERFRAME);
			last_set_ = m + _frames;
		}
		set_count_ += _frames;
		last_set_ %= COMMON_SAMPLES;
	}

	void sample_buffer::get(BYTE* _data, UINT32 _frames)
	{
		auto m = last_get_;
		auto w = last_set_;
		auto wl = (w + COMMON_SAMPLES - (COMMON_SAMPLES / 1000) * skip_threshold_) % COMMON_SAMPLES;
		auto wg = (w + COMMON_SAMPLES - (COMMON_SAMPLES / 1000) * duplicate_threshold_) % COMMON_SAMPLES;
		if (lesser((m + _frames) % COMMON_SAMPLES, wl))
		{
			m += 1;
			m %= COMMON_SAMPLES;
			skip_count_++;
			get_count_ += _frames + 1;
		}
		else if (greater((m + _frames) % COMMON_SAMPLES, wg))
		{
			m += COMMON_SAMPLES - 1;
			m %= COMMON_SAMPLES;
			duplicate_count_++;
			get_count_ += _frames - 1;
		}
		else
		{
			// 500ms -> 1skip
			auto skip = get_count_ / (COMMON_SAMPLES / 2);
			if (skip_count_ > 0 && skip != manual_skip_)
			{
				m += 1;
				m %= COMMON_SAMPLES;
				manual_skip_ = skip;
				get_count_ += _frames - 1;
			}
			else
			{
				get_count_ += _frames;
			}
		}

		if (m + _frames > COMMON_SAMPLES)
		{
			auto f2 = (m + _frames) - COMMON_SAMPLES;
			auto f1 = _frames - f2;
			std::memcpy(_data +  0 * COMMON_BYTES_PERFRAME, &buffer_.at(m * COMMON_BYTES_PERFRAME), f1 * COMMON_BYTES_PERFRAME);
			std::memcpy(_data + f1 * COMMON_BYTES_PERFRAME, &buffer_.at(0 * COMMON_BYTES_PERFRAME), f2 * COMMON_BYTES_PERFRAME);
			last_get_ = f2;
		}
		else
		{
			std::memcpy(_data, &buffer_.at(m * COMMON_BYTES_PERFRAME), _frames * COMMON_BYTES_PERFRAME);
			last_get_ = m + _frames;
		}
		last_get_ %= COMMON_SAMPLES;
	}

	UINT64 sample_buffer::get_skip_count()
	{
		return skip_count_;
	}

	UINT64 sample_buffer::get_duplicate_count()
	{
		return duplicate_count_;
	}

	void sample_buffer::get_count(UINT64 &_get_count, UINT64 &_set_count)
	{
		_get_count = get_count_;
		_set_count = set_count_;
	}
}
