#pragma once

#include "rrf_shared.h"
#include <algorithm>

void add_to_u8(std::vector<u8>& output, u32 input) {

	const auto i{ output.size() };

	output.resize(i + 4);

	memcpy(output.data() + i, &input, 4);
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

		p.lc = 3;

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

	CLzmaEncProps delta_prop(const size_t size) {

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

	CLzmaEncProps key_data_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 32;
		p.numHashBytes = 4;
		p.btMode = 1;

		p.lc = 3;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps composite_key_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;

		p.numHashBytes = 2;
		p.lc = 2;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps lowf_prop(size_t size) {

		size /= 8;

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;
		p.numHashBytes = 2;

		p.lc = size >= 20476 ? 4 : 3;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps lowf_sign_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;

		p.lc = 3;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps man_16_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;
		p.numHashBytes = 2;

		p.lc = 4;

		p.pb = 1;//2 byte alignment

		return p;
	}

	CLzmaEncProps man_4_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;

		p.lc = 3;

		p.pb = 0;

		return p;
	}

	CLzmaEncProps man_3_prop() {

		CLzmaEncProps p;
		LzmaEncProps_Init(&p);

		p.fb = 32;
		p.level = 5;
		p.mc = 16;
		//p.btMode = 1;

		p.lc = 1;

		p.pb = 0;

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
	bool sustain_4bits;
	bit_stream sustain_data;
	bit_stream abs_table_stream;
	bit_stream stream;

	void write_absolute(u8 index) {

		stream.push_back(1);

		small_write<3>(index, stream);

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

				abs_table_stream.push_back(delta < 0);
				write_bucket<3>(abs(delta), abs_table_stream, 2);

			}

			small_write<2>(0, abs_table_stream);

		}

		small_write<3>(get_absolute_index(input[0]), stream);

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

		bit_stream sustain_data_4{};
		bit_stream sustain_data_5{};

		chunk_count = chunk.size();

		for (const auto& c : chunk) {
			write_bucket(c.sustain, sustain_data_4, 4, 5);
			write_bucket(c.sustain, sustain_data_5, 5, 6);
		}

		sustain_4bits = sustain_data_4.size() < sustain_data_5.size();
		sustain_data = sustain_4bits ? sustain_data_4 : sustain_data_5;

	}

};

bit_stream combine_bool_vector(const bit_stream& a, const bit_stream& b) {

	bit_stream comp{ a };

	comp.append(b);

	return comp;
}

struct _mantissa_stream {

	// 16 4 3
	bit_stream stream[3];

	void push_back(u32 v) {

		for (size_t i{}; i < 16; ++i)
			stream[0].push_back((v >> i) & 1);

		for (size_t i{ 16 }; i < 20; ++i)
			stream[1].push_back((v >> i) & 1);

		for (size_t i{ 20 }; i < 23; ++i)
			stream[2].push_back((v >> i) & 1);

	}

	std::vector<u8> get_comp() const {

		std::vector<u8> final_bytes{};

		const CLzmaEncProps prop[3]{
			comp::man_16_prop(),
			comp::man_4_prop(),
			comp::man_3_prop()
		};

		for (size_t i{}; i < 3; ++i) {

			const auto c{ comp::compress(stream[i], prop[i]) };

			add_to_u8(final_bytes, c.size());
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
			write_bucket(v, sign_sustain, 4, 4, 4, 8);

	}

	void compress(const std::vector<int>& input) {

		calculate_sign_stream(input);

		for (auto& v : input)
			write_bucket(abs(v), lowf_value, 4, 4, 4, 8);

	}

};

struct _key_data_constructor {

	u32 previous;

	std::vector<u32> m1;
	std::vector<u32> m2;
	std::vector<u32> k1;
	std::vector<u32> k2;

	std::vector<u32> smoke;

	_key_data_constructor() {
		previous = 0;
		k1.push_back(0);
		k2.push_back(0);
		m1.push_back(0);
		m2.push_back(0);
		smoke.push_back(0);
	}

