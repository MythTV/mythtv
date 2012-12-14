/*
 * hdhomerun_device.c
 *
 * Copyright © 2006-2010 Silicondust USA Inc. <www.silicondust.com>.
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

struct hdhomerun_device_t {
	struct hdhomerun_control_sock_t *cs;
	struct hdhomerun_video_sock_t *vs;
	struct hdhomerun_debug_t *dbg;
	struct hdhomerun_channelscan_t *scan;
	uint32_t multicast_ip;
	uint16_t multicast_port;
	uint32_t device_id;
	unsigned int tuner;
	uint32_t lockkey;
	char name[32];
	char model[32];
};

static int hdhomerun_device_set_device_normal(struct hdhomerun_device_t *hd, uint32_t device_id, uint32_t device_ip)
{
	if (!hd->cs) {
		hd->cs = hdhomerun_control_create(0, 0, hd->dbg);
		if (!hd->cs) {
			hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_device: failed to create control object\n");
			return -1;
		}
	}

	hdhomerun_control_set_device(hd->cs, device_id, device_ip);

	if ((device_id == 0) || (device_id == HDHOMERUN_DEVICE_ID_WILDCARD)) {
		device_id = hdhomerun_control_get_device_id(hd->cs);
	}

	hd->multicast_ip = 0;
	hd->multicast_port = 0;
	hd->device_id = device_id;
	hd->tuner = 0;
	hd->lockkey = 0;

	sprintf(hd->name, "%08lX-%u", (unsigned long)hd->device_id, hd->tuner);
	hd->model[0] = 0; /* clear cached model string */

	return 1;
}

static int hdhomerun_device_set_device_multicast(struct hdhomerun_device_t *hd, uint32_t multicast_ip)
{
	if (hd->cs) {
		hdhomerun_control_destroy(hd->cs);
		hd->cs = NULL;
	}

	hd->multicast_ip = multicast_ip;
	hd->multicast_port = 0;
	hd->device_id = 0;
	hd->tuner = 0;
	hd->lockkey = 0;

	unsigned long ip = multicast_ip;
	sprintf(hd->name, "%lu.%lu.%lu.%lu", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip >> 0) & 0xFF);
	sprintf(hd->model, "multicast");

	return 1;
}

int hdhomerun_device_set_device(struct hdhomerun_device_t *hd, uint32_t device_id, uint32_t device_ip)
{
	if ((device_id == 0) && (device_ip == 0)) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_device: device not specified\n");
		return -1;
	}

	if (hdhomerun_discover_is_ip_multicast(device_ip)) {
		return hdhomerun_device_set_device_multicast(hd, device_ip);
	}

	return hdhomerun_device_set_device_normal(hd, device_id, device_ip);
}

int hdhomerun_device_set_tuner(struct hdhomerun_device_t *hd, unsigned int tuner)
{
	if (hd->multicast_ip != 0) {
		if (tuner != 0) {
			hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner: tuner cannot be specified in multicast mode\n");
			return -1;
		}

		return 1;
	}

	hd->tuner = tuner;
	sprintf(hd->name, "%08lX-%u", (unsigned long)hd->device_id, hd->tuner);

	return 1;
}

struct hdhomerun_device_t *hdhomerun_device_create(uint32_t device_id, uint32_t device_ip, unsigned int tuner, struct hdhomerun_debug_t *dbg)
{
	struct hdhomerun_device_t *hd = (struct hdhomerun_device_t *)calloc(1, sizeof(struct hdhomerun_device_t));
	if (!hd) {
		hdhomerun_debug_printf(dbg, "hdhomerun_device_create: failed to allocate device object\n");
		return NULL;
	}

	hd->dbg = dbg;

	if ((device_id == 0) && (device_ip == 0) && (tuner == 0)) {
		return hd;
	}

	if (hdhomerun_device_set_device(hd, device_id, device_ip) <= 0) {
		free(hd);
		return NULL;
	}
	if (hdhomerun_device_set_tuner(hd, tuner) <= 0) {
		free(hd);
		return NULL;
	}

	return hd;
}

void hdhomerun_device_destroy(struct hdhomerun_device_t *hd)
{
	if (hd->scan) {
		channelscan_destroy(hd->scan);
	}

	if (hd->vs) {
		hdhomerun_video_destroy(hd->vs);
	}

	if (hd->cs) {
		hdhomerun_control_destroy(hd->cs);
	}

	free(hd);
}

static bool_t is_hex_char(char c)
{
	if ((c >= '0') && (c <= '9')) {
		return TRUE;
	}
	if ((c >= 'A') && (c <= 'F')) {
		return TRUE;
	}
	if ((c >= 'a') && (c <= 'f')) {
		return TRUE;
	}
	return FALSE;
}

static struct hdhomerun_device_t *hdhomerun_device_create_from_str_device_id(const char *device_str, struct hdhomerun_debug_t *dbg)
{
	int i;
	const char *ptr = device_str;
	for (i = 0; i < 8; i++) {
		if (!is_hex_char(*ptr++)) {
			return NULL;
		}
	}

