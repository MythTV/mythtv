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

#include <stdio.h>
#include "hdhomerun_os.h"
#include "hdhomerun_pkt.h"
#include "hdhomerun_discover.h"
#include "hdhomerun_control.h"

const char *appname;

static int help(void)
{
	printf("Usage:\n");
	printf("\t%s discover\n", appname);
	printf("\t%s <id|ip> get help\n", appname);
	printf("\t%s <id|ip> get <item>\n", appname);
	printf("\t%s <id|ip> set <item> <value>\n", appname);
	printf("\t%s <id|ip> upgrade <filename>\n", appname);
	return 1;
}

static void extract_appname(const char *argv0)
{
	char *ptr = strrchr(argv0, '/');
	if (ptr) {
		argv0 = ptr + 1;
	}
	ptr = strrchr(argv0, '\\');
	if (ptr) {
		argv0 = ptr + 1;
	}
	appname = argv0;
}

static int contains(const char *arg, const char *cmpstr)
{
	if (strcmp(arg, cmpstr) == 0) {
		return 1;
	}

	if (*arg++ != '-') {
		return 0;
	}
	if (*arg++ != '-') {
		return 0;
	}
	if (strcmp(arg, cmpstr) == 0) {
		return 1;
	}

	return 0;
}

static int discover_print_internal(struct hdhomerun_discover_sock_t *discover_sock)
{
	if (hdhomerun_discover_send(discover_sock, HDHOMERUN_DEVICE_TYPE_TUNER, HDHOMERUN_DEVICE_ID_WILDCARD) < 0) {
		fprintf(stderr, "unable to send discover request\n");
		return 1;
	}

	while (1) {
		struct hdhomerun_discover_device_t result;
		int ret = hdhomerun_discover_recv(discover_sock, &result, 1000);
		if (ret < 0) {
			fprintf(stderr, "error listening for discover response\n");
			return 1;
		}
		if (ret == 0) {
			return 0;
		}

		printf("hdhomerun device %08lX found at %ld.%ld.%ld.%ld\n",
			result.device_id,
			(result.ip_addr >> 24) & 0x0FF, (result.ip_addr >> 16) & 0x0FF,
			(result.ip_addr >> 8) & 0x0FF, (result.ip_addr >> 0) & 0x0FF
		);
	}
}

static int discover_print(void)
{
	struct hdhomerun_discover_sock_t *discover_sock = hdhomerun_discover_create(1000);
	if (!discover_sock) {
		fprintf(stderr, "unable to create discover socket\n");
		return 1;
	}

	int ret = discover_print_internal(discover_sock);

	hdhomerun_discover_destroy(discover_sock);
	return ret;
}

