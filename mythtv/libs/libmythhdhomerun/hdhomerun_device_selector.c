/*
 * hdhomerun_device_selector.c
 *
 * Copyright © 2009-2010 Silicondust USA Inc. <www.silicondust.com>.
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

#include "hdhomerun.h"

struct hdhomerun_device_selector_t {
	struct hdhomerun_device_t **hd_list;
	size_t hd_count;
	struct hdhomerun_debug_t *dbg;
};

struct hdhomerun_device_selector_t *hdhomerun_device_selector_create(struct hdhomerun_debug_t *dbg)
{
	struct hdhomerun_device_selector_t *hds = (struct hdhomerun_device_selector_t *)calloc(1, sizeof(struct hdhomerun_device_selector_t));
	if (!hds) {
		hdhomerun_debug_printf(dbg, "hdhomerun_device_selector_create: failed to allocate selector object\n");
		return NULL;
	}

	hds->dbg = dbg;

	return hds;
}

void hdhomerun_device_selector_destroy(struct hdhomerun_device_selector_t *hds, bool_t destroy_devices)
{
	if (destroy_devices) {
		size_t index;
		for (index = 0; index < hds->hd_count; index++) {
			struct hdhomerun_device_t *entry = hds->hd_list[index];
			hdhomerun_device_destroy(entry);
		}
	}

	if (hds->hd_list) {
		free(hds->hd_list);
	}

	free(hds);
}

LIBTYPE int hdhomerun_device_selector_get_device_count(struct hdhomerun_device_selector_t *hds)
{
	return (int)hds->hd_count;
}

void hdhomerun_device_selector_add_device(struct hdhomerun_device_selector_t *hds, struct hdhomerun_device_t *hd)
{
	size_t index;
	for (index = 0; index < hds->hd_count; index++) {
		struct hdhomerun_device_t *entry = hds->hd_list[index];
		if (entry == hd) {
			return;
		}
	}

	hds->hd_list = (struct hdhomerun_device_t **)realloc(hds->hd_list, (hds->hd_count + 1) * sizeof(struct hdhomerun_device_selector_t *));
	if (!hds->hd_list) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_add_device: failed to allocate device list\n");
		return;
	}

	hds->hd_list[hds->hd_count++] = hd;
}

void hdhomerun_device_selector_remove_device(struct hdhomerun_device_selector_t *hds, struct hdhomerun_device_t *hd)
{
	size_t index = 0;
	while (1) {
		if (index >= hds->hd_count) {
			return;
		}

		struct hdhomerun_device_t *entry = hds->hd_list[index];
		if (entry == hd) {
			break;
		}

		index++;
	}

	while (index + 1 < hds->hd_count) {
		hds->hd_list[index] = hds->hd_list[index + 1];
		index++;
	}

	hds->hd_list[index] = NULL;
	hds->hd_count--;
}

struct hdhomerun_device_t *hdhomerun_device_selector_find_device(struct hdhomerun_device_selector_t *hds, uint32_t device_id, unsigned int tuner_index)
{
	size_t index;
	for (index = 0; index < hds->hd_count; index++) {
		struct hdhomerun_device_t *entry = hds->hd_list[index];
		if (hdhomerun_device_get_device_id(entry) != device_id) {
			continue;
		}
		if (hdhomerun_device_get_tuner(entry) != tuner_index) {
			continue;
		}
		return entry;
	}

	return NULL;
}

int hdhomerun_device_selector_load_from_file(struct hdhomerun_device_selector_t *hds, char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		return 0;
	}

	while(1) {
		char device_name[32];
		if (!fgets(device_name, sizeof(device_name), fp)) {
			break;
		}

		struct hdhomerun_device_t *hd = hdhomerun_device_create_from_str(device_name, hds->dbg);
		if (!hd) {
			continue;
		}

		hdhomerun_device_selector_add_device(hds, hd);
	}

	fclose(fp);
	return (int)hds->hd_count;
}

#if defined(__WINDOWS__)
int hdhomerun_device_selector_load_from_windows_registry(struct hdhomerun_device_selector_t *hds, wchar_t *wsource)
{
	HKEY tuners_key;
	LONG ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Silicondust\\HDHomeRun\\Tuners", 0, KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS, &tuners_key);
	if (ret != ERROR_SUCCESS) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_load_from_windows_registry: failed to open tuners registry key (%ld)\n", (long)ret);
		return 0;
	}

	DWORD index = 0;
	while (1) {
		/* Next tuner device. */
		wchar_t wdevice_name[32];
		DWORD size = sizeof(wdevice_name);
		ret = RegEnumKeyEx(tuners_key, index++, wdevice_name, &size, NULL, NULL, NULL, NULL);
		if (ret != ERROR_SUCCESS) {
			break;
		}

		/* Check device configuation. */
		HKEY device_key;
		ret = RegOpenKeyEx(tuners_key, wdevice_name, 0, KEY_QUERY_VALUE, &device_key);
		if (ret != ERROR_SUCCESS) {
			hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_load_from_windows_registry: failed to open registry key for %S (%ld)\n", wdevice_name, (long)ret);
			continue;
		}

		wchar_t wsource_test[32];
		size = sizeof(wsource_test);
		if (RegQueryValueEx(device_key, L"Source", NULL, NULL, (LPBYTE)&wsource_test, &size) != ERROR_SUCCESS) {
			wsprintf(wsource_test, L"Unknown");
		}

		RegCloseKey(device_key);

		if (_wcsicmp(wsource_test, wsource) != 0) {
			continue;
		}

		/* Create and add device. */
		char device_name[32];
		sprintf(device_name, "%S", wdevice_name);

		struct hdhomerun_device_t *hd = hdhomerun_device_create_from_str(device_name, hds->dbg);
		if (!hd) {
			hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_load_from_windows_registry: invalid device name '%s' / failed to create device object\n", device_name);
			continue;
		}

		hdhomerun_device_selector_add_device(hds, hd);
	}

	RegCloseKey(tuners_key);
	return (int)hds->hd_count;
}
#endif

