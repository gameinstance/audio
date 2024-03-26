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

#ifndef AUDIO_WAVE
#define AUDIO_WAVE

#include <cstdint>
#include <cstring>
#include <basics/error.hh>


/*******************************************************************************************************
 *
 * @name   WAVE codec
 *
 * @brief  A lightweight C++ WAVE stereo codec.
 *
 * The audio::wave::encoder class template encodes samples for a stream characterized by streaminfo.
 * The encode_header() member function encodes the streaminfo object and writes it to the ostream wave
 * audio stream. It must be called before any audio samples are encoded. The encode_sample() member
 * function encodes one sample into the audio stream and should be called mindful of the number and
 * order of channels.
 *
 */


namespace audio {
namespace wave {


struct streaminfo_type {
	uint32_t sample_rate;
	uint8_t sample_bit_size;
	uint8_t channel_count;
	uint64_t sample_count;
};

template<typename OUTPUT_STREAM>
class encoder {
public:
	explicit encoder(OUTPUT_STREAM &ostream);
	encoder(OUTPUT_STREAM &ostream, const streaminfo_type &streaminfo);
	~encoder();

	void encode_header(const streaminfo_type &streaminfo);
	void encode_sample(int32_t sample);

private:
	OUTPUT_STREAM &_ostream;
	streaminfo_type _streaminfo;

	void _put_string(const char *value);
	void _put_int32(int32_t value);
	void _put_int24(int32_t value);
	void _put_int16(int16_t value);
	void _put_int8(int8_t value);
};


/******************************************************************************************************/


template<typename OUTPUT_STREAM>
encoder<OUTPUT_STREAM>::encoder(OUTPUT_STREAM &ostream)
	: _ostream{ostream}, _streaminfo{}
{
}


template<typename OUTPUT_STREAM>
encoder<OUTPUT_STREAM>::encoder(OUTPUT_STREAM &ostream, const streaminfo_type &streaminfo)
	: _ostream{ostream}, _streaminfo{}
{
	encode_header(streaminfo);
}


template<typename OUTPUT_STREAM>
encoder<OUTPUT_STREAM>::~encoder()
{
	_ostream.flush();
}


template<typename OUTPUT_STREAM>
void encoder<OUTPUT_STREAM>::encode_header(const streaminfo_type &info)
{
	_put_string("RIFF");
	auto data_size = info.channel_count * info.sample_count * info.sample_bit_size;
	_put_int32(4 + 8 + 16 + 8 + data_size);
	_put_string("WAVE");

	_put_string("fmt ");
	_put_int32(16);

	_put_int16(1);
	_put_int16(info.channel_count);
	_put_int32(info.sample_rate);

	auto frame_size = info.sample_bit_size / 8 * info.channel_count;
	auto byte_rate = frame_size * info.sample_rate;
	_put_int32(byte_rate);
	_put_int16(frame_size);
	_put_int16(info.sample_bit_size); // Bits per sample

	_put_string("data");
	_put_int32(data_size);

	_streaminfo = info;
}


template<typename OUTPUT_STREAM>
void encoder<OUTPUT_STREAM>::encode_sample(int32_t sample)
{
	switch (_streaminfo.sample_bit_size) {
		case 8:
			_put_int8(sample);
			break;
		case 16:
			_put_int16(sample);
			break;
		case 24:
			_put_int24(sample);
			break;
		case 32:
			_put_int32(sample);
			break;
		default:
			throw basics::error{"wave::encoder: (protocol error) unexpected sample bit size (%zu)",
																		_streaminfo.sample_bit_size};
	}
}


template<typename OUTPUT_STREAM>
void encoder<OUTPUT_STREAM>::_put_string(const char *value)
{
	for (size_t i = 0; i < std::strlen(value); ++i)
		_ostream.put(value[i]);
}


template<typename OUTPUT_STREAM>
void encoder<OUTPUT_STREAM>::_put_int32(int32_t value)
{
	_ostream.put((value >>  0) & 0xff);
	_ostream.put((value >>  8) & 0xff);
	_ostream.put((value >> 16) & 0xff);
	_ostream.put((value >> 24) & 0xff);
}


template<typename OUTPUT_STREAM>
void encoder<OUTPUT_STREAM>::_put_int24(int32_t value)
{
	_ostream.put((value >>  0) & 0xff);
	_ostream.put((value >>  8) & 0xff);
	_ostream.put((value >> 16) & 0xff);
}


template<typename OUTPUT_STREAM>
void encoder<OUTPUT_STREAM>::_put_int16(int16_t value)
{
	_ostream.put((value >> 0) & 0xff);
	_ostream.put((value >> 8) & 0xff);
}


template<typename OUTPUT_STREAM>
void encoder<OUTPUT_STREAM>::_put_int8(int8_t value)
{
	_ostream.put((value >> 0) & 0xff);
}


} // namespace wave
} // namespace audio


#endif // AUDIO_WAVE
