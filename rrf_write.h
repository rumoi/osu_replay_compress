#pragma once

#include "rrf_shared.h"
#include <algorithm>
#include <intrin.h> // _BitScanForward 

void add_to_u8(std::vector<u8>& output, u32 input) {

	const auto i{ output.size() };

	output.resize(i + 4);

	memcpy(output.data() + i, &input, 4);
}

void add_to_u8(std::vector<u8>& output, const void* src, size_t size) {

	const auto i{ output.size() };

	output.resize(i + size);

	memcpy(output.data() + i, src, size);
}


template<typename T>
void add_to_u8(std::vector<u8>& output, const std::vector<T>& input) {

	constexpr auto SIZE{ sizeof(T) };

	const auto i{ output.size() };

	output.resize(i + input.size() * SIZE);

	memcpy(output.data() + i, input.data(), input.size() * SIZE);
}

void unique_add(u32 v, std::vector<std::pair<u32, u32>>& b) {

	for (auto& t : b) {
		if (t.first == v) {
			++t.second;
			return;
		}
	}

	b.push_back({ v,1 });

};

u32 unique_get_(u32 v, const std::vector<std::pair<u32, u32>>& b) {

	for (size_t i{}; i < b.size(); ++i) {

		if (b[i].first == v) {
			return i;
		}

	}
	return 0;
};

const auto pair_sort = [](const auto& x, const auto& y) {
	return x.second > y.second;
};

namespace comp {

	CLzmaEncProps exp_sustain_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;

		p.numHashBytes = 3;

		p.lc = 2;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps screen_space_sign_sustain(u32 size) {

		size /= 8;

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;

		p.numHashBytes = 3;

		p.lc = 1;

		if (size > 12288) {
			p.lc = 2;
		}

		p.pb = 0;

		return p;
	}


	CLzmaEncProps exp_stream_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;

		p.numHashBytes = 4;

