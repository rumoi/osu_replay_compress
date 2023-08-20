#pragma once

#include <vector>
#include <array>
#include <bitset>
#include <stdint.h>
#include <math.h> // for roundf
#include <fstream>
#include <charconv>


#ifndef __RRF_SHARED_H
#define __RRF_SHARED_H

#define BUCKET_COMP_KEYS 8,16
#define SCREEN_SPACE_DELTA 4, 4, 4, 8
#define SCREEN_SPACE_SIGN_SUSTAIN 8

#define EXP_ICHUNK_CONSUME 1,2,2
#define EXP_ICHUNK_REPEAT 2,2,4

#define BUCKET_TIME_STREAM 4,4,4,4

namespace DIAG {

	size_t INPUT_SIZE{}, OUTPUT_SIZE{};

}

#define _7Z_NO_METHOD_LZMA2

#include "LzmaEnc.h"
#include "LzmaDec.h"
#include "7zTypes.h"

static void* _lzmaAlloc(ISzAllocPtr, size_t size) {
	return new uint8_t[size];
}
static void _lzmaFree(ISzAllocPtr, void* addr) {
	if (!addr)
		return;

	delete[] reinterpret_cast<uint8_t*>(addr);
}

static ISzAlloc lzma_allocFuncs = {
		_lzmaAlloc, _lzmaFree
};

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#ifndef ON_SCOPE_EXIT

template <typename F> struct on_scope_exit {
private: F func;
public:
	on_scope_exit(const on_scope_exit&) = delete;
	on_scope_exit& operator=(const on_scope_exit&) = delete;

	constexpr on_scope_exit(F&& f) : func(std::forward<F>(f)) {}
	constexpr ~on_scope_exit() { func(); }
};

#define PCAT0(x, y) PCAT1(x, y)
#define PCAT1(x, y) PCAT2(!, x ## y)
#define PCAT2(x, r) r
#define ON_SCOPE_EXIT(...) on_scope_exit PCAT0(__scope, __LINE__) {[&]{__VA_ARGS__}}

#endif

template<typename T>
bool write_file(const char* file_name, const T& data) {

	std::ofstream file(file_name, std::ios::binary);

	if (!file.is_open())return 0;

	file.write((const char*)data.data(), data.size());

	file.close();
	return 1;
}
std::vector<u8> read_file(const char* file_name) {

	std::ifstream file(file_name, std::ios::binary | std::ios::ate | std::ios::in);

	if (file.is_open() == 0) [[unlikely]]
		return {};

	const size_t file_size{ (size_t)file.tellg() };

	std::vector<u8> ret(file_size);

	if (file_size) [[likely]] {
		file.seekg(0, std::ios::beg);
		file.read((char*)ret.data(), file_size);
	}
	file.close();

	return ret;
}

struct bit_stream {

	std::vector<u8> data;
	u8 in_count{};

	void clear() {
		in_count = 0;
		data.clear();
	}

	size_t max_size() const {
		return data.size() * 8;
	}

	size_t size() const {

		size_t r{ in_count };

		if (data.size() > 0)
			r += (data.size() - 1) * 8;

		return r;
	}

	void push_back(const bool v) {

		if (in_count > 7 || data.size() == 0) {
			in_count = 0;
			data.push_back({});
		}

		data.back() |= u8(v) << (in_count++);

	}

	bool operator[](size_t i) const {

		const auto [bucket, index] {ldiv(i, 8)};

		return (data[bucket] >> index) & 1;

	}


	void append(const bit_stream& v) {

		for (size_t i{}, size{ v.size() }; i < size; ++i)
			push_back(v[i]);

	}

};

template<size_t BIT_WRITE>
void small_write(u32 value, bit_stream& output) {

	constexpr auto MAX_VALUE{ (1 << BIT_WRITE) - 1 };

	while (value >= MAX_VALUE) {

		for (size_t i{}; i < BIT_WRITE; ++i)
			output.push_back(1);

		const auto in{ 31 - std::countl_zero(value) };

		// Only have to store the values under the most signifitcant bit, because the next bit is always 1.

		for (size_t i{ BIT_WRITE - 1 }; i < in; ++i)
			output.push_back(1);

		output.push_back(0);

		for (size_t i{}; i < in; ++i)
			output.push_back((value >> i) & 1);

		return;
	}

	for (size_t i{}; i < BIT_WRITE; ++i)
		output.push_back((value >> i) & 1);

}

template<size_t BIT_WRITE>
u32 small_read(const bit_stream& input, size_t& i) {

	if (i >= input.max_size())
		return 0;

	if constexpr (BIT_WRITE == 1)
		return input[i++];

	constexpr auto MAX_VALUE{ (1 << BIT_WRITE) - 1 };

	u32 value{}, c{};

	for (size_t size{ input.max_size() }; c < BIT_WRITE && (i + c) < size; ++c)
		value |= u32(input[i + c]) << c;

	i += c;
	c = 0;

	if (value != MAX_VALUE)
		return value;

	int bit_count{ BIT_WRITE - 1 };

	for (size_t size{ input.max_size() }; (i + c) < size; ++c)
		if (input[i + c] == 0)
			break;

	bit_count += c;
	i += c + 1;
	c = 0;
	value = 1 << bit_count;
	for (size_t size{ input.max_size() }; c < bit_count && (i + c) < size; ++c)
		value |= u32(input[i + c]) << c;

	i += bit_count;

	return value;

}

