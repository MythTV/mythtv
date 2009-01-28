/*
 * hdhomerun_types.h
 *
 * Copyright © 2008 Silicondust Engineering Ltd. <www.silicondust.com>.
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

struct hdhomerun_device_t;

struct hdhomerun_tuner_status_t {
	char channel[32];
	char lock_str[32];
	bool_t signal_present;
	bool_t lock_supported;
	bool_t lock_unsupported;
	unsigned int signal_strength;
	unsigned int signal_to_noise_quality;
	unsigned int symbol_error_quality;
	uint32_t raw_bits_per_second;
	uint32_t packets_per_second;
};

struct hdhomerun_channelscan_program_t {
	char program_str[64];
	uint16_t program_number;
	uint16_t virtual_major;
	uint16_t virtual_minor;
	uint16_t type;
	char name[8];
};

#define HDHOMERUN_CHANNELSCAN_MAX_PROGRAM_COUNT 64

struct hdhomerun_channelscan_result_t {
	char channel_str[64];
	uint32_t channel_map;
	uint32_t frequency;
	struct hdhomerun_tuner_status_t status;
	int program_count;
	struct hdhomerun_channelscan_program_t programs[HDHOMERUN_CHANNELSCAN_MAX_PROGRAM_COUNT];
};

struct hdhomerun_plotsample_t {
	int16_t real;
	int16_t imag;
};
