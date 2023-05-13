#pragma once

#include "rrf_shared.h"
#include <charconv>

void extract_exponent_stream(const u32 key_frame_count, floatp* output,
	const u8* absolute_table,
	const bit_stream& stream_bit,
	const bit_stream& sustain_bit,
	std::vector<u32>& sustain,
	const bool is_4) {

	{

		size_t bit_offset{};

		u32 bc[2]{ 5,6 };

		if (is_4) {
			bc[0] = 4;
			bc[1] = 5;
		}

		for (auto& v : sustain)
			v = read_bucket(sustain_bit, bit_offset, bc[0], bc[1]);

	}

	size_t bit_offset{};
	u8 last_exponent{};
	size_t exp_i{};

	const auto write_exponent = [&](u8 exp, u32 count) {

		for (size_t i{}; i <= count; ++i)
			output[2 * (exp_i++)].p.exponent = exp;

		last_exponent = exp;

	};

	write_exponent(absolute_table[small_read<3>(stream_bit, bit_offset)], 0);

	bool double_sign{};
	bool IMPL_SIGN{};

	for (size_t sustain_i{}; exp_i < key_frame_count; ) {

		if (small_read<1>(stream_bit, bit_offset) == 1) {

			const auto index{ small_read<3>(stream_bit, bit_offset) };

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

std::vector<u8> compress_osr_string(std::string_view string) {

	std::vector<u8> output;

	output.resize(std::max(1024, int(string.size() * 1.5f)));

	CLzmaEncProps p;
	LzmaEncProps_Init(&p);

	p.level = 0;

	size_t len{ output.size() - (5 + 8) };

	p.dictSize = string.size();

	*(u32*)(output.data() + 5 + 4) = string.size();

	SizeT propsSize = 5;

	LzmaEncode((u8*)output.data() + 5 + 8, &len, (u8*)string.data(), string.size(),
		&p, (u8*)output.data(), &propsSize, 0,
		0,
		&lzma_allocFuncs, &lzma_allocFuncs
	);

	output.resize(len + 5 + 8);

	return output;
}

bool rrf_to_osr(const char* input_file, const char* output_file) {

	const auto& raw_bytes{ read_file(input_file) };

	const _rrf_header* header{ (const _rrf_header*)raw_bytes.data() };

	u8* data{ (u8*)raw_bytes.data() + sizeof(_rrf_header) };
	
	std::vector<_osr_frame> R; R.resize(header->frame_count);

	const auto lzma_decomp = [](std::vector<u8>& ret, const u8* data, size_t size) {

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

	const auto read_chunk = [&](const _data_chunk dc) {

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

	// Delta
	{ 

		std::vector<int> delta_table{};

		{
			size_t bit_offset{};
			bit_stream bs{ std::move(read_chunk(header->time_table)) };

			delta_table.resize(small_read<5>(bs, bit_offset));

			for (auto& v : delta_table)
				v = std::bit_cast<int>(small_read<5>(bs, bit_offset));

		}

		size_t bit_offset{};
		bit_stream bs{};

		bs.data = std::move(read_chunk(header->time_stream));

		for (auto& v : R)
			v.delta = delta_table[read_bucket(bs, bit_offset, BUCKET_TIME_STREAM)];

	}

	// Keys
	{
		size_t bit_offset{};

		bit_stream bs{ std::move(read_chunk(header->composite_key)) };

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

		read_key(1 | 4);
		read_key(2 | 8);
		read_key(1);
		read_key(2);
		read_key(16);

	}

	std::vector<floatp> float_table;
	// Construct float table
	{

		const auto key_frame_count{ header->frame_count - header->lowfi_count };

		float_table.resize(2 * key_frame_count);

		std::vector<u32> sustain_buffer;

		for (size_t fc{}; fc < 2; ++fc) {

			// Sign
			{
				
				const auto& raw{ read_chunk(header->high_float[fc].sign) };

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

			// Exponent
			{

				u8 absolute_table[256];

				{
					bit_stream table_stream{ std::move(read_chunk(header->high_float[fc].exponent.absolute_table)) };

					u8* v{ absolute_table };

					size_t i{};

					*(v++) = read_bucket<3>(table_stream, i, 4) ^ 128;

					for (;;) {

						const auto is_negative{ table_stream[i++] };

						const auto delta{ read_bucket<3>(table_stream, i, 2) };

						if (delta == 0)
							break;

						const auto prev{ *(v - 1) };

						*(v++) = is_negative ? (prev - delta) : (prev + delta);


					}

				}

				sustain_buffer.resize(header->high_float[fc].exponent.chunk_count);

				bit_stream sustain_bit{}, stream_bit{};

				sustain_bit.data = std::move(read_chunk(header->high_float[fc].exponent.sustain));
				stream_bit.data = std::move(read_chunk(header->high_float[fc].exponent.stream));


				extract_exponent_stream(key_frame_count, float_table.data() + fc,
					absolute_table, stream_bit, sustain_bit,
					sustain_buffer, header->high_float[fc].exponent.sustain_4bits
				);

			}

			// Mantissa
			{

				const auto mantissa_buffer{ read_chunk(header->high_float[fc].mantissa) };

				floatp* o{ float_table.data() + fc };
				const u8* d{ mantissa_buffer.data() };

				bit_stream d_buff{};

				constexpr size_t start[]{ 0, 16, 20, 23 };

				for (size_t mc{}; mc < 3; ++mc) {

					const auto size{ *(u32*)d };

					lzma_decomp(d_buff.data, d + 4, size);

					size_t di{};
					for (size_t i{}; i < key_frame_count; ++i) {

						u32 mantissa{};

						for (size_t z{ start[mc] }; z < start[mc + 1]; ++z)
							mantissa |= (u32(d_buff[di++]) << z);

						float_table[fc + 2 * i].p.mantissa |= mantissa;

					}

					d += size + 4;
					d_buff.clear();

				}

			}

		}

	}
	
	std::vector<bool> lowf_sign[2];

	// Set up lowf sign table
	{

		bit_stream lowf_sign_stream{};
		
		lowf_sign_stream.data = std::move(read_chunk(header->lowf_sign));

		const auto MAX_BIT{ lowf_sign_stream.size() };

		for (size_t i{}, bit_offset{}; i < 2; ++i) {

			lowf_sign[i].resize(header->lowfi_count);

			int COUNT{ -1 };
			for (bool flag{}; COUNT < (int)header->lowfi_count && bit_offset < MAX_BIT; ) {

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

	size_t lowfi_x_index{}, lowfi_y_index{ header->lowf_delta_y_start_bit };

	bit_stream lowf_delta_stream{};

	lowf_delta_stream.data = std::move(read_chunk(header->lowf_delta));

	int R_X{}, R_Y{};

	for (size_t i{}, lowf_i{}; i < header->frame_count; ++i) {

		auto& f{ R[i] };

		if (f.keys != 0) {

			const auto h_i{ 2 * (i - lowf_i) };

			R[i].x = float_table[h_i].f;
			R[i].y = float_table[h_i + 1].f;

			// A lot of useless rounds
			R_X = (int)::roundf(R[i].x);
			R_Y = (int)::roundf(R[i].y);

		} else {

			auto _x{ (int)read_bucket(lowf_delta_stream, lowfi_x_index, 4, 4, 4, 8) };
			auto _y{ (int)read_bucket(lowf_delta_stream, lowfi_y_index, 4, 4, 4, 8) };

			_x = lowf_sign[0][lowf_i] ? -_x : _x;
			_y = lowf_sign[1][lowf_i] ? -_y : _y;

			R[i].x = (float)(R_X += _x);
			R[i].y = (float)(R_Y += _y);

			++lowf_i;


		}

	}

	{

		size_t i{};
		std::string OUTPUT; OUTPUT.resize(4096);

		for (auto r : R) {

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

		write_file(output_file, compress_osr_string(OUTPUT));

	}

	return 1;
}