	if (*ptr == 0) {
		unsigned long device_id;
		if (sscanf(device_str, "%lx", &device_id) != 1) {
			return NULL;
		}
		return hdhomerun_device_create((uint32_t)device_id, 0, 0, dbg);
	}

	if (*ptr == '-') {
		unsigned long device_id;
		unsigned int tuner;
		if (sscanf(device_str, "%lx-%u", &device_id, &tuner) != 2) {
			return NULL;
		}
		return hdhomerun_device_create((uint32_t)device_id, 0, tuner, dbg);
	}

	return NULL;
}

static struct hdhomerun_device_t *hdhomerun_device_create_from_str_ip_result(unsigned long a[4], unsigned int port, unsigned int tuner, struct hdhomerun_debug_t *dbg)
{
	unsigned long device_ip = (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | (a[3] << 0);
	struct hdhomerun_device_t *hd = hdhomerun_device_create(HDHOMERUN_DEVICE_ID_WILDCARD, (uint32_t)device_ip, tuner, dbg);
	if (!hd) {
		return NULL;
	}

	if (hd->multicast_ip != 0) {
		hd->multicast_port = (uint16_t)port;
	}

	return hd;
}

static struct hdhomerun_device_t *hdhomerun_device_create_from_str_ip(const char *device_str, struct hdhomerun_debug_t *dbg)
{
	unsigned long a[4];
	unsigned int port = 0;
	unsigned int tuner = 0;

	if (sscanf(device_str, "%lu.%lu.%lu.%lu:%u", &a[0], &a[1], &a[2], &a[3], &port) == 5) {
		return hdhomerun_device_create_from_str_ip_result(a, port, tuner, dbg);
	}
	if (sscanf(device_str, "%lu.%lu.%lu.%lu-%u", &a[0], &a[1], &a[2], &a[3], &tuner) == 5) {
		return hdhomerun_device_create_from_str_ip_result(a, port, tuner, dbg);
	}
	if (sscanf(device_str, "%lu.%lu.%lu.%lu", &a[0], &a[1], &a[2], &a[3]) == 4) {
		return hdhomerun_device_create_from_str_ip_result(a, port, tuner, dbg);
	}

	return NULL;
}

static struct hdhomerun_device_t *hdhomerun_device_create_from_str_dns(const char *device_str, struct hdhomerun_debug_t *dbg)
{
#if defined(__CYGWIN__)
	return NULL;
#else
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *sock_info;
	if (getaddrinfo(device_str, "65001", &hints, &sock_info) != 0) {
		return NULL;
	}

	struct sockaddr_in *sock_addr = (struct sockaddr_in *)sock_info->ai_addr;
	uint32_t device_ip = ntohl(sock_addr->sin_addr.s_addr);
	freeaddrinfo(sock_info);

	if (device_ip == 0) {
		return NULL;
	}

	return hdhomerun_device_create(HDHOMERUN_DEVICE_ID_WILDCARD, (uint32_t)device_ip, 0, dbg);
#endif
}

struct hdhomerun_device_t *hdhomerun_device_create_from_str(const char *device_str, struct hdhomerun_debug_t *dbg)
{
	struct hdhomerun_device_t *device = hdhomerun_device_create_from_str_device_id(device_str, dbg);
	if (device) {
		return device;
	}

	device = hdhomerun_device_create_from_str_ip(device_str, dbg);
	if (device) {
		return device;
	}

	device = hdhomerun_device_create_from_str_dns(device_str, dbg);
	if (device) {
		return device;
	}

	return NULL;
}

int hdhomerun_device_set_tuner_from_str(struct hdhomerun_device_t *hd, const char *tuner_str)
{
	unsigned int tuner;
	if (sscanf(tuner_str, "%u", &tuner) == 1) {
		hdhomerun_device_set_tuner(hd, tuner);
		return 1;
	}
	if (sscanf(tuner_str, "/tuner%u", &tuner) == 1) {
		hdhomerun_device_set_tuner(hd, tuner);
		return 1;
	}

	return -1;
}

const char *hdhomerun_device_get_name(struct hdhomerun_device_t *hd)
{
	return hd->name;
}

uint32_t hdhomerun_device_get_device_id(struct hdhomerun_device_t *hd)
{
	return hd->device_id;
}

uint32_t hdhomerun_device_get_device_ip(struct hdhomerun_device_t *hd)
{
	if (hd->multicast_ip != 0) {
		return hd->multicast_ip;
	}
	if (hd->cs) {
		return hdhomerun_control_get_device_ip(hd->cs);
	}

	return 0;
}

uint32_t hdhomerun_device_get_device_id_requested(struct hdhomerun_device_t *hd)
{
	if (hd->multicast_ip != 0) {
		return 0;
	}
	if (hd->cs) {
		return hdhomerun_control_get_device_id_requested(hd->cs);
	}

	return 0;
}

uint32_t hdhomerun_device_get_device_ip_requested(struct hdhomerun_device_t *hd)
{
	if (hd->multicast_ip != 0) {
		return hd->multicast_ip;
	}
	if (hd->cs) {
		return hdhomerun_control_get_device_ip_requested(hd->cs);
	}

	return 0;
}

unsigned int hdhomerun_device_get_tuner(struct hdhomerun_device_t *hd)
{
	return hd->tuner;
}

struct hdhomerun_control_sock_t *hdhomerun_device_get_control_sock(struct hdhomerun_device_t *hd)
{
	return hd->cs;
}

struct hdhomerun_video_sock_t *hdhomerun_device_get_video_sock(struct hdhomerun_device_t *hd)
{
	if (hd->vs) {
		return hd->vs;
	}

