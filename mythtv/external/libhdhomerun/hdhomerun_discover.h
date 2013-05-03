/*
 * hdhomerun_discover.h
 *
 * Copyright © 2006-2007 Silicondust USA Inc. <www.silicondust.com>.
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
 * 
 * As a special exception to the GNU Lesser General Public License,
 * you may link, statically or dynamically, an application with a
 * publicly distributed version of the Library to produce an
 * executable file containing portions of the Library, and
 * distribute that executable file under terms of your choice,
 * without any of the additional requirements listed in clause 4 of
 * the GNU Lesser General Public License.
 * 
 * By "a publicly distributed version of the Library", we mean
 * either the unmodified Library as distributed by Silicondust, or a
 * modified version of the Library that is distributed under the
 * conditions defined in the GNU Lesser General Public License.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct hdhomerun_discover_device_t {
	uint32_t ip_addr;
	uint32_t device_type;
	uint32_t device_id;
	uint8_t tuner_count;
};

/*
 * Find devices.
 *
 * The device information is stored in caller-supplied array of hdhomerun_discover_device_t vars.
 * Multiple attempts are made to find devices.
 * Execution time is typically 400ms if max_count is not reached.
 *
 * Set target_ip to zero to auto-detect the IP address.
 * Set device_type to HDHOMERUN_DEVICE_TYPE_TUNER to detect HDHomeRun tuner devices.
 * Set device_id to HDHOMERUN_DEVICE_ID_WILDCARD to detect all device ids.
 *
 * Returns the number of devices found.
 * Retruns -1 on error.
 */
extern LIBTYPE int hdhomerun_discover_find_devices_custom(uint32_t target_ip, uint32_t device_type, uint32_t device_id, struct hdhomerun_discover_device_t result_list[], int max_count);

/*
 * Optional: persistent discover instance available for discover polling use.
 */
extern LIBTYPE struct hdhomerun_discover_t *hdhomerun_discover_create(struct hdhomerun_debug_t *dbg);
extern LIBTYPE void hdhomerun_discover_destroy(struct hdhomerun_discover_t *ds);
extern LIBTYPE int hdhomerun_discover_find_devices(struct hdhomerun_discover_t *ds, uint32_t target_ip, uint32_t device_type, uint32_t device_id, struct hdhomerun_discover_device_t result_list[], int max_count);

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

/*
 * Detect if an IP address is multicast.
 *
 * Returns TRUE if multicast.
 * Returns FALSE if zero, unicast, expermental, or broadcast.
 */
extern LIBTYPE bool_t hdhomerun_discover_is_ip_multicast(uint32_t ip_addr);

#ifdef __cplusplus
}
#endif