static unsigned long get_ip_addr(struct hdhomerun_discover_sock_t *discover_sock, const char *id_str)
{
	int a[4];
	if (sscanf(id_str, "%u.%u.%u.%u", &a[0], &a[1], &a[2], &a[3]) == 4) {
		return (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | (a[3] << 0);
	}

	unsigned long id = (unsigned long)strtoll(id_str, NULL, 16);
	if (!hdhomerun_discover_validate_device_id(id)) {
		fprintf(stderr, "invalid device id: %s\n", id_str);
		return 0;
	}

	if (hdhomerun_discover_send(discover_sock, HDHOMERUN_DEVICE_TYPE_TUNER, id) < 0) {
		fprintf(stderr, "unable to send discover request\n");
		return 0;
	}

	struct hdhomerun_discover_device_t result;
	int ret = hdhomerun_discover_recv(discover_sock, &result, 1000);
	if (ret < 0) {
		fprintf(stderr, "error listening for discover response\n");
		return 0;
	}
	if (ret == 0) {
		fprintf(stderr, "unable to find hdhomerun device %08lX\n", id);
		return 0;
	}

	return result.ip_addr;
}

static struct hdhomerun_control_sock_t *create_control_sock(const char *id_str)
{
	struct hdhomerun_discover_sock_t *discover_sock = hdhomerun_discover_create(1000);
	if (!discover_sock) {
		fprintf(stderr, "unable to create discover socket\n");
		return NULL;
	}

	unsigned long ip_addr = get_ip_addr(discover_sock, id_str);

	hdhomerun_discover_destroy(discover_sock);
	if (ip_addr == 0) {
		return NULL;
	}

	struct hdhomerun_control_sock_t *control_sock = hdhomerun_control_create(ip_addr, 1000);
	if (!control_sock) {
		fprintf(stderr, "unable to connect to hdhomerun device\n");
		return NULL;
	}

	return control_sock;
}

int cmd_get(struct hdhomerun_control_sock_t *control_sock, const char *item)
{
	if (hdhomerun_control_send_get_request(control_sock, item) < 0) {
		fprintf(stderr, "communication error sending request to hdhomerun device\n");
		return 1;
	}

	struct hdhomerun_control_data_t result;
	if (hdhomerun_control_recv(control_sock, &result, 1000) <= 0) {
		fprintf(stderr, "communication error receiving response from hdhomerun device\n");
		return 1;
	}
	if (result.type != HDHOMERUN_TYPE_GETSET_RPY) {
		fprintf(stderr, "unexpected reply type from hdhomerun device\n");
		return 1;
	}

	while (result.ptr < result.end) {
		unsigned char tag;
		int length;
		unsigned char *value;
		if (hdhomerun_read_tlv(&result.ptr, result.end, &tag, &length, &value) < 0) {
			break;
		}
		switch (tag) {
		case HDHOMERUN_TAG_ERROR_MESSAGE:
		case HDHOMERUN_TAG_GETSET_VALUE:
			printf("%s\n", (char *)value);
			break;
		}
	}

	return 0;
}

int cmd_set(struct hdhomerun_control_sock_t *control_sock, const char *item, const char *value)
{
	if (hdhomerun_control_send_set_request(control_sock, item, value) < 0) {
		fprintf(stderr, "communication error sending request to hdhomerun device\n");
		return 1;
	}

	struct hdhomerun_control_data_t result;
	if (hdhomerun_control_recv(control_sock, &result, 1000) <= 0) {
		fprintf(stderr, "communication error receiving response from hdhomerun device\n");
		return 1;
	}
	if (result.type != HDHOMERUN_TYPE_GETSET_RPY) {
		fprintf(stderr, "unexpected reply type from hdhomerun device\n");
		return 1;
	}

	while (result.ptr < result.end) {
		unsigned char tag;
		int length;
		unsigned char *value;
		if (hdhomerun_read_tlv(&result.ptr, result.end, &tag, &length, &value) < 0) {
			break;
		}
		switch (tag) {
		case HDHOMERUN_TAG_ERROR_MESSAGE:
			printf("%s\n", (char *)value);
			break;
		}
	}

	return 0;
}

int cmd_upgrade(struct hdhomerun_control_sock_t *control_sock, const char *filename)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp) {
		fprintf(stderr, "unable to open file %s\n", filename);
		return 1;
	}

	unsigned long sequence = 0;
	while (1) {
		unsigned char data[256];
		int length = fread(data, 1, 256, fp);
		if (length == 0) {
			break;
		}

		int ret = hdhomerun_control_send_upgrade_request(control_sock, sequence, data, length);
		if (ret < 0) {
			fprintf(stderr, "communication error sending upgrade request to hdhomerun device\n");
			fclose(fp);
			return 1;
		}

		sequence += length;
	}

	fclose(fp);
	if (sequence == 0) {
		fprintf(stderr, "upgrade file does not contain data\n");
		return 1;
	}

	int ret = hdhomerun_control_send_upgrade_request(control_sock, 0xFFFFFFFF, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "communication error sending upgrade request to hdhomerun device\n");
		return 1;
	}

	return 0;
}

int main_cmd(struct hdhomerun_control_sock_t *control_sock, int argc, char *argv[])
{
	if (argc < 1) {
		return help();
	}

	char *cmd = *argv++; argc--;

	if (contains(cmd, "get")) {
		if (argc < 1) {
			return help();
		}
		return cmd_get(control_sock, argv[0]);
	}

	if (contains(cmd, "set")) {
		if (argc < 2) {
			return help();
		}
		return cmd_set(control_sock, argv[0], argv[1]);
	}

	if (contains(cmd, "upgrade")) {
		if (argc < 1) {
			return help();
		}
		return cmd_upgrade(control_sock, argv[0]);
	}

	return help();
}

int main(int argc, char *argv[])
{
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

	struct hdhomerun_control_sock_t *control_sock = create_control_sock(id_str);
	if (!control_sock) {
		return 1;
	}

	int ret = main_cmd(control_sock, argc, argv);
	hdhomerun_control_destroy(control_sock);
	return ret;
}