	void tick(u32 keys) {

		if (keys & 4) keys &= ~u32(1);
		if (keys & 8) keys &= ~u32(2);

		const u32 delta{ keys ^ previous };
		previous = keys;

		if (delta & 1)
			m1.push_back(0);
		else ++m1.back();

		if (delta & 2)
			m2.push_back(0);
		else
			++m2.back();

		if (delta & 4)
			k1.push_back(0);
		else ++k1.back();

		if (delta & 8)
			k2.push_back(0);
		else ++k2.back();

		if (delta & 16)
			smoke.push_back(0);
		else
			++smoke.back();



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

	u8 rrf_version;

	u32 frame_count, lowfi_count;

	_time_delta_stream time_delta;

	bit_stream key_data;

	_low_fidelity_delta_stream lowfi[2];

	std::vector<u32> sign[2];

	_exponent_stream exp[2];

	_mantissa_stream man[2];

	void write(const char* output_file) {

		std::vector<u8> data{};

		data.resize(sizeof(_rrf_header));

		_rrf_header header{};

		header.rrf_version = rrf_version;

		header.frame_count = frame_count;
		header.lowfi_count = lowfi_count;

		{
			auto& d{ header.time_table };

			d.is_compressed = 0;
			d.size = time_delta.table_stream.data.size();

			add_to_u8(data, time_delta.table_stream.data);

		}

		const auto write_stream = [&](_data_chunk& dc, const bit_stream& bs, const CLzmaEncProps& prop) {

			bool comp{};

			const auto pack{ comp::compress_conditional(bs, prop, comp) };

			add_to_u8(data, pack);

			dc.is_compressed = (comp == 0);
			dc.size = pack.size();

		};

		write_stream(header.time_stream, time_delta.stream, comp::delta_prop(time_delta.stream.size()));

		write_stream(header.composite_key, key_data, comp::composite_key_prop());

		for (size_t i{}; i < 2; ++i) {

			header.high_float[i].exponent.chunk_count = exp[i].chunk_count;
			header.high_float[i].exponent.sustain_4bits = exp[i].sustain_4bits;

			{

				auto& d{ header.high_float[i].sign };

				d.is_compressed = 0;
				d.size = sign[i].size() * sizeof(sign[i][0]);

				add_to_u8(data, sign[i]);

			}


			{

				auto& d{ header.high_float[i].exponent.absolute_table };

				d.is_compressed = 0;
				d.size = exp[i].abs_table_stream.data.size();

				add_to_u8(data, exp[i].abs_table_stream.data);

			}

			write_stream(header.high_float[i].exponent.sustain, exp[i].sustain_data, comp::exp_sustain_prop());
			write_stream(header.high_float[i].exponent.stream, exp[i].stream, comp::exp_stream_prop());

			{

				auto& d{ header.high_float[i].mantissa };

				const auto& comp{ man[i].get_comp() };

				d.is_compressed = 0;
				d.size = comp.size();

				add_to_u8(data, comp);

			}

		}

		write_stream(header.lowf_sign,
			combine_bool_vector(lowfi[0].sign_sustain, lowfi[1].sign_sustain),
			comp::lowf_sign_prop());
		{

			header.lowf_delta_y_start_bit = lowfi[0].lowf_value.size();

			const auto& comp{ combine_bool_vector(lowfi[0].lowf_value, lowfi[1].lowf_value) };

			write_stream(header.lowf_delta, comp, comp::lowf_prop(comp.size()));

		}

		memcpy(data.data(), &header, sizeof(header));

		header.print_sizes();

		DIAG::OUTPUT_SIZE = data.size();

		write_file(output_file, data);

	}

};

void encode_delta(const _osr& r, _rrf_construct& rrf) {

	std::vector<std::pair<u32, u32>> delta_map{}; delta_map.reserve(64);

	for (const auto& v : r)
		unique_add(static_cast<u32>(v.delta), delta_map);

	std::sort(delta_map.begin(), delta_map.end(), pair_sort);

	auto& stream{ rrf.time_delta };

	stream.set_delta_table(delta_map);

	for (const auto& v : r)
		write_bucket(stream.get_table_index(v.delta), stream.stream, BUCKET_TIME_STREAM);

	small_write<5>(stream.delta_table.size(), stream.table_stream);

	for (auto v : stream.delta_table)
		small_write<5>(v, stream.table_stream);

}

void encode_replay(const char* output_file, const _osr& r) {

	_rrf_construct result{};
	result.frame_count = r.size();

	encode_delta(r, result);

	_key_data_constructor kd{};

	{

		std::vector<int> lowfi_delta[2];
		std::vector<u8> exp_data[2];

		result.sign[0].push_back({});
		result.sign[1].push_back({});

		int P_LOWFI[2]{};
		//u32 P_MAN[2]{};
		bool P_SIGN[2]{};

		for (auto v : r) {

			const int lowfi[2]{
				(int)::roundf(v.x),
				(int)::roundf(v.y)
			};

			ON_SCOPE_EXIT(
				P_LOWFI[0] = lowfi[0];
				P_LOWFI[1] = lowfi[1];
			);

			kd.tick(v.keys);

			const bool is_key{ v.keys != 0 };

			if (is_key == 0) {

				++result.lowfi_count;

				lowfi_delta[0].push_back(lowfi[0] - P_LOWFI[0]);
				lowfi_delta[1].push_back(lowfi[1] - P_LOWFI[1]);

				continue;
			}

			floatp pos[2]{ v.x, v.y };

			for (size_t i{}; i < 2; ++i) {

				exp_data[i].push_back(pos[i].p.exponent);

				//const auto X{ mantissa_sub(pos[i].p.mantissa, P_MAN[i]) };
				//
				//P_MAN[i] = u32(pos[i].p.mantissa);

				result.man[i].push_back(pos[i].p.mantissa);

				if (P_SIGN[i] == pos[i].p.sign) [[likely]]
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

	for (auto& v : kd.k1)
		write_bucket(v, result.key_data, BUCKET_COMP_KEYS);

	for (auto& v : kd.k2)
		write_bucket(v, result.key_data, BUCKET_COMP_KEYS);

	for (auto& v : kd.m1)
		write_bucket(v, result.key_data, BUCKET_COMP_KEYS);

	for (auto& v : kd.m2)
		write_bucket(v, result.key_data, BUCKET_COMP_KEYS);

	for (auto& v : kd.smoke)
		write_bucket(v, result.key_data, BUCKET_COMP_KEYS);

	result.write(output_file);


}


void osr_to_rrf(const char* in_file, const char* out_file, bool osr_has_header = 1){

	const auto& raw_file{read_file(in_file)};

	if (raw_file.size() < 128)
		return;

	const u8*const file_end{ raw_file.data() + raw_file.size() };

	const u8* lzma_start{ raw_file.data() };
	size_t lzma_size{ raw_file.size() };

	if (osr_has_header) {

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

		padv(u32); // lzma_size - is this in the raw stream too?

		#undef padv

		lzma_size = size_t(file_end - lzma_start);

	}

	if (lzma_size <= 5 + 8)
		return;

	std::vector<u8> osr_string;


	DIAG::INPUT_SIZE = lzma_size;

	size_t osr_size{}, input_size{ lzma_size - (5 + 8) };
	ELzmaStatus lzma_status{};

	for (int i = 0; i < 8; ++i)
		osr_size |= (lzma_start[LZMA_PROPS_SIZE + i] << (i * 8));

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

			if (c == 3) break;
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

	encode_replay(out_file, replay_stream);

}


