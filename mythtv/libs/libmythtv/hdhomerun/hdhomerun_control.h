/*
 * hdhomerun_control.h
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

struct hdhomerun_control_sock_t;

struct hdhomerun_control_data_t {
	int type;
	unsigned char buffer[1024];
	unsigned char *ptr;
	unsigned char *end;
};

extern struct hdhomerun_control_sock_t *hdhomerun_control_create(unsigned long ip_addr, unsigned long timeout);
extern void hdhomerun_control_destroy(struct hdhomerun_control_sock_t *cs);
extern unsigned long hdhomerun_control_get_local_addr(struct hdhomerun_control_sock_t *cs);

extern int hdhomerun_control_send(struct hdhomerun_control_sock_t *cs, unsigned char *start, unsigned char *end);
extern int hdhomerun_control_send_get_request(struct hdhomerun_control_sock_t *cs, const char *name);
extern int hdhomerun_control_send_set_request(struct hdhomerun_control_sock_t *cs, const char *name, const char *value);

extern int hdhomerun_control_recv(struct hdhomerun_control_sock_t *cs, struct hdhomerun_control_data_t *result, unsigned long timeout);

#ifdef __cplusplus
}
#endif
