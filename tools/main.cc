/* Copyright (C) 2024  Bogdan-Gabriel Alecu  (GameInstance.com)
 *
 * FLAC decoder - part of audio package
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
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

#include <basics/error.hh>
#include <basics/file.hh>
#include "flac.hh"
#include "wave.hh"

using namespace basics;
using namespace audio;


void print_info(const flac::streaminfo_type &/*info*/);


int main(int argc, char *argv[])
{
	try {
		if (argc < 3)
			throw error{"Usage: %s <input.flac> <output.wav>", argv[0]};

		auto file_istream = file::input{argv[1]};
		auto flac_istream = flac::decoder{file_istream};
		auto file_ostream = file::output{argv[2], true};
		auto wave_ostream = wave::encoder{file_ostream};

		flac_istream.decode_marker();
		while (flac_istream.state() != flac::decoder_state::has_metadata)
			flac_istream.decode_metadata();

		auto info = flac_istream.streaminfo();
		print_info(info);
		wave_ostream.encode_header(wave::streaminfo_type{info.sample_rate, info.sample_bit_size,
															   info.channel_count, info.sample_count});

		while (flac_istream.state() != flac::decoder_state::complete) {
			flac_istream.decode_audio();
			if (flac_istream.block_sample_rate() != info.sample_rate)
				throw error{"variable sample rate not supported"};

			for (auto i = size_t{0}; i < flac_istream.block_size(); ++i)
				for (auto channel_idx = uint8_t{0}; channel_idx < info.channel_count; ++channel_idx)
					wave_ostream.encode_sample(flac_istream.block_data()[channel_idx][i]);
		}
	} catch (const error &err) {
		err.dump();

		return 1;
	}

	return 0;
}


void print_info(const audio::flac::streaminfo_type &info)
{
	printf("flac stream info:\n");
	printf("* min_block_size=%u\n", info.min_block_size);
	printf("* max_block_size=%u\n", info.max_block_size);
	printf("* min_frame_size=%u\n", info.min_frame_size);
	printf("* max_frame_size=%u\n", info.max_frame_size);
	printf("* sample_rate=%u\n", info.sample_rate);
	printf("* channel_count=%u\n", info.channel_count);
	printf("* sample_bit_size=%u\n", info.sample_bit_size);
	printf("* sample_count=%lu\n", info.sample_count);
}
