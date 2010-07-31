/*
 * This file is part of libbluray
 * Copyright (C) 2010  hpi1
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

#if !defined(_BD_REGISTER_H_)
#define _BD_REGISTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <util/attributes.h>

#include <stdint.h>

/*
 * Player Status Registers
 */

typedef enum {
    PSR_IG_STREAM_ID     = 0,
    PSR_PRIMARY_AUDIO_ID = 1,
    PSR_PG_STREAM        = 2, /* PG TextST and PIP PG TextST stream number */
    PSR_ANGLE_NUMBER     = 3, /* 1..N */
    PSR_TITLE_NUMBER     = 4, /* 1..N  (0 = top menu, 0xffff = first play) */
    PSR_CHAPTER          = 5, /* 1..N  (0xffff = invalid) */
    PSR_PLAYLIST         = 6, /* playlist file name number */
    PSR_PLAYITEM         = 7, /* 0..N-1 (playitem_id) */
    PSR_TIME             = 8, /* presetation time */
    PSR_NAV_TIMER        = 9,
    PSR_SELECTED_BUTTON_ID = 10,
    PSR_MENU_PAGE_ID     = 11,
    PSR_STYLE            = 12,
    PSR_PARENTAL         = 13,
    PSR_SECONDARY_AUDIO_VIDEO = 14,
    PSR_AUDIO_CAP        = 15,
    PSR_AUDIO_LANG       = 16,
    PSR_PG_AND_SUB_LANG  = 17,
    PSR_MENU_LANG        = 18,
    PSR_COUNTRY          = 19,
    PSR_REGION           = 20,
    PSR_VIDEO_CAP        = 29,
    PSR_TEXT_CAP         = 30, /* text subtitles */
    PSR_PROFILE_VERSION  = 31, /* player profile and version */
    PSR_BACKUP_PSR4      = 36,
    PSR_BACKUP_PSR5      = 37,
    PSR_BACKUP_PSR6      = 38,
    PSR_BACKUP_PSR7      = 39,
    PSR_BACKUP_PSR8      = 40,
    PSR_BACKUP_PSR10     = 42,
    PSR_BACKUP_PSR11     = 43,
    PSR_BACKUP_PSR12     = 44,
    /* 48-61: caps for characteristic text subtitle */
} bd_psr_idx;

/*
 *
 */

typedef struct bd_registers_s BD_REGISTERS;

BD_PRIVATE BD_REGISTERS *bd_registers_init(void);
BD_PRIVATE void          bd_registers_free(BD_REGISTERS *);

int      bd_psr_setting_write(BD_REGISTERS *, int reg, uint32_t val);
int      bd_psr_write(BD_REGISTERS *, int reg, uint32_t val);
uint32_t bd_psr_read(BD_REGISTERS *, int reg);

void     bd_psr_save_state(BD_REGISTERS *);
void     bd_psr_restore_state(BD_REGISTERS *);

void     bd_psr_lock(BD_REGISTERS *);
void     bd_psr_unlock(BD_REGISTERS *);

int      bd_gpr_write(BD_REGISTERS *, int reg, uint32_t val);
uint32_t bd_gpr_read(BD_REGISTERS *, int reg);

/*
 * Events when PSR value is changed
 */

#define BD_PSR_CHANGE  1
#define BD_PSR_RESTORE 2

typedef struct {
    int      ev_type;

    int      psr_idx;
    uint32_t old_val;
    uint32_t new_val;
} BD_PSR_EVENT;

void bd_psr_register_cb  (BD_REGISTERS *, void (*callback)(void*,BD_PSR_EVENT*), void *cb_handle);
void bd_psr_unregister_cb(BD_REGISTERS *, void (*callback)(void*,BD_PSR_EVENT*), void *cb_handle);

#ifdef __cplusplus
};
#endif

#endif /* _BD_REGISTER_H_ */
