/*
 * hdhomerun_channelscan.h
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

#define HDHOMERUN_CHANNELSCAN_PROGRAM_NORMAL 0
#define HDHOMERUN_CHANNELSCAN_PROGRAM_NODATA 1
#define HDHOMERUN_CHANNELSCAN_PROGRAM_CONTROL 2
#define HDHOMERUN_CHANNELSCAN_PROGRAM_ENCRYPTED 3

#define HDHOMERUN_CHANNELSCAN_OPTION_REVERSE (1 << 0)
#define HDHOMERUN_CHANNELSCAN_OPTION_REFINE_CHANNEL_MAP (1 << 1)

struct hdhomerun_channelscan_t;

extern LIBTYPE struct hdhomerun_channelscan_t *channelscan_create(struct hdhomerun_device_t *hd, uint32_t channel_map, uint32_t options);
extern LIBTYPE void channelscan_destroy(struct hdhomerun_channelscan_t *scan);

extern LIBTYPE int channelscan_advance(struct hdhomerun_channelscan_t *scan, struct hdhomerun_channelscan_result_t *result);
extern LIBTYPE int channelscan_detect(struct hdhomerun_channelscan_t *scan, struct hdhomerun_channelscan_result_t *result);
extern LIBTYPE uint8_t channelscan_get_progress(struct hdhomerun_channelscan_t *scan);