		p.lc = 1;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps time_delta_prop(const size_t size) {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 8;

		p.numHashBytes = 4;

		p.lc = (size / 8) >= 10238 ? 4 : 3;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps pos_delta_prop(size_t size) {

		size /= 8;

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.level = 5;

		p.fb = 32;
		p.mc = 16;

		p.numHashBytes = 4;

		p.lc = 1;

		if (size > 65536)
			p.lc = 5;
		else if (size > 32768)
			p.lc = 4;
		else if (size > 16384)
			p.lc = 3;
		else if (size > 4096)
			p.lc = 2;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps composite_key_prop(size_t size) {

		size /= 8;

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.level = 5;

		p.fb = 32;
		p.mc = 16;

		p.numHashBytes = 4;

		p.lc = 2;
		if (size <= 12288)
			p.lc = 1;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps lowf_prop(size_t size) {

		size /= 8;

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.level = 5;

		p.fb = 32;
		p.mc = 16;

		p.numHashBytes = 3;


		if (size >= 32768 * 2)
			p.lc = 3;

		p.lc = 2;
		if (size <= 12288)
			p.lc = 1;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps lowf_sign_prop(size_t size) {

		size /= 8;

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.level = 5;

		p.fb = 16;
		p.mc = 16;

		p.lc = 2;

		if (size >= 32768 * 2)
			p.lc = 3;

		if (size <= 12288)
			p.lc = 1;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps man_8_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.level = 5;

		p.fb = 32;
		p.mc = 32;
		p.numHashBytes = 3;

		p.lc = 4;

		p.pb = 0;

		return p;


	}

	CLzmaEncProps man_16_prop() {
		CLzmaEncProps p;
		LzmaEncProps_Init(&p);
		
		p.level = 5;

		p.fb = 64;
		p.mc = 32;
		p.numHashBytes = 2;
		
		p.lc = 4;
		//p.lp = 1; // Saves ~0.067% but with a huge perf cost
		
		p.pb = 1;//2 byte alignment

		return p;
	}

	std::vector<char> compress(const bit_stream& b, CLzmaEncProps prop) {

		std::vector<char> ret;
		ret.resize(std::max(int(b.data.size() * 1.5f), 1024));

		size_t len{ ret.size() - 9 };

		prop.dictSize = b.data.size();

		*(u32*)ret.data() = b.data.size();

		SizeT propsSize = 5;

		LzmaEncode((u8*)ret.data() + 9, &len, (u8*)b.data.data(), b.data.size(),
			&prop, (u8*)ret.data() + 4, &propsSize, 0,
			0,
			&lzma_allocFuncs, &lzma_allocFuncs
		);

		ret.resize(len + 9);
		return ret;
	}

	std::vector<char> compress_debug(const bit_stream& b, CLzmaEncProps prop) {

		const auto ret = compress(b, prop);

		printf("\ncompress ratio:%i %f\n", ret.size(), float(ret.size()) / float(b.data.size()));

		return ret;
	}

	std::vector<char> compress_conditional(const bit_stream& b, CLzmaEncProps prop, bool& not_compressed) {
		
		const auto ret = compress(b, prop);

		typedef union _cast {
			std::vector<u8> u;
			std::vector<char> s;
		};

		return (not_compressed = ret.size() >= b.data.size()) ?
			((_cast*)&b.data)->s : ret;

	}

}

struct sign_chunk {
	u32 sign : 1, count : 31;
};

struct _exponent_stream {

	std::vector<u8> absolute_table;

	void set_absolute_table(const std::vector<std::pair<u32, u32>>& input) {

		absolute_table.resize(input.size());

		u8* p = absolute_table.data();

		for (auto& [_exp, freq] : input)
			*(p++) = _exp;

	}

	u8 get_absolute_index(const u8 e) const {

		for (size_t i{}, size{ absolute_table.size() }; i < size; ++i) {

			if (absolute_table[i] == e)
				return i;
		}

		return 0;
	}

	u32 chunk_count;
	bit_stream sustain_data;
	bit_stream abs_table_stream;
	bit_stream stream;

	void write_absolute(u8 index) {

		stream.push_back(1);

		//small_write<3>(index, stream);
		write_bucket<3>(index, stream, 3);

	}

	void compress(const std::vector<u8>& input) {

		sustain_data.clear();
		stream.clear();
		abs_table_stream.clear();

		if (input.size() <= 1)
			return;

		struct _chunk {
			u32 sustain;
			u32 exp;
		};

		struct _ch {
			u32 consume;
			u32 irepeat;
		};

		const auto calculate_consume = [](const _chunk* start, const _chunk* end, int delta) {

			_ch ret{ 1, 0 };

			while (start + ret.consume < end && start[ret.consume].exp - start[ret.consume - 1].exp == delta)
				++ret.consume;

			const _chunk* next{ start + ret.consume };

			while (next < end) {

				delta = -delta;

				const auto new_delta = (int)next[0].exp - (int)next[-1].exp;

				if (delta != new_delta)
					break;

				// Proper sign, check if we can consume enough.
				int consume{ 1 };

				while (consume < ret.consume && next + consume < end && (next[consume].exp - next[consume - 1].exp) == delta)
					++consume;

				if (consume != ret.consume)
					break;

				++ret.irepeat;
				next += ret.consume;

			}

			return ret;
		};

		std::vector<_chunk> chunk{};

		chunk.push_back({ 0, input[1] });

		for (size_t i{ 2 }, size{ input.size() }; i < size; ++i) {

			const auto exp{ input[i] };
			auto& back = chunk.back();

			if (back.exp == exp)
				++back.sustain;
			else
				chunk.push_back({ 0, exp });

		}

		const auto* chunk_end{ chunk.data() + chunk.size() };

		// Construct absolute table
		{

			std::vector<std::pair<u32, u32>> tbl{};
			tbl.push_back({ (u32)input[0], 1 });

			int last{ (int)input[0] };

			for (size_t i{}, size{ chunk.size() }; i < size; ++i) {

				const auto& c{ chunk[i] };

				const int delta{ (int)c.exp - last };

				if (abs(delta) != 1) {
				WRITE_ABS_pre:
					unique_add((u32)c.exp, tbl);
					last = (int)c.exp;
					continue;
				}

				auto con = calculate_consume(&c, chunk_end, delta);

				if (con.irepeat == 0) {
					if (con.consume <= 1)
						goto WRITE_ABS_pre;
				}

				i += con.consume * (con.irepeat + 1);
				last = chunk[--i].exp;

			}

			std::sort(tbl.begin(), tbl.end(), pair_sort);

			set_absolute_table(tbl);

		}

		{

			int last{ (int)absolute_table[0] };

			write_bucket<3>(absolute_table[0] ^ 128, abs_table_stream, 4);

			for (size_t i{ 1 }; i < absolute_table.size(); ++i) {

				const auto v{ absolute_table[i] };

				const int delta{ (int)v - (int)last };

				last = (int)v;

				write_bucket<3>(abs(delta), abs_table_stream, 2);
				small_write<1>(delta < 0, abs_table_stream);

			}

			write_bucket<3>(0, abs_table_stream, 2);

		}

		//small_write<3>(get_absolute_index(input[0]), stream);
		write_bucket<3>(get_absolute_index(input[0]), stream, 3);

		{

			int last{ (int)input[0] };

			bool IMPL_SIGN{};

			for (size_t i{}, size{ chunk.size() }; i < size; ++i) {

				const auto& c{ chunk[i] };

				const int delta{ (int)c.exp - last };

				if (abs(delta) != 1) {
				WRITE_ABS:
					IMPL_SIGN = 0;
					write_absolute(get_absolute_index(c.exp));

					last = (int)c.exp;
					continue;
				}

				auto con = calculate_consume(&c, chunk_end, delta);

				if (con.irepeat == 0) {

					if (con.consume <= 1)
						goto WRITE_ABS;

					// Could check to make sure we are not thrashing a future packet here.

				}

				stream.push_back(0);

				if (IMPL_SIGN == 0)
					stream.push_back(delta != 1);

				IMPL_SIGN = (con.consume == 1);

				write_bucket((u32)con.consume - 1, stream, EXP_ICHUNK_CONSUME);
				write_bucket(con.irepeat, stream, EXP_ICHUNK_REPEAT);

				i += con.consume * (1 + con.irepeat);
				last = chunk[--i].exp;

			}
		}

		chunk_count = chunk.size();

		for (const auto& c : chunk) {

			write_bucket(c.sustain, sustain_data, GAME_SPACE_EXP_SUSTAIN);
		}

	}

};

bit_stream combine_bool_vector(const bit_stream& a, const bit_stream& b) {

	bit_stream comp{ a };

	comp.append(b);

	return comp;
}

struct _mantissa_stream {
	
	bit_stream stream[2];

	u16 last16;
	u8 last8;
	void push_back(u32 V) {

		u16 c16{ u16(V) };
		u8 c8{ u8(V >> 16) };
		
		c16 -= last16;
		c8 -= last8;

		last16 = u16(V);
		last8 = u8(V >> 16);

		stream[0].data.push_back(u8(c16));
		stream[0].data.push_back(u8(c16 >> 8));

		stream[1].data.push_back(c8);

	}

	std::vector<u8> get_comp() const {

		std::vector<u8> final_bytes{};

		const CLzmaEncProps prop[2]{
			comp::man_16_prop(),
			comp::man_8_prop()
		};

		for (size_t i{}; i < 2; ++i) {

			if (stream[i].size() == 0) {
				add_to_u8(final_bytes, 0);
				continue;
			}

			bool not_compressed{};

			const auto c{ comp::compress_conditional(stream[i], prop[i], not_compressed) };
			
			add_to_u8(final_bytes, c.size() | (u32(!not_compressed) << 31));

			add_to_u8(final_bytes, c);

		}

		return final_bytes;

	}

};

struct _low_fidelity_delta_stream {

	// Mouse movements keep the same sign for extended periods of time.

	bit_stream sign_sustain;

	bit_stream lowf_value;

	void calculate_sign_stream(const std::vector<int>& input) {

		if (input.size() == 0)
			return;

		const u8 starting_sign{ input[0] <= 0 };

		std::vector<u32> sign_change;
		sign_change.push_back({});

		bool last_sign{};

		for (auto v : input) {

			if (v == 0 || (v <= 0) == last_sign) {
				++sign_change.back();
				continue;
			}

			sign_change.push_back(0);
			last_sign = !last_sign;
		}

		for (auto& v : sign_change)
			write_bucket(v, sign_sustain, GAME_SPACE_LOWFI_SIGN_SUSTAIN);

	}

	void compress(const std::vector<int>& input) {

		calculate_sign_stream(input);

		for (auto& v : input)
			write_bucket(abs(v), lowf_value, GAME_SPACE_LOWFI_SIGN_SUSTAIN);

	}

};

struct _key_data_constructor {

	u32 previous;

	std::array<std::vector<u32>, 32> key_sustain;

	_key_data_constructor() {
		previous = 0;

		for (auto& k : key_sustain)
			k.push_back(0);

	}

	bool tick(const u32 keys) {

		const u32 delta{ keys ^ previous };

		previous = keys;

		for (size_t i{}; i < key_sustain.size(); ++i) {

			if ((delta >> i) & 1)
				key_sustain[i].push_back(0);
			else ++key_sustain[i].back();

		}

		return (keys & delta) > 0;
	}

	bool tick_std(u32 keys) {

		if (keys & 4) keys &= ~u32(1);
		if (keys & 8) keys &= ~u32(2);

		return tick(keys);

	}

};

struct _key_data {

	bit_stream k1;
	bit_stream k2;
	bit_stream m1;
	bit_stream m2;
	bit_stream smoke;

	bit_stream composite_key() const {

		bit_stream comp{};

		comp.append(k1);
		comp.append(k2);
		comp.append(m1);
		comp.append(m2);
		comp.append(smoke);

		return comp;
	}



};

struct _time_delta_stream {

	std::vector<int> delta_table;

	bit_stream table_stream;
	bit_stream stream;

	void set_delta_table(const std::vector<std::pair<u32, u32>>& input) {

		delta_table.resize(input.size());

		int* p = delta_table.data();

		for (auto& [_exp, freq] : input)
			*(p++) = std::bit_cast<int>(_exp);

	}

	u32 get_table_index(const int d) const {

		for (size_t i{}, size{ delta_table.size() }; i < size; ++i) {

			if (delta_table[i] == d)
				return i;
		}

		return 0;
	}


};

struct _rrf_construct {

	u32 rrf_version, flags, frame_count, data_count;

	rrf_tag tag_table[256];
	_rrf_data_block data_table[256];

	std::vector<u8> file_data;

	void add_compress_stream(rrf_tag tag, const bit_stream& data, const CLzmaEncProps& props) {

		if (data_count == 255 || data.size() == 0) [[unlikely]]
			return;

		ON_SCOPE_EXIT( ++data_count; );

		tag_table[data_count] = tag;

		auto& dt{ data_table[data_count] };

		bool not_compressed{};

		const auto c{ comp::compress_conditional(data, props, not_compressed) };

		if (not_compressed) {

			dt.is_compressed = 0;
			dt.size = data.data.size();

			add_to_u8(file_data, data.data);

		} else {

			dt.is_compressed = 1;
			dt.size = c.size();

			add_to_u8(file_data, c);
		}

	}

	void add_stream(rrf_tag tag, const void* src, size_t size) {

		if (data_count == 255 || size == 0) [[unlikely]]
			return;

		ON_SCOPE_EXIT( ++data_count; );

		tag_table[data_count] = tag;

		auto& dt{ data_table[data_count] };
		
		dt.is_compressed = 0;
		dt.size = size;

		const auto s{ file_data.size() };

		file_data.resize(s + size);
		memcpy(file_data.data() + s, src, size);

	}

	void add_stream(rrf_tag tag, const bit_stream& src) {
		add_stream(tag, src.data.data(), src.data.size());
	}

	u32 lowfi_count;

	_time_delta_stream time_delta;

	_low_fidelity_delta_stream lowfi[2];

	std::vector<u32> sign[2];

	_exponent_stream exp[2];

	_mantissa_stream man[2];

	void write(const char* output_file, const _key_data_constructor& kd) {

		{

			bit_stream key_data{};
			key_data.in_count = 8;

			u32 active_flags{};

			for (size_t i{}; i < kd.key_sustain.size(); ++i)
				active_flags |= (kd.key_sustain[i].size() > 1) << i;

			add_to_u8(key_data.data, active_flags);

			// Release sustains have a higher entropic state than pressing on average.

			for (const auto& ks : kd.key_sustain) {

				if (ks.size() <= 1)
					continue;

				for (const auto& v : ks)
					write_bucket(v, key_data, BUCKET_COMP_KEYS);

			}

			add_compress_stream(rrf_tag::key_bit_stream, key_data, comp::composite_key_prop(key_data.size()));

		}

		{

			DIAG::OUTPUT_SIZE = 0;
			//puts("");
			for (size_t i{}; i < data_count; ++i) {

				if (tag_table[i] == rrf_tag::osr_header)
					continue;

				DIAG::OUTPUT_SIZE += data_table[i].size;
				//printf("%i> %i|%i\n", tag_table[i], data_table[i].is_compressed, data_table[i].size);
			}
			//puts("");

		}

		std::vector<u8> final_file{};

		final_file.reserve(256 * file_data.size());

		final_file.resize(sizeof(_rrf_header));

		auto* h{ (_rrf_header*)final_file.data() };

		h->version = RRF_VERSION;
		h->flags = flags;
		h->frame_count = frame_count;
		h->data_count = data_count;

		add_to_u8(final_file, tag_table, data_count * sizeof(tag_table[0]));
		add_to_u8(final_file, data_table, data_count * sizeof(data_table[0]));

		add_to_u8(final_file, file_data);

		DIAG::using_screen = (flags & RRF_FLAG::using_screenspace) != 0;
		DIAG::OUTPUT_SIZE = final_file.size();

		if(strlen(output_file))
			write_file(output_file, final_file);

	}

};

void encode_delta_time(const _osr& r, _rrf_construct& rrf) {

	std::vector<int> delta_table{};

	{
		std::vector<std::pair<u32, u32>> temp{}; temp.reserve(64);

		for (const auto& v : r)
			unique_add(std::bit_cast<u32>(v.delta), temp);

		std::sort(temp.begin(), temp.end(), pair_sort);

		delta_table.resize(temp.size());

		for (size_t i{}; i < temp.size(); ++i)
			delta_table[i] = std::bit_cast<int>(temp[i].first);

	}

	bit_stream BS{}; BS.data.reserve(r.size());

	{
		// Nowhere near optimal, but it's simple and somewhat low impact

		small_write<5>(delta_table.size(), BS);

		for (const auto& v : delta_table)
			small_write<5>(v < 0 ? -v : v, BS);

		for (const auto& v : delta_table)
			BS.push_back(v < 0);

		rrf.add_stream(rrf_tag::time_delta_table, BS.data.data(), BS.data.size());

	}

	BS.clear();

	{

		const auto get_index = [&delta_table](const int v) {

			for (u32 i{}; i < delta_table.size(); ++i)
				if (v == delta_table[i])
					return i;

			return (u32)0;

		};
		
		for (const auto& v : r) {
			write_bucket(get_index(v.delta), BS, BUCKET_TIME_STREAM);
		}

		rrf.add_compress_stream(rrf_tag::time_delta_stream, BS, comp::time_delta_prop(BS.size()));

	}

}

float get_screen_ratio_single(const _osr& r, float R) {

	u32 flag{};

	for (size_t i{ 2 }; i < std::max(r.size(), (size_t)1) - 1; ++i) {

		const float sp{ r[i].y * R };

		// osu! incorrectly rounds the floats as it serializes, this means every single replay is actually slightly incorrect.
		// Funnily enough doing this pulls back more information than the default .osr format
		constexpr static float epsilon{ 0.001f };

		const int rounded{ (int)std::round(sp) };

		if (abs(std::round(sp) - sp) > epsilon)
			return 0.f;

		flag |= std::bit_cast<u32>(rounded);

	}

	u32 in{};
	_BitScanForward((unsigned long*)&in, flag);

	if (in != 0 && in < 8)
		R /= float(1 << in);

	return R;
}

float get_screen_ratio(const _osr& r) {

	constexpr std::array game_ratio{
		//2.250000f,
		//3.000000f,
		//1.875000f,
		//1.500000f,
		//4.375000f,
		//4.500000f,
		//2.500000f,
		//2.000000f,
		//1.250000f,
		//5.000000f,
		//1.800000f,
		//4.000000f,
		//3.750000f,
		//1.302083f
		1.302083f,
		1.800000f,
		3.000000f,
		3.750000f,
		4.375000f,
		4.000000f,
		4.500000f,
		5.000000f
	};

	for (const auto& f : game_ratio) {
		 
		const auto ratio{ get_screen_ratio_single(r, f) };

		if (ratio != 0.f) {
			return ratio;
		}
	}

	return 0.f;
}

bool encode_replay_screen_space(const char* output_file, const _osr& r, const u32 flags, std::vector<u8> osr_header_bytes = std::vector<u8>{}) {

	const auto screen_ratio{ get_screen_ratio(r) };

	if (screen_ratio == 0.f)
		return 0;

	_rrf_construct result{};

	result.flags = flags | RRF_FLAG::using_screenspace;
	result.frame_count = r.size();

	encode_delta_time(r, result);

	if (osr_header_bytes.size())
		result.add_stream(rrf_tag::osr_header, osr_header_bytes.data(), osr_header_bytes.size());

	result.add_stream(rrf_tag::screen_space_info, &screen_ratio, 4);

	_key_data_constructor kd{};

	{

		bit_stream SS_delta[2]{};
		bit_stream SS_sign_sustain[2]{};

		{

			struct _delta_int_stream {

				u32 sign_count;
				int prev_value;
				bool prev_sign;

				void finish_sign(bit_stream& sign_stream) {

					write_bucket(sign_count, sign_stream, SCREEN_SPACE_SIGN_SUSTAIN);

					sign_count = 0;

				}

				void push_back(const int v, bit_stream& delta_stream, bit_stream& sign_stream) {

					ON_SCOPE_EXIT(prev_value = v; );

					const int delta{ v - prev_value };

					write_bucket(delta < 0 ? -delta : delta, delta_stream, SCREEN_SPACE_DELTA);

					if (const auto new_sign{ delta < 0 }; delta != 0 && new_sign != prev_sign) {

						write_bucket(sign_count, sign_stream, SCREEN_SPACE_SIGN_SUSTAIN);

						prev_sign = new_sign;
						sign_count = 0;

					}
					else
						++sign_count;

				}

			};

			_delta_int_stream pos_delta_meta[2]{};

			for (auto& f : r) {

				kd.tick_std(f.keys);

				float* O{ (float*)&f.x };

				for (size_t i{}; i < 2; ++i) {
					
					pos_delta_meta[i].push_back((int)std::round(O[i] * screen_ratio),
						SS_delta[i],
						SS_sign_sustain[i]
					);

				}

			}

			for (size_t i{}; i < 2; ++i)
				pos_delta_meta[i].finish_sign(SS_sign_sustain[i]);

		}

		{

			const auto& comp{ combine_bool_vector(SS_sign_sustain[0], SS_sign_sustain[1]) };

			result.add_compress_stream(rrf_tag::screen_space_sign_sustain, comp, comp::screen_space_sign_sustain(comp.size()));

		}

		result.add_compress_stream(rrf_tag::screen_space_x_delta, SS_delta[0], comp::pos_delta_prop(SS_delta[0].size()));
		result.add_compress_stream(rrf_tag::screen_space_y_delta, SS_delta[1], comp::pos_delta_prop(SS_delta[1].size()));

		result.write(output_file, kd);

	}

	return 1;

}

void encode_taiko_mania(const char* output_file, const _osr& r, const u32 flags, std::vector<u8> osr_header_bytes = std::vector<u8>{}) {

	_rrf_construct result{};

	if (osr_header_bytes.size())
		result.add_stream(rrf_tag::osr_header, osr_header_bytes.data(), osr_header_bytes.size());

	result.flags = flags;
	result.frame_count = r.size();

	encode_delta_time(r, result);

	_key_data_constructor kd{};

	if (flags & RRF_FLAG::gamemode_mania) {

		struct _p {
			u32 start;
			float v;
		};

		std::vector<_p> scroll{}; scroll.reserve(32);

		scroll.push_back({2,  r[2].y });

		for (size_t i{ 3 }; i < r.size(); ++i) {

			if (scroll.back().v == r[i].y)
				continue;
			
			scroll.push_back({i, r[i].y});

		}

		result.add_stream(rrf_tag::mania_scroll_data, ((u8*)scroll.data()) + 4, (scroll.size() * sizeof(scroll[0])) - 4);

	}

	for (auto& f : r)
		kd.tick(f.keys);

	result.write(output_file, kd);

}

void encode_fruits(const char* output_file, const _osr& r, const u32 flags, std::vector<u8> osr_header_bytes = std::vector<u8>{}) {

	_rrf_construct result{};
	result.frame_count = r.size();
	result.flags = flags;

	if (osr_header_bytes.size())
		result.add_stream(rrf_tag::osr_header, osr_header_bytes.data(), osr_header_bytes.size());

	encode_delta_time(r, result);

	_key_data_constructor kd{};

	_mantissa_stream mantissa{};

	mantissa.stream[0].data.reserve(r.size() * 2);
	mantissa.stream[1].data.reserve(r.size());

	_exponent_stream exponent{};

	std::vector<u8> exp_data; exp_data.reserve(r.size());

	for (const auto& f : r) {

		// CTB is always clamped between 0-512, no sign data is required

		// For some ungodly reason unknown to man, you can use the smoke key in CTB.
		// Seems like an oversight so I'm just dropping it.
		kd.tick(f.keys & 1);

		const floatp c{ f.x };

		exp_data.push_back(c.p.exponent);

		mantissa.push_back(c.p.mantissa);

	}

	{

		exponent.compress(exp_data);

		result.add_stream(rrf_tag::fruits_exp_sustain, &exponent.chunk_count, 4);

		result.add_stream(rrf_tag::game_space_float_x_exponent_absolute_table,
			exponent.abs_table_stream
		);

		result.add_compress_stream(rrf_tag(rrf_tag::game_space_float_x_exponent_sustain),
			exponent.sustain_data, comp::exp_sustain_prop()
		);

		result.add_compress_stream(rrf_tag::game_space_float_x_exponent_stream,
			exponent.stream, comp::exp_stream_prop()
		);

		const auto& comp{ mantissa.get_comp() };

		result.add_stream(rrf_tag::game_space_float_x_mantissa,
			comp.data(), comp.size()
		);

	}

	result.write(output_file, kd);

}

void encode_replay(const char* output_file, _osr& r, const u32 flags, std::vector<u8> osr_header_bytes = std::vector<u8>{}) {

	if (flags & (RRF_FLAG::gamemode_taiko | RRF_FLAG::gamemode_mania)) {

		if (flags & RRF_FLAG::gamemode_mania) {
			
			if (r.size() > 1) {
				r[0].x = 0.f; r[0].y = 0.f;
				r[1].x = 0.f; r[1].y = 0.f;
			}

			for (auto& f : r)
				f.keys |= u32(f.x) << 2;

		}

		return encode_taiko_mania(output_file, r, flags, osr_header_bytes);
	}

	if (flags & RRF_FLAG::gamemode_fruits) {
		return encode_fruits(output_file, r, flags, osr_header_bytes);
	}
	
	if (encode_replay_screen_space(output_file, r, flags, osr_header_bytes))
		return;

	_rrf_construct result{};
	result.frame_count = r.size();
	result.flags = flags;

	if (osr_header_bytes.size())
		result.add_stream(rrf_tag::osr_header, osr_header_bytes.data(), osr_header_bytes.size());

	encode_delta_time(r, result);


	for (size_t i{}; i < 2; ++i) {
		result.man[i].stream[0].data.reserve(r.size() * 2);
		result.man[i].stream[1].data.reserve(r.size());
	}

	_key_data_constructor kd{};

	//constexpr float lowfi_resolution{ 2.25f }; // 1080p (~1.7% cost)
	constexpr float lowfi_resolution{ 1.f };

	{

		std::vector<int> lowfi_delta[2];

		std::vector<u8> exp_data[2];

		result.sign[0].push_back({});
		result.sign[1].push_back({});

		int P_LOWFI[2]{};
		bool P_SIGN[2]{};

		for (auto v : r) {

			const int lowfi[2]{
				(int)::roundf(v.x * lowfi_resolution),
				(int)::roundf(v.y * lowfi_resolution)
			};

			ON_SCOPE_EXIT(
				P_LOWFI[0] = lowfi[0];
				P_LOWFI[1] = lowfi[1];
			);

			const auto key_pressed{ kd.tick_std(v.keys) };

			const bool is_key{ (flags & RRF_FLAG::force_lossless) ||(v.keys != 0) };

			if (is_key == 0) {

				++result.lowfi_count;

				lowfi_delta[0].push_back(lowfi[0] - P_LOWFI[0] );
				lowfi_delta[1].push_back(lowfi[1] - P_LOWFI[1] );

				continue;
			}

			floatp pos[2]{ v.x, v.y };

			for (size_t i{}; i < 2; ++i) {

				exp_data[i].push_back(pos[i].p.exponent);

				result.man[i].push_back(pos[i].p.mantissa);

				if (P_SIGN[i] == pos[i].p.sign || pos[i].f == 0.f) [[likely]]
					++result.sign[i].back();
				else {
					result.sign[i].push_back(1);
					P_SIGN[i] = pos[i].p.sign;
				}

			}

		}

		for (size_t i{}; i < 2; ++i) {
			result.lowfi[i].compress(lowfi_delta[i]);
			result.exp[i].compress(exp_data[i]);
		}

	}

	{

		_game_space_info gsi{};

		gsi.exp_sustain_count[0] = result.exp[0].chunk_count;
		gsi.exp_sustain_count[1] = result.exp[1].chunk_count;
		gsi.lowfi_count = result.lowfi_count;
		gsi.lowfi_resolution = lowfi_resolution;
		gsi.lowf_delta_y_start_bit = result.lowfi[0].lowf_value.size();

		result.add_stream(rrf_tag::game_space_info, &gsi, sizeof(gsi));

	}


	{

		const auto sign_full{ combine_bool_vector(result.lowfi[0].sign_sustain, result.lowfi[1].sign_sustain) };

		result.add_compress_stream(rrf_tag::game_space_lowf_sign,
			sign_full, comp::lowf_sign_prop(sign_full.size())
		);

		const auto lowf_full{ combine_bool_vector(result.lowfi[0].lowf_value, result.lowfi[1].lowf_value) };

		result.add_compress_stream(rrf_tag::game_space_lowf_delta,
			lowf_full, comp::lowf_prop(lowf_full.size())
		);

	}

	for (size_t i{}; i < 2; ++i) {		

		result.add_stream(rrf_tag(i + size_t(rrf_tag::game_space_float_x_sign)),
			result.sign[i].data(), result.sign[i].size() * sizeof(result.sign[i][0])
		);

		result.add_stream(rrf_tag(i + size_t(rrf_tag::game_space_float_x_exponent_absolute_table)),
			result.exp[i].abs_table_stream
		);

		result.add_compress_stream(rrf_tag(i + size_t(rrf_tag::game_space_float_x_exponent_sustain)),
			result.exp[i].sustain_data, comp::exp_sustain_prop()
		);
		
		result.add_compress_stream(rrf_tag(i + size_t(rrf_tag::game_space_float_x_exponent_stream)),
			result.exp[i].stream, comp::exp_stream_prop()
		);

		const auto& comp{ result.man[i].get_comp() };

		result.add_stream(rrf_tag(i + size_t(rrf_tag::game_space_float_x_mantissa)),
			comp.data(), comp.size()
		);

	}

	result.write(output_file, kd);

}

void osr_to_rrf(const char* in_file, const char* out_file, const u32 flags){

	const auto& raw_file{read_file(in_file)};

	if (raw_file.size() < 128)
		return;

	const u8*const file_end{ raw_file.data() + raw_file.size() };

	const u8* lzma_start{ raw_file.data() };
	size_t lzma_size{ raw_file.size() };

	std::vector<u8> header_bytes{};

	if (flags & RRF_FLAG::has_osr_header) {

		const auto skip_string = [&]() {

			if (lzma_start + 2 > file_end || *lzma_start == 0u) {
				++lzma_start;
				return;
			}

			u32 s{ *++lzma_start & 0x7fu }, l_s{};

			while ((*lzma_start & 0x80u) && lzma_start + 1 < file_end) [[unlikely]]
				s |= (*++lzma_start & 0x7fu) << (l_s += 7);

			lzma_start += (++lzma_start + s <= file_end ? s : 0);

		};

		#define padv(type) (lzma_start += sizeof(type))

		padv(u8); // gamemode
		padv(u32); // game_version

		skip_string(); // beatmap_hash
		skip_string(); // player_name
		skip_string(); // replay_hash

		padv(u16); // hit_300
		padv(u16); // hit_100
		padv(u16); // hit_50
		padv(u16); // hit_geki
		padv(u16); // hit_katu
		padv(u16); // hit_miss
		padv(u32); // total_score
		padv(u16); // highest_combo
		padv(u8); // is_perfect
		padv(u32); // mods

		skip_string(); //life_bar

		padv(u64); // time_stamp

		add_to_u8(header_bytes, raw_file.data(), size_t(lzma_start - raw_file.data()));

		padv(u32); // lzma_size - is this in the raw stream too?

		#undef padv

		lzma_size = size_t(file_end - lzma_start);

	}

	if (lzma_size <= 5 + 8)
		return;

	std::vector<u8> osr_string;

	DIAG::INPUT_SIZE = lzma_size;

	size_t osr_size{ *(u32*)(lzma_start + 5) }, input_size{ lzma_size - (5 + 8) };

	ELzmaStatus lzma_status{};

	osr_string.resize(osr_size);

	const auto d = LzmaDecode(osr_string.data(), &osr_size, lzma_start + 5 + 8, &input_size, lzma_start, 5,
		LZMA_FINISH_END, &lzma_status, &lzma_allocFuncs
	);

	if (osr_string.size() != osr_size)
		return;

	const u8* frame_start{ osr_string.data() };

	_osr replay_stream{}; replay_stream.reserve(osr_string.size() / 20);

	const auto add_frame = [&](const u8* end) {

		//end = std::min(end, file_end);
		//frame_start = std::min(frame_start, file_end);

		if (frame_start == end)
			return;

		_osr_frame f{};

		const u8* sep[3]{ end,end,end };
		size_t c{};
		for (const u8* p{ frame_start };  p < end; ++p) {

			if (*p != u8('|'))
				continue;

			sep[c++] = p;

			if (c == 3)
				break;

		}

		std::from_chars((const char*)frame_start, (const char*)sep[0], f.delta);
		std::from_chars((const char*)sep[0] + 1, (const char*)sep[1], f.x);
		std::from_chars((const char*)sep[1] + 1, (const char*)sep[2], f.y);
		std::from_chars((const char*)sep[2] + 1, (const char*)end, f.keys);

		replay_stream.push_back(f);

		frame_start = end + 1;

	};

	for (size_t i{}, size{ osr_string.size() }; i < size; ++i) {

		if (osr_string[i] == u8(',')) {
			add_frame(osr_string.data() + i);
			continue;
		}

	}

	add_frame(osr_string.data() + osr_string.size());

	size_t seed{};

	if (replay_stream.size()) { // Don't need to put in the last frame
		seed = replay_stream.back().keys;
		replay_stream.pop_back();
	}

	encode_replay(out_file, replay_stream, flags, std::move(header_bytes));

}