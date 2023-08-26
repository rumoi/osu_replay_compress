#pragma once

#include "rrf_shared.h"
#include <charconv>

struct _stream_map {

	struct _elem {
		u8 tag;
		std::vector<u8> data;
	};

	std::vector<_elem> data;

	const std::vector<u8>& operator[](rrf_tag tag) const {

		for (const auto& v : data) {

			if (v.tag != u8(tag))
				continue;

			return v.data;

		}

		return {};
	}

};

namespace rrf {

	void lzma_decomp(std::vector<u8>& ret, const u8* data, size_t size) {

		size_t decomp_size{ *(u32*)data };

		ret.resize(decomp_size);

		size_t input_size{ size - (4 + LZMA_PROPS_SIZE) };
		ELzmaStatus lzma_status{};

		const auto rcode{
			LzmaDecode(ret.data(), &decomp_size, data + LZMA_PROPS_SIZE + 4, &input_size, data + 4,
			LZMA_PROPS_SIZE, LZMA_FINISH_END, &lzma_status, &lzma_allocFuncs)
		};

		if (rcode != SZ_OK) {
			ret.clear();
			return;
		}

		ret.resize(decomp_size);

	};

	std::vector<u8> read_chunk(const _data_chunk dc, u8*& data) {

		std::vector<u8> ret{};

		if (dc.is_compressed) {
			lzma_decomp(ret, data, dc.size);
		}
		else {
			ret.resize(dc.size);
			memcpy(ret.data(), data, dc.size);
		}

		data += dc.size;

		return ret;
	};

}

template<int stride = 2>
void extract_exponent_stream(const u32 key_frame_count, floatp* output,
	const u8* absolute_table,
	const bit_stream& stream_bit,
	const bit_stream& sustain_bit,
	std::vector<u32>& sustain) {

	{

		size_t bit_offset{};

		for (auto& v : sustain)
			v = read_bucket(sustain_bit, bit_offset, GAME_SPACE_EXP_SUSTAIN);

	}

	size_t bit_offset{};
	u8 last_exponent{};
	size_t exp_i{};

	const auto write_exponent = [&](u8 exp, u32 count) {

		for (size_t i{}; i <= count; ++i)
			output[stride * (exp_i++)].p.exponent = exp;

		last_exponent = exp;

	};

	write_exponent(absolute_table[read_bucket<3>(stream_bit, bit_offset, 3)], 0);

	bool double_sign{};
	bool IMPL_SIGN{};

	for (size_t sustain_i{}; exp_i < key_frame_count; ) {

		if (small_read<1>(stream_bit, bit_offset) == 1) {

			const auto index{ read_bucket<3>(stream_bit, bit_offset, 3) };

			write_exponent(absolute_table[index], sustain[sustain_i++]);
			IMPL_SIGN = 0;

		}
		else {

			auto current_sign{ IMPL_SIGN ? double_sign : stream_bit[bit_offset++] };

			const auto consume{
				read_bucket(stream_bit, bit_offset, EXP_ICHUNK_CONSUME)
			};

			const auto repeat{
				read_bucket(stream_bit, bit_offset, EXP_ICHUNK_REPEAT)
			};

			IMPL_SIGN = (consume == 0);

			u8 base{ last_exponent };

			for (size_t r{}; r <= repeat; ++r) {

				for (size_t c{}; c <= consume; ++c) {

					if (current_sign) --base;
					else ++base;

					write_exponent(base, sustain[sustain_i++]);

				}

				current_sign = !current_sign;
			}

			double_sign = !current_sign;

		}

	}

}


