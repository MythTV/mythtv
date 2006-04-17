/*
 * hdhomerun_discover.h
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

struct hdhomerun_discover_sock_t;

struct hdhomerun_discover_device_t {
	unsigned long ip_addr;
	unsigned long device_type;
	unsigned long device_id;
};

extern struct hdhomerun_discover_sock_t *hdhomerun_discover_create(void);
extern void hdhomerun_discover_destroy(struct hdhomerun_discover_sock_t *ds);
extern int hdhomerun_discover_send(struct hdhomerun_discover_sock_t *ds, unsigned long device_type, unsigned long device_id);
extern int hdhomerun_discover_recv(struct hdhomerun_discover_sock_t *ds, struct hdhomerun_discover_device_t *result, unsigned long timeout);

#ifdef __cplusplus
}
#endif
