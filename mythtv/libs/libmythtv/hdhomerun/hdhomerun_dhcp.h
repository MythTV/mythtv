/*
 * hdhomerun_dhcp.h
 *
 * Copyright © 2006 Silicondust Engineering Ltd. <www.silicondust.com>.
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
 */

#ifdef __cplusplus
extern "C" {
#endif

struct hdhomerun_dhcp_t;

extern LIBTYPE struct hdhomerun_dhcp_t *hdhomerun_dhcp_create(uint32_t bind_address);
extern LIBTYPE void hdhomerun_dhcp_destroy(struct hdhomerun_dhcp_t *dhcp);

#ifdef __cplusplus
}
#endif