	bool_t allow_port_reuse = (hd->multicast_port != 0);

	hd->vs = hdhomerun_video_create(hd->multicast_port, allow_port_reuse, VIDEO_DATA_BUFFER_SIZE_1S * 2, hd->dbg);
	if (!hd->vs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_video_sock: failed to create video object\n");
		return NULL;
	}

	return hd->vs;
}

uint32_t hdhomerun_device_get_local_machine_addr(struct hdhomerun_device_t *hd)
{
	if (hd->cs) {
		return hdhomerun_control_get_local_addr(hd->cs);
	}

	return 0;
}

static uint32_t hdhomerun_device_get_status_parse(const char *status_str, const char *tag)
{
	const char *ptr = strstr(status_str, tag);
	if (!ptr) {
		return 0;
	}

	unsigned long value = 0;
	sscanf(ptr + strlen(tag), "%lu", &value);

	return (uint32_t)value;
}

static bool_t hdhomerun_device_get_tuner_status_lock_is_bcast(struct hdhomerun_tuner_status_t *status)
{
	if (strcmp(status->lock_str, "8vsb") == 0) {
		return TRUE;
	}
	if (strncmp(status->lock_str, "t8", 2) == 0) {
		return TRUE;
	}
	if (strncmp(status->lock_str, "t7", 2) == 0) {
		return TRUE;
	}
	if (strncmp(status->lock_str, "t6", 2) == 0) {
		return TRUE;
	}

	return FALSE;
}

uint32_t hdhomerun_device_get_tuner_status_ss_color(struct hdhomerun_tuner_status_t *status)
{
	unsigned int ss_yellow_min;
	unsigned int ss_green_min;

	if (!status->lock_supported) {
		return HDHOMERUN_STATUS_COLOR_NEUTRAL;
	}

	if (hdhomerun_device_get_tuner_status_lock_is_bcast(status)) {
		ss_yellow_min = 50;	/* -30dBmV */
		ss_green_min = 75;	/* -15dBmV */
	} else {
		ss_yellow_min = 80;	/* -12dBmV */
		ss_green_min = 90;	/* -6dBmV */
	}

	if (status->signal_strength >= ss_green_min) {
		return HDHOMERUN_STATUS_COLOR_GREEN;
	}
	if (status->signal_strength >= ss_yellow_min) {
		return HDHOMERUN_STATUS_COLOR_YELLOW;
	}

	return HDHOMERUN_STATUS_COLOR_RED;
}

uint32_t hdhomerun_device_get_tuner_status_snq_color(struct hdhomerun_tuner_status_t *status)
{
	if (status->signal_to_noise_quality >= 70) {
		return HDHOMERUN_STATUS_COLOR_GREEN;
	}
	if (status->signal_to_noise_quality >= 50) {
		return HDHOMERUN_STATUS_COLOR_YELLOW;
	}

	return HDHOMERUN_STATUS_COLOR_RED;
}

uint32_t hdhomerun_device_get_tuner_status_seq_color(struct hdhomerun_tuner_status_t *status)
{
	if (status->symbol_error_quality >= 100) {
		return HDHOMERUN_STATUS_COLOR_GREEN;
	}

	return HDHOMERUN_STATUS_COLOR_RED;
}

int hdhomerun_device_get_tuner_status(struct hdhomerun_device_t *hd, char **pstatus_str, struct hdhomerun_tuner_status_t *status)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_status: device not set\n");
		return -1;
	}

	memset(status, 0, sizeof(struct hdhomerun_tuner_status_t));

	char name[32];
	sprintf(name, "/tuner%u/status", hd->tuner);

	char *status_str;
	int ret = hdhomerun_control_get(hd->cs, name, &status_str, NULL);
	if (ret <= 0) {
		return ret;
	}

	if (pstatus_str) {
		*pstatus_str = status_str;
	}

	if (status) {
		char *channel = strstr(status_str, "ch=");
		if (channel) {
			sscanf(channel + 3, "%31s", status->channel);
		}

		char *lock = strstr(status_str, "lock=");
		if (lock) {
			sscanf(lock + 5, "%31s", status->lock_str);
		}

		status->signal_strength = (unsigned int)hdhomerun_device_get_status_parse(status_str, "ss=");
		status->signal_to_noise_quality = (unsigned int)hdhomerun_device_get_status_parse(status_str, "snq=");
		status->symbol_error_quality = (unsigned int)hdhomerun_device_get_status_parse(status_str, "seq=");
		status->raw_bits_per_second = hdhomerun_device_get_status_parse(status_str, "bps=");
		status->packets_per_second = hdhomerun_device_get_status_parse(status_str, "pps=");

		status->signal_present = status->signal_strength >= 45;

		if (strcmp(status->lock_str, "none") != 0) {
			if (status->lock_str[0] == '(') {
				status->lock_unsupported = TRUE;
			} else {
				status->lock_supported = TRUE;
			}
		}
	}

	return 1;
}