template<u32 OVERFLOW_BITS = 5, bool DEBUG = 0, typename ...T >
void write_bucket(u32 value, bit_stream& output, const T... bits) {

	bool done{};

	const auto write = [&](const size_t b) {

		if (b == 0 || done)
			return 0;

		const auto MAX_VALUE{ (1 << b) - 1 };

		if (value < MAX_VALUE) {

			for (size_t i{}; i < b; ++i)
				output.push_back((value >> i) & 1);

			done = 1;
			return 1;
		}

		for (size_t i{ 1 }; i <= b; ++i)
			output.push_back(1);

		value -= MAX_VALUE;
		return 0;
	};

	((write(bits)), ...);

	if (done)
		return;

	if constexpr (DEBUG)
		printf("WB> OVERFLOW %i\n", value);

	value += 1;

	const auto in{ (size_t)31 - std::countl_zero(value) };

	for (size_t i{}; i < OVERFLOW_BITS; ++i)
		output.push_back((in >> i) & 1);

	for (size_t i{}; i < in; ++i)
		output.push_back((value >> i) & 1);



}

template<u32 OVERFLOW_BITS = 5, typename ...T >
u32 read_bucket(const bit_stream& input, size_t& i, const T... bits) {

	u32 result{};

	const auto max_size{ input.max_size() };

	bool done{};

	const auto read = [&](const size_t b) {

		if (b == 0 || done)
			return 0;

		u32 v{};

		ON_SCOPE_EXIT(result += v; );

		for (size_t z{}; z < b && i < max_size; ++z) {
			v |= u32(input[i++] != 0) << z;
		}

		if (v != (1 << b) - 1) {
			done = 1;
			return 1;
		}

		return 0;
	};

	((read(bits)), ...);

	if (done)
		return result;

	u32 bit_count{};

	for (size_t z{}; z < OVERFLOW_BITS && i < max_size; ++z)
		bit_count |= u32(input[i++] != 0) << z;

	u32 v{ (u32)1 << bit_count };

	for (size_t z{}; z < bit_count && i < max_size; ++z)
		v |= u32(input[i++] != 0) << z;

	return result + v - 1;

}

struct _data_chunk {
	u32 size : 31, is_compressed : 1;
};

enum RRF_FLAG {

	has_osr_header = 1 << 0,
	force_lossless = 1 << 1, // Only affects raw input replays
	using_screenspace = 1 << 2,

	//exp23_check = force_lossless | mantissa_all_23,
};


struct _rrf_header {

	u32 rrf_version : 4, flags : 28;
	u32 frame_count;

	union {
		struct {
			u32 lowfi_count, lowf_delta_y_start_bit;
		};
		struct {
			float screen_ratio;
		};
		
	};

	_data_chunk time_table;
	_data_chunk time_stream;

	_data_chunk composite_key;

	union {

		struct {

			_data_chunk screen_space_sign_sustain;
			_data_chunk screen_space[2];

		};

		struct {

			struct {

				_data_chunk sign;

				struct {

					u32 chunk_count : 31, sustain_4bits : 1;

					_data_chunk absolute_table;
					_data_chunk sustain;
					_data_chunk stream;

				} exponent;

				_data_chunk mantissa;

			} high_float[2];

			_data_chunk lowf_sign;
			_data_chunk lowf_delta;

		};

	};

	void print_sizes() const {

		printf("rrf_header: %i\n", sizeof(*this));

		#define DO(x)printf(#x ": %i| %i\n", x.is_compressed, x.size)

		DO(time_table);
		DO(time_stream);
		DO(composite_key);


		if (flags & RRF_FLAG::using_screenspace) {
			printf("screen_ratio: %f\n", screen_ratio);
			DO(screen_space_sign_sustain);
			DO(screen_space[0]);
			DO(screen_space[1]);
		}
		else {

			DO(high_float[0].sign);
			DO(high_float[0].exponent.absolute_table);
			DO(high_float[0].exponent.sustain);
			DO(high_float[0].exponent.stream);
			DO(high_float[0].mantissa);

			DO(high_float[1].sign);
			DO(high_float[1].exponent.absolute_table);
			DO(high_float[1].exponent.sustain);
			DO(high_float[1].exponent.stream);
			DO(high_float[1].mantissa);


			DO(lowf_sign);
			DO(lowf_delta);
		}

		#undef DO

	}

};

struct _osr_frame {
	int delta;
	float x, y;
	u32 keys;
};

using _osr = std::vector<_osr_frame>;

typedef union {
	float f;
	struct {
		u32 mantissa : 23, exponent : 8, sign : 1;
	} p;
} floatp;

#endif