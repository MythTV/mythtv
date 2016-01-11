/*
 * This file is part of libudfread
 * Copyright (C) 2014-2015 VLC authors and VideoLAN
 *
 * Authors: Petri Hintukainen <phintuka@users.sourceforge.net>
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "ecma167.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#define ecma_error(...)   fprintf(stderr, "ecma: " __VA_ARGS__)

/*
 * Part 1: General
 */

/* fixed-length dstring, ECMA 1/7.2.12 */
static int _decode_dstring(const uint8_t *p, int field_length, uint8_t *str)
{
    int length = _get_u8(p + field_length - 1);
    if (length > field_length - 1) {
        length = field_length - 1;
    }
    memcpy(str, p, length);
    return length;
}

/* Extent Descriptor (ECMA 167, 3/7.1) */
static void _decode_extent_ad(const uint8_t *p, struct extent_ad *ad)
{
    ad->length = _get_u32(p + 0);
    ad->lba    = _get_u32(p + 4);
}

/* Entity Identifier (ECMA 167, 1/7.4) */
void decode_entity_id(const uint8_t *p, struct entity_id *eid)
{
    memcpy(eid->identifier,        p + 1,  23);
    memcpy(eid->identifier_suffix, p + 24, 8);
}

/*
 * Part 3: Volume Structure
 */

#define AD_LENGTH_MASK    0x3fffffff

/* Descriptor Tag (ECMA 167, 3/7.2) */
enum tag_identifier decode_descriptor_tag(const uint8_t *buf)
{
  uint16_t id;
  uint8_t  checksum = 0;
  int      i;

  id = _get_u16(buf + 0);

  /* descriptor tag is 16 bytes */

  /* calculate tag checksum */
  for (i = 0; i < 4; i++) {
      checksum += buf[i];
  }
  for (i = 5; i < 16; i++) {
      checksum += buf[i];
  }

  if (checksum != buf[4]) {
      return ECMA_TAG_NONE;
  }

  return (enum tag_identifier)id;
}

/* Primary Volume Descriptor (ECMA 167, 3/10.1) */
void decode_primary_volume(const uint8_t *p, struct primary_volume_descriptor *pvd)
{
    pvd->volume_identifier_length = _decode_dstring(p + 24, 32, pvd->volume_identifier);

    memcpy(pvd->volume_set_identifier, p + 72, 128);
}

/* Anchor Volume Description Pointer (ECMA 167 3/10.2) */
void decode_avdp(const uint8_t *p, struct anchor_volume_descriptor *avdp)
{
    /* Main volume descriptor sequence extent */
    _decode_extent_ad(p + 16, &avdp->mvds);

    /* Reserve (backup) volume descriptor sequence extent */
    _decode_extent_ad(p + 24, &avdp->rvds);
}

/* Partition Descriptor (ECMA 167 3/10.5) */
void decode_partition(const uint8_t *p, struct partition_descriptor *pd)
{
    pd->number      = _get_u16(p + 22);
    pd->start_block = _get_u32(p + 188);
    pd->num_blocks  = _get_u32(p + 192);
}

/* Logical Volume Descriptor (ECMA 167 3/10.6) */
void decode_logical_volume(const uint8_t *p, struct logical_volume_descriptor *lvd)
{
    lvd->block_size = _get_u32(p + 212);

    decode_entity_id(p + 216, &lvd->domain_id);

    memcpy(lvd->contents_use, p + 248, 16);

    lvd->partition_map_lable_length = _get_u32(p + 264);
    lvd->num_partition_maps         = _get_u32(p + 268);

    /* XXX cut long maps */
    uint32_t map_size = lvd->partition_map_lable_length;
    if (map_size > sizeof(lvd->partition_map_table)) {
        map_size = sizeof(lvd->partition_map_table);
    }

    memcpy(lvd->partition_map_table, p + 440, map_size);
}

/*
 * Part 4: File Structure
 */

/* File Set Descriptor (ECMA 167 4/14.1) */
void decode_file_set_descriptor(const uint8_t *p, struct file_set_descriptor *fsd)
{
    decode_long_ad(p + 400, &fsd->root_icb);
}

/* File Identifier (ECMA 167 4/14.4) */
size_t decode_file_identifier(const uint8_t *p, struct file_identifier *fi)
{
    uint16_t       l_iu; /* length of implementation use field */

    fi->characteristic = _get_u8(p + 18);
    fi->filename_len   = _get_u8(p + 19);
    decode_long_ad(p + 20, &fi->icb);

    l_iu = _get_u16(p + 36);

    if (fi->filename_len) {
        memcpy(fi->filename, p + 38 + l_iu, fi->filename_len);
    }
    fi->filename[fi->filename_len] = 0;

    /* ECMA 167, 4/14.4
     * padding size 4 * ip((L_FI+L_IU+38+3)/4) - (L_FI+L_IU+38)
     * => padded to dwords
     */
    return 4 * ((38 + fi->filename_len + l_iu + 3) / 4);
}

/* ICB Tag (ECMA 167 4/14.6) */

struct icb_tag {
    uint8_t  file_type;
    uint16_t strategy_type;
    uint16_t flags;
};