int hdhomerun_device_get_oob_status(struct hdhomerun_device_t *hd, char **pstatus_str, struct hdhomerun_tuner_status_t *status)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_oob_status: device not set\n");
		return -1;
	}

	memset(status, 0, sizeof(struct hdhomerun_tuner_status_t));

	char *status_str;
	int ret = hdhomerun_control_get(hd->cs, "/oob/status", &status_str, NULL);
	if (ret <= 0) {
		return ret;
	}

	if (pstatus_str) {
		*pstatus_str = status_str;
	}

	if (status) {
		char *channel = strstr(status_str, "ch=");
		if (channel) {
			sscanf(channel + 3, "%31s", status->channel);
		}

		char *lock = strstr(status_str, "lock=");
		if (lock) {
			sscanf(lock + 5, "%31s", status->lock_str);
		}

		status->signal_strength = (unsigned int)hdhomerun_device_get_status_parse(status_str, "ss=");
		status->signal_to_noise_quality = (unsigned int)hdhomerun_device_get_status_parse(status_str, "snq=");
		status->signal_present = status->signal_strength >= 45;
		status->lock_supported = (strcmp(status->lock_str, "none") != 0);
	}

	return 1;
}

int hdhomerun_device_get_tuner_vstatus(struct hdhomerun_device_t *hd, char **pvstatus_str, struct hdhomerun_tuner_vstatus_t *vstatus)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_vstatus: device not set\n");
		return -1;
	}

	memset(vstatus, 0, sizeof(struct hdhomerun_tuner_vstatus_t));

	char var_name[32];
	sprintf(var_name, "/tuner%u/vstatus", hd->tuner);

	char *vstatus_str;
	int ret = hdhomerun_control_get(hd->cs, var_name, &vstatus_str, NULL);
	if (ret <= 0) {
		return ret;
	}

	if (pvstatus_str) {
		*pvstatus_str = vstatus_str;
	}

	if (vstatus) {
		char *vch = strstr(vstatus_str, "vch=");
		if (vch) {
			sscanf(vch + 4, "%31s", vstatus->vchannel);
		}

		char *name = strstr(vstatus_str, "name=");
		if (name) {
			sscanf(name + 5, "%31s", vstatus->name);
		}

		char *auth = strstr(vstatus_str, "auth=");
		if (auth) {
			sscanf(auth + 5, "%31s", vstatus->auth);
		}

		char *cci = strstr(vstatus_str, "cci=");
		if (cci) {
			sscanf(cci + 4, "%31s", vstatus->cci);
		}

		char *cgms = strstr(vstatus_str, "cgms=");
		if (cgms) {
			sscanf(cgms + 5, "%31s", vstatus->cgms);
		}

		if (strncmp(vstatus->auth, "not-subscribed", 14) == 0) {
			vstatus->not_subscribed = TRUE;
		}

		if (strncmp(vstatus->auth, "error", 5) == 0) {
			vstatus->not_available = TRUE;
		}
		if (strncmp(vstatus->auth, "dialog", 6) == 0) {
			vstatus->not_available = TRUE;
		}

		if (strncmp(vstatus->cci, "protected", 9) == 0) {
			vstatus->copy_protected = TRUE;
		}
		if (strncmp(vstatus->cgms, "protected", 9) == 0) {
			vstatus->copy_protected = TRUE;
		}
	}

	return 1;
}

int hdhomerun_device_get_tuner_streaminfo(struct hdhomerun_device_t *hd, char **pstreaminfo)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_streaminfo: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/streaminfo", hd->tuner);
	return hdhomerun_control_get(hd->cs, name, pstreaminfo, NULL);
}

int hdhomerun_device_get_tuner_channel(struct hdhomerun_device_t *hd, char **pchannel)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_channel: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/channel", hd->tuner);
	return hdhomerun_control_get(hd->cs, name, pchannel, NULL);
}

int hdhomerun_device_get_tuner_vchannel(struct hdhomerun_device_t *hd, char **pvchannel)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_vchannel: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/vchannel", hd->tuner);
	return hdhomerun_control_get(hd->cs, name, pvchannel, NULL);
}

