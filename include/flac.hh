/* Copyright (C) 2024  Bogdan-Gabriel Alecu  (GameInstance.com)
 *
 * audio - C++ audio codecs.
 *
 * This library is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef AUDIO_FLAC
#define AUDIO_FLAC

#include <vector>
#include <basics/error.hh>
#include <stream/bit.hh>


/*******************************************************************************************************
 *
 * @name  FLAC codec
 *
 * @brief A comprehensible, lightweight C++ FLAC codec.
 *
 * The audio::flac::decoder class template reads byte frames from istream and decodes them to metadata
 * or audio blocks. The state() member function returns *init* after decoder construction, *has_marker*
 * after the call to decode_marker(), *has_metadata* after reading all metadata entries by successive
 * calls to decode_metadata() and *complete* after all audio blocks are decoded by repeated calls to
 * decode_audio(). The streaminfo() member function returns a reference to the flac stream information
 * member if decoder state is either *has_metadata* or *complete*. No more than block_size() samples can
 * be extracted from the member buffer pointed to by block_data() after each call to decode_audio().
 *
 */


namespace audio {
namespace flac {


using buffer_sample_type = int64_t;
using audio_data = std::vector<std::vector<buffer_sample_type>>;

struct streaminfo_type {  // 208 bytes
	uint16_t min_block_size;
	uint16_t max_block_size;
	uint32_t min_frame_size;
	uint32_t max_frame_size;
	uint32_t sample_rate;
	uint8_t channel_count;
	uint8_t sample_bit_size;
	uint64_t sample_count;
	// byte md5_signature[16];
};

enum class decoder_state {
	init,
	has_marker,
	has_metadata,
	complete,
};

static const size_t max_channel_count = 2;


template<typename INPUT_STREAM, size_t BUFFER_SIZE = 8192>
class decoder {
public:
	using state_type = decoder_state;

	explicit decoder(INPUT_STREAM &istream);

	void decode_marker();
	void decode_metadata();
	void decode_audio();

	inline const decoder_state &state() const;
	inline const streaminfo_type &streaminfo() const;
	inline const uint32_t &block_sample_rate() const;
	inline const audio_data &block_data() const;
	inline const uint16_t &block_size() const;

private:
	inline void _decode_subframe(uint8_t sample_bit_size);
	inline void _decode_subframe_fixed(uint8_t order, uint8_t sample_bit_size);
	inline void _decode_subframe_lpc(uint8_t order, uint8_t sample_bit_size);
	inline void _decode_residuals(uint8_t order);
	inline void _restore_linear_prediction(const int16_t *coefficients, uint8_t order, uint8_t shift);
	inline int64_t _get_rice_int(size_t bit_count);
	inline uint16_t _get_block_size(uint8_t flags_4bit);
	inline uint32_t _get_sample_rate(uint8_t flags_4bit);
	inline uint8_t _get_sample_bit_size(uint8_t flags_3bit);