static void _decode_icb_tag(const uint8_t *p, struct icb_tag *tag)
{
    tag->strategy_type = _get_u16(p + 4);
    tag->file_type     = _get_u8 (p + 11);
    tag->flags         = _get_u16(p + 18);
}

/* Short Allocation Descriptor (ECMA 167, 4/14.14.1) */
static void _decode_short_ad(const uint8_t *buf, uint16_t partition, struct long_ad *ad)
{
    ad->length    = _get_u32(buf + 0) & AD_LENGTH_MASK;
    ad->lba       = _get_u32(buf + 4);
    ad->partition = partition;
}

/* Long Allocation Descriptor (ECMA 167, 4/14.14.2) */
void decode_long_ad(const uint8_t *buf, struct long_ad *ad)
{
    ad->length    = _get_u32(buf + 0) & AD_LENGTH_MASK;
    ad->lba       = _get_u32(buf + 4);
    ad->partition = _get_u16(buf + 8);
}

/* Exrtended Allocation Descriptor (ECMA 167, 4/14.14.3) */
static void _decode_extended_ad(const uint8_t *buf, struct long_ad *ad)
{
    ad->length    = _get_u32(buf + 0) & AD_LENGTH_MASK;
    ad->lba       = _get_u32(buf + 12);
    ad->partition = _get_u16(buf + 16);
}

/* File Entry */

static void _decode_file_ads(const uint8_t *p, int flags, uint16_t partition, struct file_entry *fe)
{
    uint32_t i;

    flags &= 7;
    for (i = 0; i < fe->num_ad; i++) {
        switch (flags) {
        case 0:
            _decode_short_ad(p, partition, &fe->data.ad[i]);
            p += 8;
            break;
        case 1:
            decode_long_ad(p, &fe->data.ad[i]);
            p += 16;
            break;
        case 2:
            _decode_extended_ad(p, &fe->data.ad[i]);
            p += 20;
            break;
        }
    }
}

static struct file_entry *_decode_file_entry(const uint8_t *p, size_t size,
                                             uint16_t partition, uint32_t l_ad, uint32_t p_ad)
{
    struct file_entry *fe;
    struct icb_tag     tag;
    int                num_ad;
    int                content_inline = 0;

    if (p_ad + l_ad > size) {
        ecma_error("not enough data in file entry\n");
        return NULL;
    }

    _decode_icb_tag(p + 16, &tag);
    if (tag.strategy_type != 4) {
        /* UDF (2.): only ICB strategy types 4 and 4096 shall be recorded */
        ecma_error("unsupported icb strategy type %d\n", tag.strategy_type);
        return NULL;
    }

    switch (tag.flags & 7) {
        case 0: num_ad = l_ad / 8;  break;
        case 1: num_ad = l_ad / 16; break;
        case 2: num_ad = l_ad / 20; break;
        case 3:
            num_ad = 0;
            content_inline = 1;
            break;
        default:
            ecma_error("unsupported icb flags: 0x%x\n", tag.flags);
            return NULL;
    }

    if (content_inline) {
        fe = (struct file_entry *)calloc(1, sizeof(struct file_entry) + l_ad);
    } else {
        fe = (struct file_entry *)calloc(1, sizeof(struct file_entry) + sizeof(struct long_ad) * (num_ad - 1));
    }

    if (!fe) {
        return NULL;
    }

    fe->file_type = tag.file_type;
    fe->length    = _get_u64(p + 56);
    fe->num_ad    = num_ad;

    if (content_inline) {
        /* data of small files can be embedded in file entry */
        /* copy embedded file data */
        fe->content_inline = 1;
        memcpy(fe->data.content, p + p_ad, l_ad);
    } else {
        _decode_file_ads(p + p_ad, tag.flags, partition, fe);
    }

    return fe;
}

/* File Entry (ECMA 167, 4/14.9) */
struct file_entry *decode_file_entry(const uint8_t *p, size_t size, uint16_t partition)
{
    uint32_t l_ea, l_ad;

    l_ea = _get_u32(p + 168);
    l_ad = _get_u32(p + 172);

    /* check for integer overflow */
    if ((uint64_t)l_ea + (uint64_t)l_ad + (uint64_t)176 >= (uint64_t)1<<32) {
        ecma_error("invalid file entry\n");
        return NULL;
    }

    return _decode_file_entry(p, size, partition, l_ad, 176 + l_ea);
}

/* Extended File Entry (ECMA 167, 4/14.17) */
struct file_entry *decode_ext_file_entry(const uint8_t *p, size_t size, uint16_t partition)
{
    uint32_t l_ea, l_ad;

    l_ea = _get_u32(p + 208);
    l_ad = _get_u32(p + 212);

    /* check for integer overflow */
    if ((uint64_t)l_ea + (uint64_t)l_ad + (uint64_t)216 >= (uint64_t)1<<32) {
        ecma_error("invalid file entry\n");
        return NULL;
    }

    return _decode_file_entry(p, size, partition, l_ad, 216 + l_ea);
}

void free_file_entry(struct file_entry **p_fe)
{
    if (p_fe) {
        free(*p_fe);
        *p_fe = NULL;
    }
}
