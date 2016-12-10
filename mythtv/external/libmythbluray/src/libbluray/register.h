/*
 * This file is part of libbluray
 * Copyright (C) 2010-2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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

#include "util/attributes.h"

#include <stdint.h>


#define BD_PSR_COUNT 128
#define BD_GPR_COUNT 4096

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
    PSR_OUTPUT_PREFER    = 21,
    PSR_3D_STATUS        = 22,
    PSR_DISPLAY_CAP      = 23,
    PSR_3D_CAP           = 24,
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


typedef struct bd_registers_s BD_REGISTERS;

/**
 *
 *  Initialize registers
 *
 * @return allocated BD_REGISTERS object with default values
 */
BD_PRIVATE BD_REGISTERS *bd_registers_init(void);

/**
 *
 *  Free BD_REGISTERS object
 *
 * @param registers  BD_REGISTERS object
 */
BD_PRIVATE void bd_registers_free(BD_REGISTERS *);


/*
 * GPR (general-purprose register) access
 */

/**
 *
 *  Read value of general-purprose register
 *
 * @param registers  BD_REGISTERS object
 * @param reg  register number
 * @return value stored in register, -1 on error (invalid register number)
 */
uint32_t bd_gpr_read(BD_REGISTERS *, int reg);

/**
 *
 *  Write to general-purprose register
 *
 * @param registers  BD_REGISTERS object
 * @param reg  register number
 * @param val  new value for register
 * @return 0 on success, -1 on error (invalid register number)
 */
int bd_gpr_write(BD_REGISTERS *, int reg, uint32_t val);


/*
 * PSR (player status / setting register) access
 */

/**
 *
 *  Read value of player status/setting register
 *
 * @param registers  BD_REGISTERS object
 * @param reg  register number
 * @return value stored in register, -1 on error (invalid register number)
 */
uint32_t bd_psr_read(BD_REGISTERS *, int reg);

/**
 *
 *  Write to player status register.
 *
 *  Writing to player setting registers will fail.
 *
 * @param registers  BD_REGISTERS object
 * @param reg  register number
 * @param val  new value for register
 * @return 0 on success, -1 on error (invalid register number)
 */
int bd_psr_write(BD_REGISTERS *, int reg, uint32_t val);

/**
 *
 *  Atomically change bits in player status register.
 *
 *  Replace selected bits of player status register.
 *  New value for PSR is (CURRENT_PSR_VALUE & ~mask) | (val & mask)
 *  Writing to player setting registers will fail.
 *
 * @param registers  BD_REGISTERS object
 * @param reg  register number
 * @param val  new value for register
 * @param mask  bit mask. bits to be written are set to 1.
 * @return 0 on success, -1 on error (invalid register number)
 */
BD_PRIVATE int bd_psr_write_bits(BD_REGISTERS *, int reg, uint32_t val, uint32_t mask);

/**
 *
 *  Write to any PSR, including player setting registers.
 *
 *  This should be called only by the application.
 *
 * @param registers  BD_REGISTERS object
 * @param reg  register number
 * @param val  new value for register
 * @return 0 on success, -1 on error (invalid register number)
 */
BD_PRIVATE int bd_psr_setting_write(BD_REGISTERS *, int reg, uint32_t val);

/**
 *
 *  Lock PSRs for atomic read-modify-write operation
 *
 * @param registers  BD_REGISTERS object
 */
BD_PRIVATE void bd_psr_lock(BD_REGISTERS *);

/**
 *
 *  Unlock PSRs
 *
 * @param registers  BD_REGISTERS object
 */
BD_PRIVATE void bd_psr_unlock(BD_REGISTERS *);

/**
 *
 *  Save player state
 *
 *  Copy values of registers 4-8 and 10-12 to backup registers 36-40 and 42-44.
 *
 * @param registers  BD_REGISTERS object
 */
BD_PRIVATE void bd_psr_save_state(BD_REGISTERS *);

/**
 *
 *  Restore player state
 *
 *  Restore registers 4-8 and 10-12 from backup registers 36-40 and 42-44.
 *  Initialize backup registers to default values.
 *
 * @param registers  BD_REGISTERS object
 */
BD_PRIVATE void bd_psr_restore_state(BD_REGISTERS *);

/**
 *
 *  Reset backup registers
 *
 *  Initialize backup registers 36-40 and 42-44 to default values.
 *
 * @param registers  BD_REGISTERS object
 */
BD_PRIVATE void bd_psr_reset_backup_registers(BD_REGISTERS *);


/*
 * Events when PSR value is changed
 */

/* event types */
#define BD_PSR_SAVE    1 /* backup player state. Single event, psr_idx and values undefined */
#define BD_PSR_WRITE   2 /* write, value unchanged */
#define BD_PSR_CHANGE  3 /* write, value changed */
#define BD_PSR_RESTORE 4 /* restore backup values */

/* event data */
typedef struct {
    int      ev_type; /* event type */

    int      psr_idx; /* register index */
    uint32_t old_val; /* old value of register */
    uint32_t new_val; /* new value of register */
} BD_PSR_EVENT;

/**
 *
 *  Register callback function
 *
 *  Function is called every time PSR value changes.
 *
 * @param registers  BD_REGISTERS object
 * @param callback  callback function pointer
 * @param handle  application-specific handle that is provided to callback function as first parameter
 */
void bd_psr_register_cb(BD_REGISTERS *, void (*callback)(void*,BD_PSR_EVENT*), void *cb_handle);

/**
 *
 *  Unregister callback function
 *
 * @param registers  BD_REGISTERS object
 * @param callback  callback function to unregister
 * @param handle  application-specific handle that was used when callback was registered
 */
void bd_psr_unregister_cb(BD_REGISTERS *, void (*callback)(void*,BD_PSR_EVENT*), void *cb_handle);

BD_PRIVATE void psr_init_3D(BD_REGISTERS *, int initial_mode);


/*
 * save / restore registers between playback sessions
 *
 * When state is restored, restore events will be generated and playback state is restored.
 */

BD_PRIVATE void registers_save(BD_REGISTERS *p, uint32_t *psr, uint32_t *gpr);
BD_PRIVATE void registers_restore(BD_REGISTERS *p, const uint32_t *psr, const uint32_t *gpr);

#endif /* _BD_REGISTER_H_ */