static bool_t hdhomerun_device_selector_choose_test(struct hdhomerun_device_selector_t *hds, struct hdhomerun_device_t *test_hd)
{
	const char *name = hdhomerun_device_get_name(test_hd);

	/*
	 * Attempt to aquire lock.
	 */
	char *error;
	int ret = hdhomerun_device_tuner_lockkey_request(test_hd, &error);
	if (ret > 0) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s chosen\n", name);
		return TRUE;
	}
	if (ret < 0) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s communication error\n", name);
		return FALSE;
	}

	/*
	 * In use - check target.
	 */
	char *target;
	ret = hdhomerun_device_get_tuner_target(test_hd, &target);
	if (ret < 0) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s communication error\n", name);
		return FALSE;
	}
	if (ret == 0) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s in use, failed to read target\n", name);
		return FALSE;
	}

	char *ptr = strstr(target, "//");
	if (ptr) {
		target = ptr + 2;
	}
	ptr = strchr(target, ' ');
	if (ptr) {
		*ptr = 0;
	}

	unsigned long a[4];
	unsigned long target_port;
	if (sscanf(target, "%lu.%lu.%lu.%lu:%lu", &a[0], &a[1], &a[2], &a[3], &target_port) != 5) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s in use, no target set (%s)\n", name, target);
		return FALSE;
	}

	uint32_t target_ip = (uint32_t)((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | (a[3] << 0));
	uint32_t local_ip = hdhomerun_device_get_local_machine_addr(test_hd);
	if (target_ip != local_ip) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s in use by %s\n", name, target);
		return FALSE;
	}

	/*
	 * Test local port.
	 */
	hdhomerun_sock_t test_sock = hdhomerun_sock_create_udp();
	if (test_sock == HDHOMERUN_SOCK_INVALID) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s in use, failed to create test sock\n", name);
		return FALSE;
	}

	bool_t inuse = (hdhomerun_sock_bind(test_sock, INADDR_ANY, (uint16_t)target_port, FALSE) == FALSE);
	hdhomerun_sock_destroy(test_sock);

	if (inuse) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s in use by local machine\n", name);
		return FALSE;
	}

	/*
	 * Dead local target, force clear lock.
	 */
	ret = hdhomerun_device_tuner_lockkey_force(test_hd);
	if (ret < 0) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s communication error\n", name);
		return FALSE;
	}
	if (ret == 0) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s in use by local machine, dead target, failed to force release lockkey\n", name);
		return FALSE;
	}

	hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s in use by local machine, dead target, lockkey force successful\n", name);

	/*
	 * Attempt to aquire lock.
	 */
	ret = hdhomerun_device_tuner_lockkey_request(test_hd, &error);
	if (ret > 0) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s chosen\n", name);
		return TRUE;
	}
	if (ret < 0) {
		hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s communication error\n", name);
		return FALSE;
	}

	hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_test: device %s still in use after lockkey force (%s)\n", name, error);
	return FALSE;
}

struct hdhomerun_device_t *hdhomerun_device_selector_choose_and_lock(struct hdhomerun_device_selector_t *hds, struct hdhomerun_device_t *prefered)
{
	/* Test prefered device first. */
	if (prefered) {
		if (hdhomerun_device_selector_choose_test(hds, prefered)) {
			return prefered;
		}
	}

	/* Test other tuners. */
	size_t index;
	for (index = 0; index < hds->hd_count; index++) {
		struct hdhomerun_device_t *entry = hds->hd_list[index];
		if (entry == prefered) {
			continue;
		}

		if (hdhomerun_device_selector_choose_test(hds, entry)) {
			return entry;
		}
	}

	hdhomerun_debug_printf(hds->dbg, "hdhomerun_device_selector_choose_and_lock: no devices available\n");
	return NULL;
}
