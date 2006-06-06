/*
 * hdhomerun_video.h
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
#ifdef __cplusplus
extern "C" {
#endif

struct hdhomerun_video_sock_t;

#define VIDEO_DATA_DMESG_SIZE  188*7
#define VIDEO_DATA_DMESG_COUNT 100

struct hdhomerun_video_data_t {
	int length;
	unsigned char buffer[VIDEO_DATA_DMESG_SIZE * VIDEO_DATA_DMESG_COUNT];
};

extern struct hdhomerun_video_sock_t *hdhomerun_video_create(void);
extern void hdhomerun_video_destroy(struct hdhomerun_video_sock_t *vs);
extern unsigned short hdhomerun_video_get_local_port(struct hdhomerun_video_sock_t *vs);
extern int hdhomerun_video_recv(struct hdhomerun_video_sock_t *vs, struct hdhomerun_video_data_t *result, unsigned long timeout);

#ifdef __cplusplus
}
#endif