int hdhomerun_device_get_tuner_channelmap(struct hdhomerun_device_t *hd, char **pchannelmap)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_channelmap: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/channelmap", hd->tuner);
	return hdhomerun_control_get(hd->cs, name, pchannelmap, NULL);
}

int hdhomerun_device_get_tuner_filter(struct hdhomerun_device_t *hd, char **pfilter)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_filter: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/filter", hd->tuner);
	return hdhomerun_control_get(hd->cs, name, pfilter, NULL);
}

int hdhomerun_device_get_tuner_program(struct hdhomerun_device_t *hd, char **pprogram)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_program: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/program", hd->tuner);
	return hdhomerun_control_get(hd->cs, name, pprogram, NULL);
}

int hdhomerun_device_get_tuner_target(struct hdhomerun_device_t *hd, char **ptarget)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_target: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/target", hd->tuner);
	return hdhomerun_control_get(hd->cs, name, ptarget, NULL);
}

static int hdhomerun_device_get_tuner_plotsample_internal(struct hdhomerun_device_t *hd, const char *name, struct hdhomerun_plotsample_t **psamples, size_t *pcount)
{
	char *result;
	int ret = hdhomerun_control_get(hd->cs, name, &result, NULL);
	if (ret <= 0) {
		return ret;
	}

	struct hdhomerun_plotsample_t *samples = (struct hdhomerun_plotsample_t *)result;
	*psamples = samples;
	size_t count = 0;

	while (1) {
		char *ptr = strchr(result, ' ');
		if (!ptr) {
			break;
		}
		*ptr++ = 0;

		unsigned long raw;
		if (sscanf(result, "%lx", &raw) != 1) {
			break;
		}

		uint16_t real = (raw >> 12) & 0x0FFF;
		if (real & 0x0800) {
			real |= 0xF000;
		}

		uint16_t imag = (raw >> 0) & 0x0FFF;
		if (imag & 0x0800) {
			imag |= 0xF000;
		}

		samples->real = (int16_t)real;
		samples->imag = (int16_t)imag;
		samples++;
		count++;

		result = ptr;
	}

	*pcount = count;
	return 1;
}

int hdhomerun_device_get_tuner_plotsample(struct hdhomerun_device_t *hd, struct hdhomerun_plotsample_t **psamples, size_t *pcount)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_plotsample: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/plotsample", hd->tuner);
	return hdhomerun_device_get_tuner_plotsample_internal(hd, name, psamples, pcount);
}

int hdhomerun_device_get_oob_plotsample(struct hdhomerun_device_t *hd, struct hdhomerun_plotsample_t **psamples, size_t *pcount)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_oob_plotsample: device not set\n");
		return -1;
	}

	return hdhomerun_device_get_tuner_plotsample_internal(hd, "/oob/plotsample", psamples, pcount);
}

int hdhomerun_device_get_tuner_lockkey_owner(struct hdhomerun_device_t *hd, char **powner)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_tuner_lockkey_owner: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/lockkey", hd->tuner);
	return hdhomerun_control_get(hd->cs, name, powner, NULL);
}

int hdhomerun_device_get_ir_target(struct hdhomerun_device_t *hd, char **ptarget)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_ir_target: device not set\n");
		return -1;
	}

	return hdhomerun_control_get(hd->cs, "/ir/target", ptarget, NULL);
}

int hdhomerun_device_get_lineup_location(struct hdhomerun_device_t *hd, char **plocation)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_lineup_location: device not set\n");
		return -1;
	}

	return hdhomerun_control_get(hd->cs, "/lineup/location", plocation, NULL);
}

int hdhomerun_device_get_version(struct hdhomerun_device_t *hd, char **pversion_str, uint32_t *pversion_num)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_version: device not set\n");
		return -1;
	}

	char *version_str;
	int ret = hdhomerun_control_get(hd->cs, "/sys/version", &version_str, NULL);
	if (ret <= 0) {
		return ret;
	}

	if (pversion_str) {
		*pversion_str = version_str;
	}

	if (pversion_num) {
		unsigned long version_num;
		if (sscanf(version_str, "%lu", &version_num) != 1) {
			*pversion_num = 0;
		} else {
			*pversion_num = (uint32_t)version_num;
		}
	}

	return 1;
}

int hdhomerun_device_get_supported(struct hdhomerun_device_t *hd, char *prefix, char **pstr)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_channel: device not set\n");
		return -1;
	}

	char *features;
	int ret = hdhomerun_control_get(hd->cs, "/sys/features", &features, NULL);
	if (ret <= 0) {
		return ret;
	}

	if (!prefix) {
		*pstr = features;
		return 1;
	}

	char *ptr = strstr(features, prefix);
	if (!ptr) {
		return 0;
	}

	ptr += strlen(prefix);
	*pstr = ptr;

	ptr = strchr(ptr, '\n');
	if (ptr) {
		*ptr = 0;
	}

	return 1;
}

