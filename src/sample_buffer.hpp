#pragma once

#include "common.hpp"

#include <vector>

namespace app
{

	class sample_buffer
	{
	private:
		std::vector<BYTE> buffer_;
		UINT32 last_set_;
		UINT32 last_get_;
		UINT64 skip_count_;
		UINT64 duplicate_count_;
		UINT32 skip_threshold_;
		UINT32 duplicate_threshold_;
		UINT64 set_count_;
		UINT64 get_count_;

	public:
		sample_buffer(UINT32, UINT32, UINT32);
		~sample_buffer();

		void set(const BYTE*, UINT32);
		void get(BYTE*, UINT32);

		UINT64 get_skip_count() const;
		UINT64 get_duplicate_count() const;
		void get_count(UINT64 &, UINT64 &) const;
	};
}