template<int stride = 2>
void extract_high_float(const _stream_map& stream_data, const u32 index,
	const u32 key_frame_count, const u32 sustain_length,
	floatp* float_table) {

	float_table += index;

	std::vector<u32> sustain_buffer{}; sustain_buffer.resize(sustain_length);

	// Exponent
	{

		u8 absolute_table[256];

		{

			bit_stream table_stream{ stream_data[rrf_tag(u8(rrf_tag::game_space_float_x_exponent_absolute_table) + index)] };

			u8* v{ absolute_table };

			size_t i{};

			*(v++) = read_bucket<3>(table_stream, i, 4) ^ 128;

			for (;;) {


				auto delta{ (int)read_bucket<3>(table_stream, i, 2) };

				if (delta == 0)
					break;

				delta = small_read<1>(table_stream, i) ? -delta : delta;

				const auto prev{ *(v - 1) };

				*(v++) = (prev + delta);


			}

		}

		extract_exponent_stream<stride>(key_frame_count, float_table,
			absolute_table,
			bit_stream{ stream_data[rrf_tag(u8(rrf_tag::game_space_float_x_exponent_stream) + index)] },
			bit_stream{ stream_data[rrf_tag(u8(rrf_tag::game_space_float_x_exponent_sustain) + index)] },
			sustain_buffer
		);

	}

	// Mantissa
	{

		const auto mantissa_buffer{ stream_data[rrf_tag(u8(rrf_tag::game_space_float_x_mantissa) + index)] };
				
		const u8* d{ mantissa_buffer.data() };

		bit_stream d_buff{};

		constexpr size_t start[]{ 0, 16, 24 };

		for (size_t mc{}; mc < 2; ++mc) {

			auto size{ *(u32*)d };

			if (size == 0) {
				d += 4;
				continue;
			}

			if (size & ((u32)1 << 31)) {
				size <<= 1;
				size >>= 1;
				rrf::lzma_decomp(d_buff.data, d + 4, size);
			}
			else {
				d_buff.data.resize(size);
				memcpy(d_buff.data.data(), d + 4, size);
			}

			u16 last16{};
			u8 last8{};

			size_t di{};
			for (size_t i{}; i < key_frame_count; ++i) {

				u32 mantissa{};

				for (size_t z{ start[mc] }; z < start[mc + 1]; ++z)
					mantissa |= (u32(d_buff[di++]) << z);

				float_table[stride * i].p.mantissa |= mantissa;

				if (mc == 1) {

					mantissa = float_table[stride * i].p.mantissa;

					u16 t16{ u16(mantissa) };
					u8 t8{ u8(mantissa >> 16) };

					t8 += last8;
					t16 += last16;

					last8 = t8;
					last16 = t16;

					float_table[stride * i].p.mantissa = (u32(t8) << 16) | u32(t16);

				}

			}

			d += size + 4;
			d_buff.clear();

		}


	}


}



std::vector<u8> compress_osr_string(std::string_view string) {

	std::vector<u8> output;

	output.resize(std::max(1024, int(string.size() * 1.5f)));

	CLzmaEncProps p;
	LzmaEncProps_Init(&p);

	p.level = 0;

	size_t len{ output.size() - (5 + 8) };

	p.dictSize = string.size();

	*(u32*)(output.data() + 5) = string.size();

	SizeT propsSize = 5;

	LzmaEncode((u8*)output.data() + 5 + 8, &len, (u8*)string.data(), string.size(),
		&p, (u8*)output.data(), &propsSize, 0,
		0,
		&lzma_allocFuncs, &lzma_allocFuncs
	);

	output.resize(len + 5 + 8);

	return output;
}

