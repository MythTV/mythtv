/*
 * hdhomerun_discover.h
 *
 * Copyright © 2006-2007 Silicondust Engineering Ltd. <www.silicondust.com>.
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
#ifdef __cplusplus
extern "C" {
#endif

struct hdhomerun_discover_device_t {
	uint32_t ip_addr;
	uint32_t device_type;
	uint32_t device_id;
};

/*
 * Find devices.
 *
 * The device information is stored in caller-supplied array of hdhomerun_discover_device_t vars.
 * Multiple attempts are made to find devices.
 * Execution time is 1 second.
 *
 * Set target_ip to zero to auto-detect IP address.
 *
 * Returns the number of devices found.
 * Retruns -1 on error.
 */
extern LIBTYPE int hdhomerun_discover_find_devices_custom(uint32_t target_ip, uint32_t device_type, uint32_t device_id, struct hdhomerun_discover_device_t result_list[], int max_count);

/*
 * Verify that the device ID given is valid.
 *
 * The device ID contains a self-check sequence that detects common user input errors including
 * single-digit errors and two digit transposition errors.
 *
 * Returns TRUE if valid.
 * Returns FALSE if not valid.
 */
extern LIBTYPE bool_t hdhomerun_discover_validate_device_id(uint32_t device_id);

#ifdef __cplusplus
}
#endif