int hdhomerun_device_set_tuner_channel(struct hdhomerun_device_t *hd, const char *channel)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_channel: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/channel", hd->tuner);
	return hdhomerun_control_set_with_lockkey(hd->cs, name, channel, hd->lockkey, NULL, NULL);
}

int hdhomerun_device_set_tuner_vchannel(struct hdhomerun_device_t *hd, const char *vchannel)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_vchannel: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/vchannel", hd->tuner);
	return hdhomerun_control_set_with_lockkey(hd->cs, name, vchannel, hd->lockkey, NULL, NULL);
}

int hdhomerun_device_set_tuner_channelmap(struct hdhomerun_device_t *hd, const char *channelmap)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_channelmap: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/channelmap", hd->tuner);
	return hdhomerun_control_set_with_lockkey(hd->cs, name, channelmap, hd->lockkey, NULL, NULL);
}

int hdhomerun_device_set_tuner_filter(struct hdhomerun_device_t *hd, const char *filter)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_filter: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/filter", hd->tuner);
	return hdhomerun_control_set_with_lockkey(hd->cs, name, filter, hd->lockkey, NULL, NULL);
}

static int hdhomerun_device_set_tuner_filter_by_array_append(char **pptr, char *end, uint16_t range_begin, uint16_t range_end)
{
	char *ptr = *pptr;

	size_t available = end - ptr;
	size_t required;

	if (range_begin == range_end) {
		required = snprintf(ptr, available, "0x%04x ", range_begin) + 1;
	} else {
		required = snprintf(ptr, available, "0x%04x-0x%04x ", range_begin, range_end) + 1;
	}

	if (required > available) {
		return FALSE;
	}

	*pptr = strchr(ptr, 0);
	return TRUE;
}

int hdhomerun_device_set_tuner_filter_by_array(struct hdhomerun_device_t *hd, unsigned char filter_array[0x2000])
{
	char filter[1024];
	char *ptr = filter;
	char *end = filter + sizeof(filter);

	uint16_t range_begin = 0xFFFF;
	uint16_t range_end = 0xFFFF;

	uint16_t i;
	for (i = 0; i <= 0x1FFF; i++) {
		if (!filter_array[i]) {
			if (range_begin == 0xFFFF) {
				continue;
			}
			if (!hdhomerun_device_set_tuner_filter_by_array_append(&ptr, end, range_begin, range_end)) {
				return 0;
			}
			range_begin = 0xFFFF;
			range_end = 0xFFFF;
			continue;
		}

		if (range_begin == 0xFFFF) {
			range_begin = i;
			range_end = i;
			continue;
		}

		range_end = i;
	}

	if (range_begin != 0xFFFF) {
		if (!hdhomerun_device_set_tuner_filter_by_array_append(&ptr, end, range_begin, range_end)) {
			return 0;
		}
	}

	/* Remove trailing space. */
	if (ptr > filter) {
		ptr--;
	}
	*ptr = 0;

	return hdhomerun_device_set_tuner_filter(hd, filter);
}

int hdhomerun_device_set_tuner_program(struct hdhomerun_device_t *hd, const char *program)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_program: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/program", hd->tuner);
	return hdhomerun_control_set_with_lockkey(hd->cs, name, program, hd->lockkey, NULL, NULL);
}

int hdhomerun_device_set_tuner_target(struct hdhomerun_device_t *hd, const char *target)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_target: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/target", hd->tuner);
	return hdhomerun_control_set_with_lockkey(hd->cs, name, target, hd->lockkey, NULL, NULL);
}

static int hdhomerun_device_set_tuner_target_to_local(struct hdhomerun_device_t *hd, const char *protocol)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_target_to_local: device not set\n");
		return -1;
	}
	if (!hd->vs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_tuner_target_to_local: video not initialized\n");
		return -1;
	}

	/* Set target. */
	char target[64];
	uint32_t local_ip = hdhomerun_control_get_local_addr(hd->cs);
	uint16_t local_port = hdhomerun_video_get_local_port(hd->vs);
	sprintf(target, "%s://%u.%u.%u.%u:%u",
		protocol,
		(unsigned int)(local_ip >> 24) & 0xFF, (unsigned int)(local_ip >> 16) & 0xFF,
		(unsigned int)(local_ip >> 8) & 0xFF, (unsigned int)(local_ip >> 0) & 0xFF,
		(unsigned int)local_port
	);

	return hdhomerun_device_set_tuner_target(hd, target);
}

int hdhomerun_device_set_ir_target(struct hdhomerun_device_t *hd, const char *target)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_ir_target: device not set\n");
		return -1;
	}

	return hdhomerun_control_set(hd->cs, "/ir/target", target, NULL, NULL);
}

int hdhomerun_device_set_lineup_location(struct hdhomerun_device_t *hd, const char *location)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_lineup_location: device not set\n");
		return -1;
	}

	return hdhomerun_control_set(hd->cs, "/lineup/location", location, NULL, NULL);
}

