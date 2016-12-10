/*
 * This file is part of libbluray
 * Copyright (C) 2014  VideoLAN
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

#if !defined(_BD_DISC_DEC_H_)
#define _BD_DISC_DEC_H_

/*
 * low-level stream decoding
 */

#include "util/attributes.h"

#include <stdint.h>

struct bd_file_s;
struct bd_enc_info;

typedef struct bd_file_s * (*file_openFp)(void *, const char *);

/* device to use */
struct dec_dev {
    void          *file_open_bdrom_handle;
    file_openFp   pf_file_open_bdrom;
    void          *file_open_vfs_handle;
    file_openFp   pf_file_open_vfs;
    const char    *root;   /* may be NULL if disc is not mounted */
    const char    *device; /* may be null if not reading from real device */
};

typedef struct bd_dec BD_DEC;

BD_PRIVATE BD_DEC *dec_init(struct dec_dev *dev,
                            struct bd_enc_info *enc_info,
                            const char *keyfile_path,
                            void *regs, void *psr_read, void *psr_write);
BD_PRIVATE void dec_close(BD_DEC **);

/* get decoder data */
BD_PRIVATE const uint8_t *dec_data(BD_DEC *, int type);

/* status events from upper layers */
BD_PRIVATE void dec_start(BD_DEC *, uint32_t num_titles);
BD_PRIVATE void dec_title(BD_DEC *, uint32_t title);
BD_PRIVATE void dec_application(BD_DEC *, uint32_t data);

/* open low-level stream */
BD_PRIVATE struct bd_file_s *dec_open_stream(BD_DEC *dec, struct bd_file_s *fp, uint32_t clip_id);


#endif /* _BD_DISC_DEC_H_ */
