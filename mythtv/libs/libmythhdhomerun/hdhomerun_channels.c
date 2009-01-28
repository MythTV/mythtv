/*
 * hdhomerun_channelscan.c
 *
 * Copyright © 2007-2008 Silicondust Engineering Ltd. <www.silicondust.com>.
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

#include "hdhomerun.h"

#define FREQUENCY_RESOLUTION 62500

struct hdhomerun_channel_entry_t {
	struct hdhomerun_channel_entry_t *next;
	struct hdhomerun_channel_entry_t *prev;
	uint32_t channel_map;
	uint32_t frequency;
	uint8_t channel_number;
	char name[16];
};

struct hdhomerun_channel_map_range_t {
	uint32_t channel_map;
	uint8_t channel_range_start;
	uint8_t channel_range_end;
	uint32_t frequency;
	uint32_t spacing;
};

static const struct hdhomerun_channel_map_range_t hdhomerun_channels_map_ranges[] = {
	{CHANNEL_MAP_US_BCAST,   2,   4,  57000000, 6000000},
	{CHANNEL_MAP_US_BCAST,   5,   6,  79000000, 6000000},
	{CHANNEL_MAP_US_BCAST,   7,  13, 177000000, 6000000},
	{CHANNEL_MAP_US_BCAST,  14,  69, 473000000, 6000000},

	{CHANNEL_MAP_US_CABLE,   1,   1,  75000000, 6000000},
	{CHANNEL_MAP_US_CABLE,   2,   4,  57000000, 6000000},
	{CHANNEL_MAP_US_CABLE,   5,   6,  79000000, 6000000},
	{CHANNEL_MAP_US_CABLE,   7,  13, 177000000, 6000000},
	{CHANNEL_MAP_US_CABLE,  14,  22, 123000000, 6000000},
	{CHANNEL_MAP_US_CABLE,  23,  94, 219000000, 6000000},
	{CHANNEL_MAP_US_CABLE,  95,  99,  93000000, 6000000},
	{CHANNEL_MAP_US_CABLE, 100, 136, 651000000, 6000000},

	{CHANNEL_MAP_US_HRC,     1,   1,  73753600, 6000300},
	{CHANNEL_MAP_US_HRC,     2,   4,  55752700, 6000300},
	{CHANNEL_MAP_US_HRC,     5,   6,  79753900, 6000300},
	{CHANNEL_MAP_US_HRC,     7,  13, 175758700, 6000300},
	{CHANNEL_MAP_US_HRC,    14,  22, 121756000, 6000300},
	{CHANNEL_MAP_US_HRC,    23,  94, 217760800, 6000300},
	{CHANNEL_MAP_US_HRC,    95,  99,  91754500, 6000300},
	{CHANNEL_MAP_US_HRC,   100, 136, 649782400, 6000300},

	{CHANNEL_MAP_US_IRC,     1,   1,  75012500, 6000000},
	{CHANNEL_MAP_US_IRC,     2,   4,  57012500, 6000000},
	{CHANNEL_MAP_US_IRC,     5,   6,  81012500, 6000000},
	{CHANNEL_MAP_US_IRC,     7,  13, 177012500, 6000000},
	{CHANNEL_MAP_US_IRC,    14,  22, 123012500, 6000000},
	{CHANNEL_MAP_US_IRC,    23,  41, 219012500, 6000000},
	{CHANNEL_MAP_US_IRC,    42,  42, 333025000, 6000000},
	{CHANNEL_MAP_US_IRC,    43,  94, 339012500, 6000000},
	{CHANNEL_MAP_US_IRC,    95,  97,  93012500, 6000000},
	{CHANNEL_MAP_US_IRC,    98,  99, 111025000, 6000000},
	{CHANNEL_MAP_US_IRC,   100, 136, 651012500, 6000000},

	{CHANNEL_MAP_UK_BCAST,  21,  68, 474000000, 8000000},
	{CHANNEL_MAP_NZ_BCAST,  25,  62, 506000000, 8000000},

	{0,                      0,   0,         0,       0}
};

static struct hdhomerun_channel_entry_t *hdhomerun_channel_list_head = NULL;
static struct hdhomerun_channel_entry_t *hdhomerun_channel_list_tail = NULL;

uint32_t hdhomerun_channel_entry_channel_map(struct hdhomerun_channel_entry_t *entry)
{
	return entry->channel_map;
}

uint8_t hdhomerun_channel_entry_channel_number(struct hdhomerun_channel_entry_t *entry)
{
	return entry->channel_number;
}

uint32_t hdhomerun_channel_entry_frequency(struct hdhomerun_channel_entry_t *entry)
{
	return entry->frequency;
}

const char *hdhomerun_channel_entry_name(struct hdhomerun_channel_entry_t *entry)
{
	return entry->name;
}

static const char *hdhomerun_channel_map_name(uint32_t channel_map)
{
	switch (channel_map) {
	case CHANNEL_MAP_US_BCAST:
		return "us-bcast";
	case CHANNEL_MAP_US_CABLE:
		return "us-cable";
	case CHANNEL_MAP_US_HRC:
		return "us-hrc";
	case CHANNEL_MAP_US_IRC:
		return "us-irc";
	case CHANNEL_MAP_UK_BCAST:
		return "uk-bcast";
	case CHANNEL_MAP_NZ_BCAST:
		return "nz-bcast";
	default:
		return "unknown";
	}
}

static void hdhomerun_channel_list_build_insert(struct hdhomerun_channel_entry_t *entry)
{
	struct hdhomerun_channel_entry_t *prev = NULL;
	struct hdhomerun_channel_entry_t *next = hdhomerun_channel_list_head;

	while (next) {
		if (next->frequency > entry->frequency) {
			break;
		}

		prev = next;
		next = next->next;
	}

	entry->prev = prev;
	entry->next = next;

	if (prev) {
		prev->next = entry;
	} else {
		hdhomerun_channel_list_head = entry;
	}

	if (next) {
		next->prev = entry;
	} else {
		hdhomerun_channel_list_tail = entry;
	}
}

static void hdhomerun_channel_list_build_range(const struct hdhomerun_channel_map_range_t *range)
{
	uint8_t channel_number;
	for (channel_number = range->channel_range_start; channel_number <= range->channel_range_end; channel_number++) {
		struct hdhomerun_channel_entry_t *entry = (struct hdhomerun_channel_entry_t *)calloc(1, sizeof(struct hdhomerun_channel_entry_t));
		if (!entry) {
			return;
		}

		entry->channel_map = range->channel_map;
		entry->channel_number = channel_number;
		entry->frequency = range->frequency + ((uint32_t)(channel_number - range->channel_range_start) * range->spacing);
		entry->frequency = (entry->frequency / FREQUENCY_RESOLUTION) * FREQUENCY_RESOLUTION;
		sprintf(entry->name, "%s:%u", hdhomerun_channel_map_name(entry->channel_map), entry->channel_number);

		hdhomerun_channel_list_build_insert(entry);
	}
}

static void hdhomerun_channel_list_build(void)
{
	const struct hdhomerun_channel_map_range_t *range = hdhomerun_channels_map_ranges;
	while (range->channel_map) {
		hdhomerun_channel_list_build_range(range);
		range++;
	}
}

struct hdhomerun_channel_entry_t *hdhomerun_channel_list_first(uint32_t channel_map)
{
	if (!hdhomerun_channel_list_head) {
		hdhomerun_channel_list_build();
	}

	struct hdhomerun_channel_entry_t *entry = hdhomerun_channel_list_head;
	while (entry) {
		if (entry->channel_map & channel_map) {
			return entry;
		}

		entry = entry->next;
	}

	return NULL;
}

struct hdhomerun_channel_entry_t *hdhomerun_channel_list_last(uint32_t channel_map)
{
	if (!hdhomerun_channel_list_head) {
		hdhomerun_channel_list_build();
	}

	struct hdhomerun_channel_entry_t *entry = hdhomerun_channel_list_tail;
	while (entry) {
		if (entry->channel_map & channel_map) {
			return entry;
		}

		entry = entry->prev;
	}

	return NULL;
}

struct hdhomerun_channel_entry_t *hdhomerun_channel_list_next(uint32_t channel_map, struct hdhomerun_channel_entry_t *entry)
{
	entry = entry->next;
	while (entry) {
		if (entry->channel_map & channel_map) {
			return entry;
		}

		entry = entry->next;
	}

	return NULL;
}

struct hdhomerun_channel_entry_t *hdhomerun_channel_list_prev(uint32_t channel_map, struct hdhomerun_channel_entry_t *entry)
{
	entry = entry->prev;
	while (entry) {
		if (entry->channel_map & channel_map) {
			return entry;
		}

		entry = entry->prev;
	}

	return NULL;
}

uint32_t hdhomerun_channel_list_total_count(uint32_t channel_map)
{
	uint32_t count = 0;

	struct hdhomerun_channel_entry_t *entry = hdhomerun_channel_list_first(channel_map);
	while (entry) {
		count++;
		entry = hdhomerun_channel_list_next(channel_map, entry);
	}

	return count;
}

uint32_t hdhomerun_channel_list_frequency_count(uint32_t channel_map)
{
	uint32_t count = 0;
	uint32_t last_frequency = 0;

	struct hdhomerun_channel_entry_t *entry = hdhomerun_channel_list_first(channel_map);
	while (entry) {
		if (entry->frequency != last_frequency) {
			last_frequency = entry->frequency;
			count++;
		}

		entry = hdhomerun_channel_list_next(channel_map, entry);
	}

	return count;
}

uint32_t hdhomerun_channel_number_to_frequency(uint32_t channel_map, uint8_t channel_number)
{
	struct hdhomerun_channel_entry_t *entry = hdhomerun_channel_list_first(channel_map);
	while (entry) {
		if (entry->channel_number == channel_number) {
			return entry->frequency;
		}

		entry = hdhomerun_channel_list_next(channel_map, entry);
	}

	return 0;
}

uint8_t hdhomerun_channel_frequency_to_number(uint32_t channel_map, uint32_t frequency)
{
	frequency = (frequency / FREQUENCY_RESOLUTION) * FREQUENCY_RESOLUTION;

	struct hdhomerun_channel_entry_t *entry = hdhomerun_channel_list_first(channel_map);
	while (entry) {
		if (entry->frequency == frequency) {
			return entry->channel_number;
		}
		if (entry->frequency > frequency) {
			return 0;
		}

		entry = hdhomerun_channel_list_next(channel_map, entry);
	}

	return 0;
}