int hdhomerun_device_set_sys_dvbc_modulation(struct hdhomerun_device_t *hd, const char *modulation_list)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_sys_dvbc_modulation: device not set\n");
		return -1;
	}

	return hdhomerun_control_set(hd->cs, "/sys/dvbc_modulation", modulation_list, NULL, NULL);
}

int hdhomerun_device_get_var(struct hdhomerun_device_t *hd, const char *name, char **pvalue, char **perror)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_var: device not set\n");
		return -1;
	}

	return hdhomerun_control_get(hd->cs, name, pvalue, perror);
}

int hdhomerun_device_set_var(struct hdhomerun_device_t *hd, const char *name, const char *value, char **pvalue, char **perror)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_set_var: device not set\n");
		return -1;
	}

	return hdhomerun_control_set_with_lockkey(hd->cs, name, value, hd->lockkey, pvalue, perror);
}

int hdhomerun_device_tuner_lockkey_request(struct hdhomerun_device_t *hd, char **perror)
{
	if (hd->multicast_ip != 0) {
		return 1;
	}
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_tuner_lockkey_request: device not set\n");
		return -1;
	}

	uint32_t new_lockkey = random_get32();

	char name[32];
	sprintf(name, "/tuner%u/lockkey", hd->tuner);

	char new_lockkey_str[64];
	sprintf(new_lockkey_str, "%u", (unsigned int)new_lockkey);

	int ret = hdhomerun_control_set_with_lockkey(hd->cs, name, new_lockkey_str, hd->lockkey, NULL, perror);
	if (ret <= 0) {
		hd->lockkey = 0;
		return ret;
	}

	hd->lockkey = new_lockkey;
	return ret;
}

int hdhomerun_device_tuner_lockkey_release(struct hdhomerun_device_t *hd)
{
	if (hd->multicast_ip != 0) {
		return 1;
	}
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_tuner_lockkey_release: device not set\n");
		return -1;
	}

	if (hd->lockkey == 0) {
		return 1;
	}

	char name[32];
	sprintf(name, "/tuner%u/lockkey", hd->tuner);
	int ret = hdhomerun_control_set_with_lockkey(hd->cs, name, "none", hd->lockkey, NULL, NULL);

	hd->lockkey = 0;
	return ret;
}

int hdhomerun_device_tuner_lockkey_force(struct hdhomerun_device_t *hd)
{
	if (hd->multicast_ip != 0) {
		return 1;
	}
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_tuner_lockkey_force: device not set\n");
		return -1;
	}

	char name[32];
	sprintf(name, "/tuner%u/lockkey", hd->tuner);
	int ret = hdhomerun_control_set(hd->cs, name, "force", NULL, NULL);

	hd->lockkey = 0;
	return ret;
}

void hdhomerun_device_tuner_lockkey_use_value(struct hdhomerun_device_t *hd, uint32_t lockkey)
{
	if (hd->multicast_ip != 0) {
		return;
	}

	hd->lockkey = lockkey;
}

int hdhomerun_device_wait_for_lock(struct hdhomerun_device_t *hd, struct hdhomerun_tuner_status_t *status)
{
	/* Delay for SS reading to be valid (signal present). */
	msleep_minimum(250);

	/* Wait for up to 2.5 seconds for lock. */
	uint64_t timeout = getcurrenttime() + 2500;
	while (1) {
		/* Get status to check for lock. Quality numbers will not be valid yet. */
		int ret = hdhomerun_device_get_tuner_status(hd, NULL, status);
		if (ret <= 0) {
			return ret;
		}

		if (!status->signal_present) {
			return 1;
		}
		if (status->lock_supported || status->lock_unsupported) {
			return 1;
		}

		if (getcurrenttime() >= timeout) {
			return 1;
		}

		msleep_approx(250);
	}
}

int hdhomerun_device_stream_start(struct hdhomerun_device_t *hd)
{
	hdhomerun_device_get_video_sock(hd);
	if (!hd->vs) {
		return -1;
	}

	/* Set target. */
	if (hd->multicast_ip != 0) {
		int ret = hdhomerun_video_join_multicast_group(hd->vs, hd->multicast_ip, 0);
		if (ret <= 0) {
			return ret;
		}
	} else {
		int ret = hdhomerun_device_set_tuner_target_to_local(hd, HDHOMERUN_TARGET_PROTOCOL_RTP);
		if (ret == 0) {
			ret = hdhomerun_device_set_tuner_target_to_local(hd, HDHOMERUN_TARGET_PROTOCOL_UDP);
		}
		if (ret <= 0) {
			return ret;
		}
	}

	/* Flush video buffer. */
	msleep_minimum(64);
	hdhomerun_video_flush(hd->vs);

	/* Success. */
	return 1;
}

uint8_t *hdhomerun_device_stream_recv(struct hdhomerun_device_t *hd, size_t max_size, size_t *pactual_size)
{
	if (!hd->vs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_stream_recv: video not initialized\n");
		return NULL;
	}

	return hdhomerun_video_recv(hd->vs, max_size, pactual_size);
}

