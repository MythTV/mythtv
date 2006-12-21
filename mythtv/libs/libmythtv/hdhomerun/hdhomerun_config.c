/*
 * hdhomerun_config.c
 *
 * Copyright © 2006 Silicondust Engineering Ltd. <www.silicondust.com>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "hdhomerun.h"

static const char *appname;

struct hdhomerun_device_t *hd;

static int help(void)
{
	printf("Usage:\n");
	printf("\t%s discover\n", appname);
	printf("\t%s <id> get help\n", appname);
	printf("\t%s <id> get <item>\n", appname);
	printf("\t%s <id> set <item> <value>\n", appname);
	printf("\t%s <id> scan <tuner> <starting channel>\n", appname);
	printf("\t%s <id> upgrade <filename>\n", appname);
	return -1;
}

static void extract_appname(const char *argv0)
{
	const char *ptr = strrchr(argv0, '/');
	if (ptr) {
		argv0 = ptr + 1;
	}
	ptr = strrchr(argv0, '\\');
	if (ptr) {
		argv0 = ptr + 1;
	}
	appname = argv0;
}

static bool_t contains(const char *arg, const char *cmpstr)
{
	if (strcmp(arg, cmpstr) == 0) {
		return TRUE;
	}

	if (*arg++ != '-') {
		return FALSE;
	}
	if (*arg++ != '-') {
		return FALSE;
	}
	if (strcmp(arg, cmpstr) == 0) {
		return TRUE;
	}

	return FALSE;
}

static int discover_print(void)
{
	struct hdhomerun_discover_device_t result_list[64];
	int count = hdhomerun_discover_find_devices(HDHOMERUN_DEVICE_TYPE_TUNER, result_list, 64);
	if (count < 0) {
		fprintf(stderr, "error sending discover request\n");
		return -1;
	}
	if (count == 0) {
		printf("no devices found\n");
		return 0;
	}

	int index;
	for (index = 0; index < count; index++) {
		struct hdhomerun_discover_device_t *result = &result_list[index];
		printf("hdhomerun device %08lX found at %u.%u.%u.%u\n",
			(unsigned long)result->device_id,
			(unsigned int)(result->ip_addr >> 24) & 0x0FF, (unsigned int)(result->ip_addr >> 16) & 0x0FF,
			(unsigned int)(result->ip_addr >> 8) & 0x0FF, (unsigned int)(result->ip_addr >> 0) & 0x0FF
		);
	}

	return count;
}

static bool_t parse_device_id_str(const char *s, uint32_t *pdevice_id, uint32_t *pdevice_ip)
{
	unsigned long a[4];
	if (sscanf(s, "%lu.%lu.%lu.%lu", &a[0], &a[1], &a[2], &a[3]) == 4) {
		*pdevice_id = HDHOMERUN_DEVICE_ID_WILDCARD;
		*pdevice_ip = (uint32_t)((a[0] << 24) | (a[1] << 16) | (a[2] << 8) | (a[3] << 0));
		return TRUE;
	}

	unsigned long device_id_raw;
	if (sscanf(s, "%lx", &device_id_raw) != 1) {
		fprintf(stderr, "invalid device id: %s\n", s);
		return FALSE;
	}

	uint32_t device_id = (uint32_t)device_id_raw;
	if (!hdhomerun_discover_validate_device_id(device_id)) {
		fprintf(stderr, "invalid device id: %s\n", s);
		return FALSE;
	}

	*pdevice_id = device_id;
	*pdevice_ip = 0;
	return TRUE;
}

static int cmd_get(const char *item)
{
	char *ret_value;
	char *ret_error;
	if (hdhomerun_device_get_var(hd, item, &ret_value, &ret_error) < 0) {
		fprintf(stderr, "communication error sending request to hdhomerun device\n");
		return -1;
	}

	if (ret_error) {
		printf("%s\n", ret_error);
		return 0;
	}

	printf("%s\n", ret_value);
	return 1;
}

static int cmd_set(const char *item, const char *value)
{
	char *ret_error;
	if (hdhomerun_device_set_var(hd, item, value, NULL, &ret_error) < 0) {
		fprintf(stderr, "communication error sending request to hdhomerun device\n");
		return -1;
	}

	if (ret_error) {
		printf("%s\n", ret_error);
		return 0;
	}

	return 1;
}

static int cmd_streaminfo(const char *tuner_str)
{
	fprintf(stderr, "streaminfo: use \"get /tuner<n>/streaminfo\"\n");
	return -1;
}

static int cmd_scan(const char *tuner_str, const char *start_value)
{
	unsigned int tuner;
	if (sscanf(tuner_str, "%u", &tuner) != 1) {
		fprintf(stderr, "invalid tuner number\n");
		return -1;
	}

	hdhomerun_device_set_tuner(hd, tuner);

	char channel_str[64];
	strncpy(channel_str, start_value, sizeof(channel_str));
	channel_str[sizeof(channel_str) - 8] = 0;

	char *channel_number_ptr = strrchr(channel_str, ':');
	if (!channel_number_ptr) {
		channel_number_ptr = channel_str;
	} else {
		channel_number_ptr++;
	}

	unsigned int channel_number = atol(channel_number_ptr);
	if (channel_number == 0) {
		fprintf(stderr, "invalid starting channel\n");
		return -1;
	}

	/* Test starting channel. */
	int ret = hdhomerun_device_set_tuner_channel(hd, channel_str);
	if (ret < 0) {
		fprintf(stderr, "communication error sending request to hdhomerun device\n");
		return -1;
	}
	if (ret == 0) {
		fprintf(stderr, "invalid starting channel\n");
		return -1;
	}

	while (1) {
		/* Update channel value */
		sprintf(channel_number_ptr, "%u", channel_number);

		/* Set channel. */
		ret = hdhomerun_device_set_tuner_channel(hd, channel_str);
		if (ret < 0) {
			fprintf(stderr, "communication error sending request to hdhomerun device\n");
			return -1;
		}
		if (ret == 0) {
			return 0;
		}

		/* Wait 1.5s for lock (qam auto is the slowest to lock). */
		usleep(HDHOMERUN_DEVICE_MAX_TUNE_TO_LOCK_TIME * 1000);

		/* Get status to check for signal. Quality numbers will not be valid yet. */
		struct hdhomerun_tuner_status_t status;
		if (hdhomerun_device_get_tuner_status(hd, &status) < 0) {
			fprintf(stderr, "communication error sending request to hdhomerun device\n");
			return -1;
		}

		/* If no signal then advance to next channel. */
		if (status.signal_strength == 0) {
			printf("%s: no signal\n", channel_str);
			channel_number++;
			continue;
		}

		/* Wait for 2s. */
		usleep(HDHOMERUN_DEVICE_MAX_LOCK_TO_DATA_TIME * 1000);

		/* Get status to check quality numbers. */
		if (hdhomerun_device_get_tuner_status(hd, &status) < 0) {
			fprintf(stderr, "communication error sending request to hdhomerun device\n");
			return -1;
		}
		if (status.signal_strength == 0) {
			printf("%s: no signal\n", channel_str);
			channel_number++;
			continue;
		}
		printf("%s: ss=%u snq=%u seq=%u\n", channel_str, status.signal_strength, status.signal_to_noise_quality, status.symbol_error_quality);

		/* Detect sub channels. */
		usleep(4 * 1000000);
		char *streaminfo;
		if (hdhomerun_device_get_tuner_streaminfo(hd, &streaminfo) <= 0) {
			channel_number++;
			continue;
		}
		while (1) {
			char *end = strchr(streaminfo, '\n');
			if (!end) {
				break;
			}

			*end++ = 0;
			printf("program %s\n", streaminfo);

			streaminfo = end;
		}

		/* Advance to next channel. */
		channel_number++;
	}
}