void construct_screen_space(const _rrf_header* header, const _stream_map& stream_data , std::vector<_osr_frame>& R) {

	std::vector<int> delta_table[2];
	
	delta_table[0].resize(header->frame_count);
	delta_table[1].resize(header->frame_count);
	
	{

		const auto read_delta_stream = [](const bit_stream& stream, std::vector<int>& output) {

			size_t i{};
	
			for (auto& v : output)
				v = read_bucket(stream, i, SCREEN_SPACE_DELTA);
	
		};
	

		read_delta_stream(bit_stream{ stream_data[rrf_tag::screen_space_x_delta] }, delta_table[0]);
		read_delta_stream(bit_stream{ stream_data[rrf_tag::screen_space_y_delta] }, delta_table[1]);

	}
	
	{
	
		const bit_stream sign_stream{ stream_data[rrf_tag::screen_space_sign_sustain] };
	
		const auto read_sign_stream = [](const bit_stream& stream, std::vector<int>& output, size_t& i) {
	
			size_t sign{};
			int frame{ -1 };
	
			while (frame < int(output.size())) {
	
				const auto sustain{ 1 + read_bucket(stream, i, SCREEN_SPACE_SIGN_SUSTAIN) };
	
				if (sign) {
	
					for (size_t z{}; z < sustain; ++z) {
	
						output[frame] = -output[frame];
						++frame;
					}
	
				} else frame += sustain;
	
				sign = !sign;
	
			}
	
		};
	
		size_t i{};
		read_sign_stream(sign_stream, delta_table[0], i);
		read_sign_stream(sign_stream, delta_table[1], i);
	
	}
	
	int PREV[2]{};
	
	const auto& d{ stream_data[rrf_tag::screen_space_info] };

	const float screen_ratio{ std::max(d.size() == 4 ? *(float*)d.data() : 1.f, 1.f) };

	//const float RR{ 1.f / header->screen_ratio };
	
	for (size_t i{}; i < header->frame_count; ++i) {
		
		float* O = &R[i].x;
	
		for (size_t z{}; z < 2; ++z) {
	
			int v{ PREV[z] + delta_table[z][i] };
			PREV[z] = v;
	
			//O[z] = float(v) * RR;
			O[z] = float(v) / screen_ratio;
	
		}
	
	}

}


void construct_game_space(const _rrf_header* header, const _stream_map& stream_data, std::vector<_osr_frame>& R) {

	_game_space_info gsi{};

	{

		const auto& d{ stream_data[rrf_tag::game_space_info] };

		if (d.size() != sizeof(gsi))
			return;

		memcpy(&gsi, d.data(), sizeof(gsi));

	}

	std::vector<floatp> float_table{};
	// Construct float table
	{

		const auto key_frame_count{ header->frame_count - gsi.lowfi_count };

		float_table.resize(2 * key_frame_count);

		std::vector<u32> sustain_buffer;

		for (size_t fc{}; fc < 2; ++fc) {

			// Sign
			{

				const auto& raw{ stream_data[rrf_tag(u8(rrf_tag::game_space_float_x_sign) + fc)] };

				const auto count{ raw.size() / 4 };

				u32* data{ (u32*)raw.data() };

				size_t i{};

				bool current_sign{};

				for (size_t sc{}; sc < count; ++sc) {

					const auto v{ data[sc] };

					ON_SCOPE_EXIT(current_sign = !current_sign; i += v; );

					if (current_sign == 0)
						continue;

					for (size_t z{}; z < v; ++z)
						float_table[fc + 2 * (i + z)].p.sign = 1;
				}

			}

			extract_high_float(stream_data, fc, key_frame_count, gsi.exp_sustain_count[fc], float_table.data());

		}

	}

	std::vector<bool> lowf_sign[2];

	// Set up lowf sign table
	{

		bit_stream lowf_sign_stream{ stream_data[rrf_tag::game_space_lowf_sign] };

		const auto MAX_BIT{ lowf_sign_stream.size() };

		for (size_t i{}, bit_offset{}; i < 2; ++i) {

			lowf_sign[i].resize(gsi.lowfi_count);

			int COUNT{ -1 };
			for (bool flag{}; COUNT < (int)gsi.lowfi_count && bit_offset < MAX_BIT; ) {

				const auto sustain = 1 + read_bucket(lowf_sign_stream, bit_offset, 4, 4, 4, 8);

				if (flag) {
					for (size_t z{}; z < sustain; ++z)
						lowf_sign[i][COUNT + z] = 1;
				}

				COUNT += sustain;
				flag = !flag;
			}

		}
	}

	// Now we can start reconstructing the full stream.

	size_t lowfi_x_index{}, lowfi_y_index{ gsi.lowf_delta_y_start_bit };

	bit_stream lowf_delta_stream{ stream_data[rrf_tag::game_space_lowf_delta] };

	const float lowf_r{ 1.f / gsi.lowfi_resolution };

	int R_X{}, R_Y{};

	for (size_t i{}, lowf_i{}; i < header->frame_count; ++i) {

		auto& f{ R[i] };

		if (f.keys != 0) {

			const auto h_i{ 2 * (i - lowf_i) };

			R[i].x = float_table[h_i].f;
			R[i].y = float_table[h_i + 1].f;

			// A lot of useless rounds
			R_X = (int)::roundf(R[i].x * gsi.lowfi_resolution);
			R_Y = (int)::roundf(R[i].y * gsi.lowfi_resolution);

		}
		else {

			auto _x{ (int)read_bucket(lowf_delta_stream, lowfi_x_index, 4, 4, 4, 8) };
			auto _y{ (int)read_bucket(lowf_delta_stream, lowfi_y_index, 4, 4, 4, 8) };

			_x = lowf_sign[0][lowf_i] ? -_x : _x;
			_y = lowf_sign[1][lowf_i] ? -_y : _y;

			R[i].x = float(R_X += _x) * lowf_r;
			R[i].y = float(R_Y += _y) * lowf_r;

			++lowf_i;

		}

	}

}


