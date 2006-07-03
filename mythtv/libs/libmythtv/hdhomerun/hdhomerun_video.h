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

#define VIDEO_DATA_PACKET_SIZE (188 * 7)
#define VIDEO_DATA_BUFFER_SIZE_1S (20000000 / 8)

extern struct hdhomerun_video_sock_t *hdhomerun_video_create(unsigned long buffer_size, unsigned long timeout);
extern void hdhomerun_video_destroy(struct hdhomerun_video_sock_t *vs);
extern unsigned short hdhomerun_video_get_local_port(struct hdhomerun_video_sock_t *vs);
extern int hdhomerun_video_get_state(struct hdhomerun_video_sock_t *vs);
extern unsigned long hdhomerun_video_recv_memcpy(struct hdhomerun_video_sock_t *vs, unsigned char *buffer, unsigned long size);
extern unsigned char *hdhomerun_video_recv_inplace(struct hdhomerun_video_sock_t *vs, unsigned long max_size, unsigned long *pactual_size);
extern void hdhomerun_video_flush(struct hdhomerun_video_sock_t *vs);

#ifdef __cplusplus
}
#endif
