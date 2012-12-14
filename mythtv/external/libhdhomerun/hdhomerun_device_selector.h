/*
 * hdhomerun_device_selector.h
 *
 * Copyright © 2009 Silicondust USA Inc. <www.silicondust.com>.
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

/*
 * Create a device selector object for use with dynamic tuner allocation support.
 * All tuners registered with a specific device selector instance must have the same signal source.
 * The dbg parameter may be null.
 */
extern LIBTYPE struct hdhomerun_device_selector_t *hdhomerun_device_selector_create(struct hdhomerun_debug_t *dbg);
extern LIBTYPE void hdhomerun_device_selector_destroy(struct hdhomerun_device_selector_t *hds, bool_t destroy_devices);

/*
 * Get the number of devices in the list.
 */
extern LIBTYPE int hdhomerun_device_selector_get_device_count(struct hdhomerun_device_selector_t *hds);

/*
 * Populate device selector with devices from given source.
 * Returns the number of devices populated.
 */
extern LIBTYPE int hdhomerun_device_selector_load_from_file(struct hdhomerun_device_selector_t *hds, char *filename);
#if defined(__WINDOWS__)
extern LIBTYPE int hdhomerun_device_selector_load_from_windows_registry(struct hdhomerun_device_selector_t *hds, wchar_t *wsource);
#endif

/*
 * Add/remove a device from the selector list.
 */
extern LIBTYPE void hdhomerun_device_selector_add_device(struct hdhomerun_device_selector_t *hds, struct hdhomerun_device_t *hd);
extern LIBTYPE void hdhomerun_device_selector_remove_device(struct hdhomerun_device_selector_t *hds, struct hdhomerun_device_t *hd);

/*
 * Find a device in the selector list.
 */
extern LIBTYPE struct hdhomerun_device_t *hdhomerun_device_selector_find_device(struct hdhomerun_device_selector_t *hds, uint32_t device_id, unsigned int tuner_index);

/*
 * Select and lock an available device.
 * If not null, preference will be given to the prefered device specified.
 * The device resource lock must be released by the application when no longer needed by
 * calling hdhomerun_device_tuner_lockkey_release().
 *
 * Recommended channel change logic:
 *
 * Start (inactive -> active):
 * - Call hdhomerun_device_selector_choose_and_lock() to choose and lock an available tuner.
 * 
 * Stop (active -> inactive):
 * - Call hdhomerun_device_tuner_lockkey_release() to release the resource lock and allow the tuner
 *   to be allocated by other computers.
 *
 * Channel change (active -> active):
 * - If the new channel has a different signal source then call hdhomerun_device_tuner_lockkey_release()
 *   to release the lock on the tuner playing the previous channel, then call
 *   hdhomerun_device_selector_choose_and_lock() to choose and lock an available tuner.
 * - If the new channel has the same signal source then call hdhomerun_device_tuner_lockkey_request()
 *   to refresh the lock. If this function succeeds then the same device can be used. If this fucntion fails
 *   then call hdhomerun_device_selector_choose_and_lock() to choose and lock an available tuner.
 */
extern LIBTYPE struct hdhomerun_device_t *hdhomerun_device_selector_choose_and_lock(struct hdhomerun_device_selector_t *hds, struct hdhomerun_device_t *prefered);

#ifdef __cplusplus
}
#endif
