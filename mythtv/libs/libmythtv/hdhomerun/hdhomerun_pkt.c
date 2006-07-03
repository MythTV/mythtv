/*
 * hdhomerun_pkt.c
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

#include <string.h>
#include "hdhomerun_pkt.h"

unsigned char hdhomerun_read_u8(unsigned char **pptr)
{
	unsigned char *ptr = *pptr;
	unsigned char v = *ptr++;
	*pptr = ptr;
	return v;
}

unsigned short hdhomerun_read_u16(unsigned char **pptr)
{
	unsigned char *ptr = *pptr;
	unsigned short v;
	v =  (unsigned short)*ptr++ << 8;
	v |= (unsigned short)*ptr++ << 0;
	*pptr = ptr;
	return v;
}

unsigned long hdhomerun_read_u32(unsigned char **pptr)
{
	unsigned char *ptr = *pptr;
	unsigned long v;
	v =  (unsigned long)*ptr++ << 24;
	v |= (unsigned long)*ptr++ << 16;
	v |= (unsigned long)*ptr++ << 8;
	v |= (unsigned long)*ptr++ << 0;
	*pptr = ptr;
	return v;
}

static int hdhomerun_read_var_length(unsigned char **pptr, unsigned char *end)
{
	unsigned char *ptr = *pptr;
	unsigned int length;
	
	if (ptr + 1 > end) {
		return -1;
	}

	length = (unsigned int)*ptr++;
	if (length & 0x0080) {
		if (ptr + 1 > end) {
			return -1;
		}

		length &= 0x007F;
		length |= (unsigned int)*ptr++ << 7;
	}
	
	*pptr = ptr;
	return (int)length; 
}

int hdhomerun_read_tlv(unsigned char **pptr, unsigned char *end, unsigned char *ptag, int *plength, unsigned char **pvalue)
{
	if (end - *pptr < 2) {
		return -1;
	}
	
	*ptag = hdhomerun_read_u8(pptr);
	*plength = hdhomerun_read_var_length(pptr, end);
	*pvalue = *pptr;
	
	if (*plength < 0) {
		return -1;
	}
	if (end - *pptr < *plength) {
		return -1;
	}
	
	*pptr += *plength;
	return 0;
}

void hdhomerun_write_u8(unsigned char **pptr, unsigned char v)
{
	unsigned char *ptr = *pptr;
	*ptr++ = v;
	*pptr = ptr;
}

void hdhomerun_write_u16(unsigned char **pptr, unsigned short v)
{
	unsigned char *ptr = *pptr;
	*ptr++ = (v >> 8) & 0xFF;
	*ptr++ = (v >> 0) & 0xFF;
	*pptr = ptr;
}

void hdhomerun_write_u32(unsigned char **pptr, unsigned long v)
{
	unsigned char *ptr = *pptr;
	*ptr++ = (v >> 24) & 0xFF;
	*ptr++ = (v >> 16) & 0xFF;
	*ptr++ = (v >> 8) & 0xFF;
	*ptr++ = (v >> 0) & 0xFF;
	*pptr = ptr;
}

void hdhomerun_write_var_length(unsigned char **pptr, int v)
{
	unsigned char *ptr = *pptr;
	if (v <= 127) {
		*ptr++ = v;
	} else {
		*ptr++ = v | 0x80;
		*ptr++ = v >> 7;
	}
	*pptr = ptr;
}

static void hdhomerun_write_mem(unsigned char **pptr, void *mem, int length)
{
	unsigned char *ptr = *pptr;
	memcpy(ptr, mem, length);
	ptr += length;
	*pptr = ptr;
}

static unsigned long hdhomerun_calc_crc(unsigned char *start, unsigned char *end)
{
	unsigned char *ptr = start;
	unsigned long crc = 0xFFFFFFFF;
	while (ptr < end) {
	unsigned char x = (crc & 0x000000FF) ^ *ptr++;
	crc >>= 8;
		if (x & 0x01) crc ^= 0x77073096;
		if (x & 0x02) crc ^= 0xEE0E612C;
		if (x & 0x04) crc ^= 0x076DC419;
		if (x & 0x08) crc ^= 0x0EDB8832;
		if (x & 0x10) crc ^= 0x1DB71064;
		if (x & 0x20) crc ^= 0x3B6E20C8;
		if (x & 0x40) crc ^= 0x76DC4190;
		if (x & 0x80) crc ^= 0xEDB88320;
	}
	return crc ^ 0xFFFFFFFF;
}

static int hdhomerun_check_crc(unsigned char *start, unsigned char *end)
{
	if (end - start < 8) {
		return -1;
	}
	unsigned char *ptr = end -= 4;
	unsigned long actual_crc = hdhomerun_calc_crc(start, ptr);
	unsigned long packet_crc;
	packet_crc =  (unsigned long)*ptr++ << 0;
	packet_crc |= (unsigned long)*ptr++ << 8;
	packet_crc |= (unsigned long)*ptr++ << 16;
	packet_crc |= (unsigned long)*ptr++ << 24;
	if (actual_crc != packet_crc) {
		return -1;
	}
	return 0;
}

static void hdhomerun_write_header_length(unsigned char *ptr, unsigned long length)
{
	hdhomerun_write_u16(&ptr, length);
}

static void hdhomerun_write_crc(unsigned char **pptr, unsigned char *start)
{
	unsigned char *ptr = *pptr;
	unsigned long crc = hdhomerun_calc_crc(start, ptr);
	*ptr++ = (crc >> 0) & 0xFF;
	*ptr++ = (crc >> 8) & 0xFF;
	*ptr++ = (crc >> 16) & 0xFF;
	*ptr++ = (crc >> 24) & 0xFF;
	*pptr = ptr;
}

void hdhomerun_write_discover_request(unsigned char **pptr, unsigned long device_type, unsigned long device_id)
{
	unsigned char *start = *pptr;
	hdhomerun_write_u16(pptr, HDHOMERUN_TYPE_DISCOVER_REQ);
	hdhomerun_write_u16(pptr, 0);

	hdhomerun_write_u8(pptr, HDHOMERUN_TAG_DEVICE_TYPE);
	hdhomerun_write_var_length(pptr, 4);
	hdhomerun_write_u32(pptr, device_type);
	hdhomerun_write_u8(pptr, HDHOMERUN_TAG_DEVICE_ID);
	hdhomerun_write_var_length(pptr, 4);
	hdhomerun_write_u32(pptr, device_id);

	hdhomerun_write_header_length(start + 2, *pptr - start - 4);
	hdhomerun_write_crc(pptr, start);
}

void hdhomerun_write_get_request(unsigned char **pptr, const char *name)
{
	unsigned char *start = *pptr;
	hdhomerun_write_u16(pptr, HDHOMERUN_TYPE_GETSET_REQ);
	hdhomerun_write_u16(pptr, 0);

	int name_len = strlen(name) + 1;
	hdhomerun_write_u8(pptr, HDHOMERUN_TAG_GETSET_NAME);
	hdhomerun_write_var_length(pptr, name_len);
	hdhomerun_write_mem(pptr, (void *)name, name_len);

	hdhomerun_write_header_length(start + 2, *pptr - start - 4);
	hdhomerun_write_crc(pptr, start);
}

void hdhomerun_write_set_request(unsigned char **pptr, const char *name, const char *value)
{
	unsigned char *start = *pptr;
	hdhomerun_write_u16(pptr, HDHOMERUN_TYPE_GETSET_REQ);
	hdhomerun_write_u16(pptr, 0);

	int name_len = strlen(name) + 1;
	int value_len = strlen(value) + 1;
	hdhomerun_write_u8(pptr, HDHOMERUN_TAG_GETSET_NAME);
	hdhomerun_write_var_length(pptr, name_len);
	hdhomerun_write_mem(pptr, (void *)name, name_len);
	hdhomerun_write_u8(pptr, HDHOMERUN_TAG_GETSET_VALUE);
	hdhomerun_write_var_length(pptr, value_len);
	hdhomerun_write_mem(pptr, (void *)value, value_len);

	hdhomerun_write_header_length(start + 2, *pptr - start - 4);
	hdhomerun_write_crc(pptr, start);
}

void hdhomerun_write_upgrade_request(unsigned char **pptr, unsigned long sequence, void *data, int length)
{
	unsigned char *start = *pptr;
	hdhomerun_write_u16(pptr, HDHOMERUN_TYPE_UPGRADE_REQ);
	hdhomerun_write_u16(pptr, 0);

	hdhomerun_write_u32(pptr, sequence);
	if (length > 0) {
		hdhomerun_write_mem(pptr, data, length);
	}

	hdhomerun_write_header_length(start + 2, *pptr - start - 4);
	hdhomerun_write_crc(pptr, start);
}

int hdhomerun_peek_packet_length(unsigned char *ptr)
{
	ptr += 2;
	return hdhomerun_read_u16(&ptr) + 8;
}

int hdhomerun_process_packet(unsigned char **pptr, unsigned char **pend)
{
	if (hdhomerun_check_crc(*pptr, *pend) < 0) {
		return -1;
	}
	*pend -= 4;
	
	unsigned short type = hdhomerun_read_u16(pptr);
	unsigned short length = hdhomerun_read_u16(pptr);
	if ((*pend - *pptr) < length) {
		return -1;
	}
	*pend = *pptr + length;
	return (int)type;
}