void construct_ctb(const _rrf_header* header, const _stream_map& stream_data, std::vector<_osr_frame>& R) {

	const auto& exp_raw{ stream_data[rrf_tag::fruits_exp_sustain] };

	if (exp_raw.size() != 4)
		return;

	extract_high_float<4>(stream_data, 0, header->frame_count, *(u32*)exp_raw.data(), (floatp*)&R[0].x);

}

bool rrf_to_osr(const char* input_file, const char* output_file) {

	const auto& raw_bytes{ read_file(input_file) };

	const _rrf_header*const header{ (const _rrf_header*)raw_bytes.data() };

	_stream_map stream_data{};
	stream_data.data.resize(header->data_count);

	{

		const u8* const tags{ raw_bytes.data() + sizeof(_rrf_header) };
		const _rrf_data_block* const data_block{ (_rrf_data_block*)(tags + header->data_count) };

		u8* data_ptr{ (u8*)(data_block + header->data_count) };

		for (size_t i{}, size{ header->data_count }; i < size; ++i) {
						
			auto& o{ stream_data.data[i] };
			const auto& db{ data_block[i] };

			o.tag = tags[i];

			if (db.is_compressed == 0)
				add_to_u8(o.data, data_ptr, db.size);
			else
				rrf::lzma_decomp(o.data, data_ptr, db.size);

			data_ptr += db.size;

		}

	}

	std::vector<_osr_frame> R; R.resize(header->frame_count);

	// Delta Time
	{ 
	
		std::vector<int> delta_table{};
	
		{
			size_t bit_offset{};

			bit_stream bs{ stream_data[rrf_tag::time_delta_table] };

			delta_table.resize(small_read<5>(bs, bit_offset));
	
			for (auto& v : delta_table) v = small_read<5>(bs, bit_offset);
			for (auto& v : delta_table) v = small_read<1>(bs, bit_offset) ? -v : v;
	
		}

		{
			size_t bit_offset{};
			bit_stream bs{ stream_data[rrf_tag::time_delta_stream] };
			
			for (auto& v : R)
				v.delta = delta_table[read_bucket(bs, bit_offset, BUCKET_TIME_STREAM)];
		}
	
	}

	// Keys
	{
		size_t bit_offset{32};
	
		bit_stream bs{ stream_data[rrf_tag::key_bit_stream] };

		const u32 flags{ bs.size() >= 4 ? *(u32*)bs.data.data() : 0 };

		const auto read_key = [&](u32 flag_bits) {
	
			int COUNT{ -1 };
			for (bool flag{}; COUNT < (int)header->frame_count && bit_offset < bs.size(); ) {
	
				const auto sustain{
					1 + read_bucket(bs, bit_offset, BUCKET_COMP_KEYS)
				};
	
				if (flag) {
					for (size_t i{}; i < sustain; ++i)
						R[COUNT + i].keys |= flag_bits;
				}
	
				COUNT += sustain;
				flag = !flag;
			}
	
		};


		if (header->flags & (RRF_FLAG::gamemode_mania | RRF_FLAG::gamemode_taiko)) {

			for (size_t i{}; i < 31; ++i) {

				if (flags & (1 << i))
					read_key(1 << i);

			}

		} else {

			if (flags & (1 << 0)) read_key(1);
			if (flags & (1 << 1)) read_key(2);
			if (flags & (1 << 2)) read_key(1 | 4);
			if (flags & (1 << 3)) read_key(2 | 8);
			if (flags & (1 << 4)) read_key(16);

		}

	}
	if (header->flags & RRF_FLAG::gamemode_mania) {

		const float speed{ *(float*)stream_data[rrf_tag::mania_scroll_data].data() };

		for (auto& r : R) {
			r.x = float(r.keys >> 2);
			r.y = speed;
			r.keys &= 3;
		}

		R[0].x = 256.f; R[0].y = -500.f;

		R[1].x = 256.f; R[1].y = -500.f;

	}else if (header->flags & RRF_FLAG::gamemode_taiko) {
	} else if (header->flags & RRF_FLAG::gamemode_fruits) {
		construct_ctb(header, stream_data, R);
	}else
		((header->flags & RRF_FLAG::using_screenspace) ? construct_screen_space : construct_game_space)(header, stream_data, R);

	{

		size_t i{};
		std::string OUTPUT; OUTPUT.resize(4096);
	
		for (auto& r : R) {

			constexpr auto M_C{ 25 * 4 };
	
			if ((OUTPUT.size() - i) < M_C)
				OUTPUT.resize(OUTPUT.size() + 4096);
	
			i = std::to_chars(OUTPUT.data() + i, OUTPUT.data() + OUTPUT.size(), r.delta).ptr - OUTPUT.data();
			OUTPUT[i++] = '|';
	
			i = std::to_chars(OUTPUT.data() + i, OUTPUT.data() + OUTPUT.size(), r.x).ptr - OUTPUT.data();
			OUTPUT[i++] = '|';
	
			i = std::to_chars(OUTPUT.data() + i, OUTPUT.data() + OUTPUT.size(), r.y).ptr - OUTPUT.data();
			OUTPUT[i++] = '|';
	
			i = std::to_chars(OUTPUT.data() + i, OUTPUT.data() + OUTPUT.size(), r.keys).ptr - OUTPUT.data();
			OUTPUT[i++] = ',';
	
		}
	
		OUTPUT.resize(std::max(i, (size_t)1) - 1);
	
		{
			OUTPUT += ",-12345|0|0|0";
		}
	
		const auto& compressed_string{ compress_osr_string(OUTPUT) };
	
		std::vector<u8> final_file_output{};

		if (header->flags & RRF_FLAG::has_osr_header) {

			add_to_u8(final_file_output, stream_data[rrf_tag::osr_header]);

			add_to_u8(final_file_output, compressed_string.size());
		}
		
		add_to_u8(final_file_output, compressed_string);
	
		if (header->flags & RRF_FLAG::has_osr_header) {
			add_to_u8(final_file_output, 0);// score id
			add_to_u8(final_file_output, 0);
		}
	
		write_file(output_file, final_file_output);
	
	}

	return 1;
}