static int cmd_upgrade(const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		fprintf(stderr, "unable to open file %s\n", filename);
		return -1;
	}

	if (hdhomerun_device_upgrade(hd, fp) <= 0) {
		fprintf(stderr, "error sending upgrade file to hdhomerun device\n");
		fclose(fp);
		return -1;
	}

	printf("upgrade complete\n");
	return 0;
}

static int main_cmd(int argc, char *argv[])
{
	if (argc < 1) {
		return help();
	}

	char *cmd = *argv++; argc--;

	if (contains(cmd, "get")) {
		if (argc < 1) {
			return help();
		}
		return cmd_get(argv[0]);
	}

	if (contains(cmd, "set")) {
		if (argc < 2) {
			return help();
		}
		return cmd_set(argv[0], argv[1]);
	}

	if (contains(cmd, "streaminfo")) {
		if (argc < 1) {
			return help();
		}
		return cmd_streaminfo(argv[0]);
	}

	if (contains(cmd, "scan")) {
		if (argc < 2) {
			return help();
		}
		return cmd_scan(argv[0], argv[1]);
	}

	if (contains(cmd, "upgrade")) {
		if (argc < 1) {
			return help();
		}
		return cmd_upgrade(argv[0]);
	}

	return help();
}

static int main_internal(int argc, char *argv[])
{
#if defined(__WINDOWS__)
	//Start pthreads
	pthread_win32_process_attach_np();

	// Start WinSock
	WORD wVersionRequested = MAKEWORD(2, 0);
	WSADATA wsaData;
	WSAStartup(wVersionRequested, &wsaData);
#endif

	extract_appname(argv[0]);
	argv++;
	argc--;

	if (argc == 0) {
		return help();
	}

	char *id_str = *argv++; argc--;
	if (contains(id_str, "help")) {
		return help();
	}
	if (contains(id_str, "discover")) {
		return discover_print();
	}

	/* Device ID. */
	uint32_t device_id, device_ip;
	if (!parse_device_id_str(id_str, &device_id, &device_ip)) {
		return -1;
	}

	/* Device object. */
	hd = hdhomerun_device_create(device_id, device_ip, 0);
	if (!hd) {
		fprintf(stderr, "unable to create device\n");
		return -1;
	}

	/* Connect to device and check firmware version. */
	int ret = hdhomerun_device_firmware_version_check(hd, 0);
	if (ret < 0) {
		fprintf(stderr, "unable to connect to device\n");
		hdhomerun_device_destroy(hd);
		return -1;
	}
	if (ret == 0) {
		fprintf(stderr, "WARNING: firmware upgrade needed for all operations to function\n");
	}

	/* Command. */
	ret = main_cmd(argc, argv);

	/* Cleanup. */
	hdhomerun_device_destroy(hd);

	/* Complete. */
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = main_internal(argc, argv);
	if (ret <= 0) {
		return 1;
	}
	return 0;
}
