/*
 * hdhomerun_pkt.h
 *
 * Copyright © 2005-2006 Silicondust Engineering Ltd. <www.silicondust.com>.
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

#define HDHOMERUN_DISCOVER_UDP_PORT 65001
#define HDHOMERUN_CONTROL_TCP_PORT 65001

#define HDHOMERUN_TYPE_DISCOVER_REQ 0x0002
#define HDHOMERUN_TYPE_DISCOVER_RPY 0x0003
#define HDHOMERUN_TYPE_GETSET_REQ 0x0004
#define HDHOMERUN_TYPE_GETSET_RPY 0x0005
#define HDHOMERUN_TYPE_UPGRADE_REQ 0x0006
#define HDHOMERUN_TYPE_UPGRADE_RPY 0x0007

#define HDHOMERUN_TAG_DEVICE_TYPE 0x01
#define HDHOMERUN_TAG_DEVICE_ID 0x02
#define HDHOMERUN_TAG_GETSET_NAME 0x03
#define HDHOMERUN_TAG_GETSET_VALUE 0x04
#define HDHOMERUN_TAG_ERROR_MESSAGE 0x05

#define HDHOMERUN_DEVICE_TYPE_WILDCARD 0xFFFFFFFF
#define HDHOMERUN_DEVICE_TYPE_TUNER 0x00000001
#define HDHOMERUN_DEVICE_ID_WILDCARD 0xFFFFFFFF

#define HDHOMERUN_MIN_PEEK_LENGTH 4

extern unsigned char hdhomerun_read_u8(unsigned char **pptr);
extern unsigned short hdhomerun_read_u16(unsigned char **pptr);
extern unsigned long hdhomerun_read_u32(unsigned char **pptr);
extern void hdhomerun_write_u8(unsigned char **pptr, unsigned char v);
extern void hdhomerun_write_u16(unsigned char **pptr, unsigned short v);
extern void hdhomerun_write_u32(unsigned char **pptr, unsigned long v);

extern int hdhomerun_peek_packet_length(unsigned char *ptr);
extern int hdhomerun_process_packet(unsigned char **pptr, unsigned char **pend);
extern int hdhomerun_read_tlv(unsigned char **pptr, unsigned char *end, unsigned char *ptag, unsigned char *plength, unsigned char **pvalue);

extern void hdhomerun_write_discover_request(unsigned char **pptr, unsigned long device_type, unsigned long device_id);
extern void hdhomerun_write_get_request(unsigned char **pptr, const char *name);
extern void hdhomerun_write_set_request(unsigned char **pptr, const char *name, const char *value);
extern void hdhomerun_write_upgrade_request(unsigned char **pptr, unsigned long sequence, void *data, int length);

#ifdef __cplusplus
}
#endif