	stream::bit::input<INPUT_STREAM> _istream;
	state_type _state;
	streaminfo_type _streaminfo;
	uint64_t _sample_count;
	uint16_t _block_size;
	uint32_t _block_sample_rate;
	uint8_t _channel_idx;
	uint64_t _frame_count;
	int16_t _coefficients[32];
	audio_data _buffer;
};


/******************************************************************************************************/


static constexpr const char *_decoder_name = "audio::flac::decoder";
static constexpr int16_t _fixed_prediction_coefficients[5][4] = {
	{},
	{1},
	{2, -1},
	{3, -3, 1},
	{4, -6, 4, -1},
};


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
decoder<INPUT_STREAM, BUFFER_SIZE>::decoder(INPUT_STREAM &upstream)
	: _istream{upstream}, _state{state_type::init}, _streaminfo{},
	  _sample_count{0}, _block_size{0}, _block_sample_rate{0}, _channel_idx{0},
	  _frame_count{}, _coefficients{},
	  _buffer(max_channel_count, std::vector<buffer_sample_type>(BUFFER_SIZE, 0))
{
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
void decoder<INPUT_STREAM, BUFFER_SIZE>::decode_marker()
{
	if (_istream.get_uint(32) != 0x664c6143)
		throw basics::error{"%s: (protocol error) unexpected marker", _decoder_name};

	_state = state_type::has_marker;
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
void decoder<INPUT_STREAM, BUFFER_SIZE>::decode_metadata()
{
	// METADATA_BLOCK_HEADER <32>
	if (_istream.get_uint(1) == 1)
		_state = state_type::has_metadata;

	const auto metadata_type_id = _istream.get_uint(7);
	auto metadata_byte_size = _istream.get_uint(24);

	// METADATA_BLOCK_DATA
	if (metadata_type_id == 0) {  // STREAMINFO
		_streaminfo.min_block_size  = _istream.get_uint(16);
		_streaminfo.max_block_size  = _istream.get_uint(16);
		_streaminfo.min_frame_size  = _istream.get_uint(24);
		_streaminfo.max_frame_size  = _istream.get_uint(24);
		_streaminfo.sample_rate     = _istream.get_uint(20);
		_streaminfo.channel_count   = _istream.get_uint(3) + 1;
		_streaminfo.sample_bit_size = _istream.get_uint(5) + 1;
		_streaminfo.sample_count    = _istream.get_uint(36);

		if (_streaminfo.channel_count > max_channel_count)
			throw basics::error{"%s: (assertion failed) expecting maximum %zu channels; got %u",
										_decoder_name, max_channel_count, _streaminfo.channel_count};
		if (_streaminfo.max_block_size > BUFFER_SIZE)
			throw basics::error{"%s: (assertion failed) expecting maximum %zu samples/block; got %u",
											_decoder_name, BUFFER_SIZE, _streaminfo.max_block_size};

		for (auto i = uint8_t{0}; i < 16; ++i)
			_istream.get_byte();
	} else {  // OTHER METADATA BLOCKS
		for (; metadata_byte_size > 0; --metadata_byte_size)
			_istream.get_byte();
	}
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
void decoder<INPUT_STREAM, BUFFER_SIZE>::decode_audio()
{   // O(N)
	if (_istream.eos()) {
		_state = state_type::complete;

		return;
	}

	// FRAME_HEADER
	const auto sync_code = _istream.get_uint(14);
	if (sync_code != 0b11111111111110)
		throw basics::error{"%s: (protocol error) unexpected frame sync code; got 0x%x, expecting 0x%x",
															_decoder_name, sync_code, 0b11111111111110};

	if (_istream.get_uint(1) != 0)
		throw basics::error{"%s: (protocol error) unexpected frame reserved bit #1", _decoder_name};

	/*const auto block_strategy_bitset = (uint8_t)*/_istream.get_uint(1);
	const auto block_size_bitset         = (uint8_t)_istream.get_uint(4);
	const auto sample_rate_bitset        = (uint8_t)_istream.get_uint(4);
	const auto channel_assignment_bitset = (uint8_t)_istream.get_uint(4);
	const auto sample_bit_size_bitset    = (uint8_t)_istream.get_uint(3);
	if (_istream.get_uint(1) != 0)
		throw basics::error{"%s: (protocol error) unexpected frame reserved bit #2", _decoder_name};

	const auto extra_byte_len = stream::bit::countl_zero<uint8_t>(~(uint8_t)_istream.get_uint(8)) - 1;
	for (int i = 0; i < extra_byte_len; ++i)
		_istream.get_uint(8);

	_block_size = _get_block_size(block_size_bitset);
	_block_sample_rate = _get_sample_rate(sample_rate_bitset);
	const auto sample_bit_size = _get_sample_bit_size(sample_bit_size_bitset);

	_istream.get_uint(8);  // CRC-8 polynomial

	// SUBFRAME+
	if (channel_assignment_bitset < 8) {
		for (_channel_idx = 0; _channel_idx < _streaminfo.channel_count; ++_channel_idx) {
			_buffer[_channel_idx].resize(_block_size);

			_decode_subframe(sample_bit_size);
		}
	} else if (channel_assignment_bitset < 11) {
		_channel_idx = 0;
		_buffer[_channel_idx].resize(_block_size);
		_decode_subframe(sample_bit_size + ((channel_assignment_bitset == 9) ? 1 : 0));

		_channel_idx = 1;
		_buffer[_channel_idx].resize(_block_size);
		_decode_subframe(sample_bit_size + ((channel_assignment_bitset == 9) ? 0 : 1));

		if (channel_assignment_bitset == 8) {
			for (uint16_t i = 0; i < _block_size; ++i)
				_buffer[1][i] = _buffer[0][i] - _buffer[1][i];
		} else if (channel_assignment_bitset == 9) {
			for (uint16_t i = 0; i < _block_size; ++i)
				_buffer[0][i] += _buffer[1][i];
		} else if (channel_assignment_bitset == 10) {
			buffer_sample_type side{};
			buffer_sample_type right{};
			for (uint16_t i = 0; i < _block_size; ++i) {
				side = _buffer[1][i];
				right = (buffer_sample_type)_buffer[0][i] - (side >> 1);
				_buffer[1][i] = right;
				_buffer[0][i] = right + side;
			}
		}
	} else
		throw basics::error{"%s: (assertion failed) unsupported channel assignment (%u)",
															_decoder_name, channel_assignment_bitset};

	_sample_count += _block_size;
	++_frame_count;

	_istream.align();  // zero padding to byte alignment

	// FRAME FOOTER
	_istream.get_uint(16);  // CRC-16 polynomial
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline const decoder_state &decoder<INPUT_STREAM, BUFFER_SIZE>::state() const
{
	return _state;
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline const streaminfo_type &decoder<INPUT_STREAM, BUFFER_SIZE>::streaminfo() const
{
	return _streaminfo;
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline const audio_data &decoder<INPUT_STREAM, BUFFER_SIZE>::block_data() const
{
	return _buffer;
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline const uint16_t &decoder<INPUT_STREAM, BUFFER_SIZE>::block_size() const
{
	return _block_size;
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline const uint32_t &decoder<INPUT_STREAM, BUFFER_SIZE>::block_sample_rate() const
{
	return _block_sample_rate;
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline void decoder<INPUT_STREAM, BUFFER_SIZE>::_decode_subframe(uint8_t sample_bit_size)
{
	//  SUBFRAME_HEADER
	_istream.get_uint(1); // zero padding (NOT ENFORCED)

	auto subframe_type = (uint8_t)_istream.get_uint(6);

	auto wasted_bits = uint8_t{0};
	if (_istream.get_uint(1) == 1) {
		for (;;) {
			if (_istream.get_uint(1) == 1)
				break;
			++wasted_bits;
		}
	}
	sample_bit_size -= wasted_bits;

	// SUBFRAME DATA
	if (subframe_type == 0) {  // SUBFRAME_CONSTANT: O(N)
		fill_n(_buffer[_channel_idx].begin(), _buffer[_channel_idx].size(),
																	_istream.get_int(sample_bit_size));
	} else if (subframe_type == 1) {  // SUBFRAME_VERBATIM: O(N)
		for (uint16_t i = 0; i < _buffer[_channel_idx].size(); ++i)
			_buffer[_channel_idx][i] = _istream.get_int(sample_bit_size);
	} else if (subframe_type < 8) {
		throw basics::error{"%s: (protocol error) reserved subframe type 1(%u)",
																		_decoder_name, subframe_type};
	} else if (subframe_type < 13) {  // SUBFRAME_FIXED
		_decode_subframe_fixed(subframe_type - 8, sample_bit_size);
	} else if (subframe_type < 32) {
		throw basics::error{"%s: (protocol error) reserved subframe type 2(%u)",
																		_decoder_name, subframe_type};
	} else {  // SUBFRAME_LPC
		_decode_subframe_lpc(subframe_type - 31, sample_bit_size);
	}

	if (wasted_bits > 0)
		for (auto sample: _buffer[_channel_idx])
			sample <<= wasted_bits;
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline void decoder<INPUT_STREAM, BUFFER_SIZE>::_decode_subframe_fixed(uint8_t order,
																				uint8_t sample_bit_size)
{
	for (uint8_t i = 0; i < order; ++i)
		_buffer[_channel_idx][i] = _istream.get_int(sample_bit_size);

	_decode_residuals(order);
	_restore_linear_prediction(_fixed_prediction_coefficients[order], order, 0);
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline void decoder<INPUT_STREAM, BUFFER_SIZE>::_decode_subframe_lpc(uint8_t order,
																				uint8_t sample_bit_size)
{
	for (uint8_t i = 0; i < order; ++i)
		_buffer[_channel_idx][i] = _istream.get_int(sample_bit_size);

	auto precision = (uint8_t)_istream.get_uint(4) + 1;
	auto shift =      (int8_t)_istream.get_int(5);
	for (int i = 0; i < order; ++i)
		_coefficients[i] = _istream.get_int(precision);

	_decode_residuals(order);
	_restore_linear_prediction(_coefficients, order, shift);
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline void decoder<INPUT_STREAM, BUFFER_SIZE>::_decode_residuals(uint8_t order)
{  // O(N)
	auto coding_method = (uint8_t)_istream.get_uint(2);
	if (coding_method > 1)
		throw basics::error{"%s: (protocol error) reserved residual coding method (%u)",
																		_decoder_name, coding_method};

	auto partition_order = (uint8_t)_istream.get_uint(4);
	auto partition_count = (uint16_t)1 << partition_order;

	auto parameter_bit_size = (uint8_t)(coding_method == 0) ? 4 : 5;
	auto escape_code = (uint8_t)(coding_method == 0) ? 0xF : 0x1F;

	if (_buffer[_channel_idx].size() % partition_count != 0)
		throw basics::error{"%s: (protocol error) invalid partition count vs. block size "
					"(%u %% %u != 0)", _decoder_name, _buffer[_channel_idx].size(), partition_count};

	uint16_t partition_size = _buffer[_channel_idx].size() / partition_count;

	for (uint16_t i = 0; i < partition_count; ++i) {
		auto start = (uint16_t)(i * partition_size + ((i == 0) ? order : 0));
		auto end = (uint16_t)((i + 1) * partition_size);

		auto param = (uint8_t)_istream.get_uint(parameter_bit_size);
		if (param < escape_code) {
			for (auto j = start; j < end; ++j)
				_buffer[_channel_idx][j] = _get_rice_int(param);
		} else {
			auto bit_count = (uint8_t)_istream.get_uint(5);
			for (auto j = start; j < end; ++j)
				_buffer[_channel_idx][j] = _istream.get_int(bit_count);
		}
	}
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline void decoder<INPUT_STREAM, BUFFER_SIZE>::_restore_linear_prediction(const int16_t *coefficients,
																		uint8_t order, uint8_t shift)
{  // O(N*order)
	for (uint16_t i = order; i < _buffer[_channel_idx].size(); ++i) {
		int64_t sum{0};
		for (uint16_t j = 0; j < order; ++j)
			sum += _buffer[_channel_idx][i - 1 - j] * coefficients[j];

		_buffer[_channel_idx][i] += (sum >> shift);
	}
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline int64_t decoder<INPUT_STREAM, BUFFER_SIZE>::_get_rice_int(size_t bit_count)
{
	uint64_t uval{0};
	while (_istream.get_uint(1) == 0)
		++uval;

	uval = (uval << bit_count) | _istream.get_uint(bit_count);
	int64_t res = (uval & 1)? -((int64_t)(uval >> 1)) - 1 : (int)(uval >> 1);
	return res;
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline uint16_t decoder<INPUT_STREAM, BUFFER_SIZE>::_get_block_size(uint8_t flags_4bit)
{
	if (flags_4bit == 1)                       return 192;
	if ((flags_4bit > 1) && (flags_4bit < 6))  return 144 * (1 << flags_4bit);
	if (flags_4bit == 6)                       return _istream.get_uint(8) + 1;
	if (flags_4bit == 7)                       return _istream.get_uint(16) + 1;
	if ((flags_4bit > 7) && (flags_4bit < 16)) return 256 * (1 << (flags_4bit - 8));
	// if (flags_4bit == 0)
	throw basics::error{"%s: (protocol error) unexpected block size bits (reserved)", _decoder_name};
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline uint32_t decoder<INPUT_STREAM, BUFFER_SIZE>::_get_sample_rate(uint8_t flags_4bit)
{
	if (flags_4bit ==  0) return _streaminfo.sample_rate;
	if (flags_4bit ==  1) return  88200;
	if (flags_4bit ==  2) return 176400;
	if (flags_4bit ==  3) return 192000;
	if (flags_4bit ==  4) return   8000;
	if (flags_4bit ==  5) return  16000;
	if (flags_4bit ==  6) return  22050;
	if (flags_4bit ==  7) return  24000;
	if (flags_4bit ==  8) return  32000;
	if (flags_4bit ==  9) return  44100;
	if (flags_4bit == 10) return  48000;
	if (flags_4bit == 11) return  96000;
	if (flags_4bit == 12) return _istream.get_uint(8) * 1000;
	if (flags_4bit == 13) return _istream.get_uint(16);
	if (flags_4bit == 14) return _istream.get_uint(16) * 10;
	// if (flags_4bit == 15)
	throw basics::error{"%s: (protocol error) unexpected block size bits (reserved)", _decoder_name};
}


template<typename INPUT_STREAM, size_t BUFFER_SIZE>
inline uint8_t decoder<INPUT_STREAM, BUFFER_SIZE>::_get_sample_bit_size(uint8_t flags_3bit)
{
	if (flags_3bit == 0) return _streaminfo.sample_bit_size;
	if (flags_3bit == 1) return  8;
	if (flags_3bit == 2) return 12;
	if (flags_3bit == 4) return 16;
	if (flags_3bit == 5) return 20;
	if (flags_3bit == 6) return 24;
	if (flags_3bit == 7) return 32;
	// if (flags_3bit == 3)
	throw basics::error{"%s: (protocol error) unexpected sample bit size (reserved)", _decoder_name};
}


} // namespace flac
} // namespace audio


#endif // AUDIO_FLAC