void hdhomerun_device_stream_flush(struct hdhomerun_device_t *hd)
{
	if (!hd->vs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_stream_flush: video not initialized\n");
		return;
	}

	hdhomerun_video_flush(hd->vs);
}

void hdhomerun_device_stream_stop(struct hdhomerun_device_t *hd)
{
	if (!hd->vs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_stream_stop: video not initialized\n");
		return;
	}

	if (hd->multicast_ip != 0) {
		hdhomerun_video_leave_multicast_group(hd->vs);
	} else {
		hdhomerun_device_set_tuner_target(hd, "none");
	}
}

int hdhomerun_device_channelscan_init(struct hdhomerun_device_t *hd, const char *channelmap)
{
	if (hd->scan) {
		channelscan_destroy(hd->scan);
	}

	hd->scan = channelscan_create(hd, channelmap);
	if (!hd->scan) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_channelscan_init: failed to create scan object\n");
		return -1;
	}

	return 1;
}

int hdhomerun_device_channelscan_advance(struct hdhomerun_device_t *hd, struct hdhomerun_channelscan_result_t *result)
{
	if (!hd->scan) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_channelscan_advance: scan not initialized\n");
		return 0;
	}

	int ret = channelscan_advance(hd->scan, result);
	if (ret <= 0) { /* Free scan if normal finish or fatal error */
		channelscan_destroy(hd->scan);
		hd->scan = NULL;
	}

	return ret;
}

int hdhomerun_device_channelscan_detect(struct hdhomerun_device_t *hd, struct hdhomerun_channelscan_result_t *result)
{
	if (!hd->scan) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_channelscan_detect: scan not initialized\n");
		return 0;
	}

	int ret = channelscan_detect(hd->scan, result);
	if (ret < 0) { /* Free scan if fatal error */
		channelscan_destroy(hd->scan);
		hd->scan = NULL;
	}

	return ret;
}

uint8_t hdhomerun_device_channelscan_get_progress(struct hdhomerun_device_t *hd)
{
	if (!hd->scan) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_channelscan_get_progress: scan not initialized\n");
		return 0;
	}

	return channelscan_get_progress(hd->scan);
}

const char *hdhomerun_device_get_model_str(struct hdhomerun_device_t *hd)
{
	if (*hd->model) {
		return hd->model;
	}

	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_get_model_str: device not set\n");
		return NULL;
	}

	char *model_str;
	int ret = hdhomerun_control_get(hd->cs, "/sys/model", &model_str, NULL);
	if (ret < 0) {
		return NULL;
	}
	if (ret == 0) {
		strncpy(hd->model, "hdhomerun_atsc", sizeof(hd->model) - 1);
		hd->model[sizeof(hd->model) - 1] = 0;
		return hd->model;
	}

	strncpy(hd->model, model_str, sizeof(hd->model) - 1);
	hd->model[sizeof(hd->model) - 1] = 0;
	return hd->model;
}

int hdhomerun_device_upgrade(struct hdhomerun_device_t *hd, FILE *upgrade_file)
{
	if (!hd->cs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_upgrade: device not set\n");
		return -1;
	}

	hdhomerun_control_set(hd->cs, "/tuner0/lockkey", "force", NULL, NULL);
	hdhomerun_control_set(hd->cs, "/tuner0/channel", "none", NULL, NULL);

	hdhomerun_control_set(hd->cs, "/tuner1/lockkey", "force", NULL, NULL);
	hdhomerun_control_set(hd->cs, "/tuner1/channel", "none", NULL, NULL);

	return hdhomerun_control_upgrade(hd->cs, upgrade_file);
}

void hdhomerun_device_debug_print_video_stats(struct hdhomerun_device_t *hd)
{
	if (!hdhomerun_debug_enabled(hd->dbg)) {
		return;
	}

	if (hd->cs) {
		char name[32];
		sprintf(name, "/tuner%u/debug", hd->tuner);

		char *debug_str;
		char *error_str;
		int ret = hdhomerun_control_get(hd->cs, name, &debug_str, &error_str);
		if (ret < 0) {
			hdhomerun_debug_printf(hd->dbg, "video dev: communication error getting debug stats\n");
			return;
		}

		if (error_str) {
			hdhomerun_debug_printf(hd->dbg, "video dev: %s\n", error_str);
		} else {
			hdhomerun_debug_printf(hd->dbg, "video dev: %s\n", debug_str);
		}
	}

	if (hd->vs) {
		hdhomerun_video_debug_print_stats(hd->vs);
	}
}

void hdhomerun_device_get_video_stats(struct hdhomerun_device_t *hd, struct hdhomerun_video_stats_t *stats)
{
	if (!hd->vs) {
		hdhomerun_debug_printf(hd->dbg, "hdhomerun_device_stream_flush: video not initialized\n");
		memset(stats, 0, sizeof(struct hdhomerun_video_stats_t));
		return;
	}

	hdhomerun_video_get_stats(hd->vs, stats);
}
