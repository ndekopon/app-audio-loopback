[1mdiff --git a/src/sample_buffer.cpp b/src/sample_buffer.cpp[m
[1mindex 4af0182..be065e7 100644[m
[1m--- a/src/sample_buffer.cpp[m
[1m+++ b/src/sample_buffer.cpp[m
[36m@@ -5,8 +5,8 @@[m
 namespace[m
 {[m
 	constexpr UINT32 BUFFER_SECONDS = 10;[m
[31m-	constexpr UINT32 COMMON_LOWER = app::COMMON_SAMPLES * BUFFER_SECONDS / 4 * 1;[m
[31m-	constexpr UINT32 COMMON_HIGHER = app::COMMON_SAMPLES * BUFFER_SECONDS / 4 * 3;[m
[32m+[m	[32mconstexpr UINT32 COMMON_LOWER = app::COMMON_SAMPLES * BUFFER_SECONDS * 1 / 4;[m
[32m+[m	[32mconstexpr UINT32 COMMON_HIGHER = app::COMMON_SAMPLES * BUFFER_SECONDS * 3 / 4;[m
 [m
 	inline bool lesser(uint64_t _a, uint64_t _b)[m
 	{[m
[36m@@ -20,6 +20,22 @@[m [mnamespace[m
 		if (_b < COMMON_LOWER && COMMON_HIGHER < _a) return false;[m
 		return _a > _b;[m
 	}[m
[32m+[m
[32m+[m	[32minline void skip(UINT32& _m, const unsigned int _buffer_frames, UINT32 _frames, UINT64& _count, UINT64& _get_count)[m
[32m+[m	[32m{[m
[32m+[m		[32m_m += 1;[m
[32m+[m		[32m_m %= _buffer_frames;[m
[32m+[m		[32m_count++;[m
[32m+[m		[32m_get_count += _frames + 1;[m
[32m+[m	[32m}[m
[32m+[m
[32m+[m	[32minline void duplicate(UINT32& _m, const unsigned int _buffer_frames, UINT32 _frames, UINT64& _count, UINT64& _get_count)[m
[32m+[m	[32m{[m
[32m+[m		[32m_m += _buffer_frames - 1;[m
[32m+[m		[32m_m %= _buffer_frames;[m
[32m+[m		[32m_count++;[m
[32m+[m		[32m_get_count += (_frames - 1);[m
[32m+[m	[32m}[m
 }[m
 [m
 [m
[36m@@ -70,26 +86,45 @@[m [mnamespace app[m
 		auto m = last_get_;[m
 		auto w = last_set_;[m
 [m
[31m-		// しきい値によるスキップ処理[m
[31m-		auto wl = (w + buffer_frames - (COMMON_SAMPLES / 1000) * skip_threshold_) % buffer_frames;[m
[31m-		auto wg = (w + buffer_frames - (COMMON_SAMPLES / 1000) * duplicate_threshold_) % buffer_frames;[m
[31m-		if (lesser((m + _frames) % buffer_frames, wl))[m
[31m-		{[m
[31m-			m += 1;[m
[31m-			m %= buffer_frames;[m
[31m-			skip_count_++;[m
[31m-			get_count_ += _frames + 1;[m
[31m-		}[m
[31m-		else if (greater((m + _frames) % buffer_frames, wg))[m
[32m+[m		[32m// しきい値によるスキップ＆重複処理[m
[32m+[m		[32mauto w_s = (w + buffer_frames - (COMMON_SAMPLES / 1000) * skip_threshold_) % buffer_frames;[m
[32m+[m		[32mauto w_d = (w + buffer_frames - (COMMON_SAMPLES / 1000) * duplicate_threshold_) % buffer_frames;[m
[32m+[m[41m		[m
[32m+[m		[32mauto next_m = (m + _frames) % buffer_frames;[m
[32m+[m		[32mif (w_s < w_d)[m
 		{[m
[31m-			m += buffer_frames - 1;[m
[31m-			m %= buffer_frames;[m
[31m-			duplicate_count_++;[m
[31m-			get_count_ += (_frames - 1);[m
[32m+[m			[32mif (lesser(next_m, w_s))[m
[32m+[m			[32m{[m
[32m+[m				[32mskip(m, buffer_frames, _frames, skip_count_, get_count_);[m
[32m+[m			[32m}[m
[32m+[m			[32melse if (greater(next_m, w_d))[m
[32m+[m			[32m{[m
[32m+[m				[32mduplicate(m, buffer_frames, _frames, duplicate_count_, get_count_);[m
[32m+[m			[32m}[m
[32m+[m			[32melse[m
[32m+[m			[32m{[m
[32m+[m				[32mget_count_ += _frames;[m
[32m+[m			[32m}[m
 		}[m
[31m-		else[m
[32m+[m		[32melse if (w_d < w_s)[m
 		{[m
[31m-			get_count_ += _frames;[m
[32m+[m			[32mif (greater(next_m, w_d) && lesser(next_m, w_s))[m
[32m+[m			[32m{[m
[32m+[m				[32mauto a = w_s - next_m;[m
[32m+[m				[32mauto b = next_m - w_d;[m
[32m+[m				[32mif (a > b)[m
[32m+[m				[32m{[m
[32m+[m					[32mduplicate(m, buffer_frames, _frames, duplicate_count_, get_count_);[m
[32m+[m				[32m}[m
[32m+[m				[32melse[m
[32m+[m				[32m{[m
[32m+[m					[32mskip(m, buffer_frames, _frames, skip_count_, get_count_);[m
[32m+[m				[32m}[m
[32m+[m			[32m}[m
[32m+[m			[32melse[m
[32m+[m			[32m{[m
[32m+[m				[32mget_count_ += _frames;[m
[32m+[m			[32m}[m
 		}[m
 [m
 		if (m + _frames > buffer_frames)[m
