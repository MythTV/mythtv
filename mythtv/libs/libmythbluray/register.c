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

#include "register.h"
#include "util/attributes.h"
#include "util/macro.h"
#include "util/logging.h"
#include "util/mutex.h"

#include <stdlib.h>
#include <string.h>

#define BD_PSR_COUNT 128
#define BD_GPR_COUNT 4096

/*
 * Initial values for player status/setting registers (5.8.2).
 *
 * PS in comment indicates player setting -> register can't be changed from movie object code.
 */

static const uint32_t bd_psr_init[BD_PSR_COUNT] = {
    1,           /*     PSR0:  Interactive graphics stream number */
    0xff,        /*     PSR1:  Primary audio stream number */
    0x0fff0fff,  /*     PSR2:  PG TextST stream number and PiP PG stream number*/
    1,           /*     PSR3:  Angle number */
    0xffff,      /*     PSR4:  Title number */
    0xffff,      /*     PSR5:  Chapter number */
    0,           /*     PSR6:  PlayList ID */
    0,           /*     PSR7:  PlayItem ID */
    0,           /*     PSR8:  Presentation time */
    0,           /*     PSR9:  Navigation timer */
    0xffff,      /*     PSR10: Selected button ID */
    0,           /*     PSR11: Page ID */
    0xff,        /*     PSR12: User style number */
    0xff,        /* PS: PSR13: User age */
    0xffff,      /*     PSR14: Secondary audio stream number and secondary video stream number */
    0xffff,      /* PS: PSR15: player capability for audio */
    0xffffff,    /* PS: PSR16: Language code for audio */
    0xffffff,    /* PS: PSR17: Language code for PG and Text subtitles */
    0xffffff,    /* PS: PSR18: Menu description language code */
    0xffff,      /* PS: PSR19: Country code */
    0x07,        /* PS: PSR20: Region code */ /* 1 - A, 2 - B, 4 - C */
    0,           /*     PSR21 */
    0,           /*     PSR22 */
    0,           /*     PSR23 */
    0,           /*     PSR24 */
    0,           /*     PSR25 */
    0,           /*     PSR26 */
    0,           /*     PSR27 */
    0,           /*     PSR28 */
    0x03,        /* PS: PSR29: player capability for video */
    0x1ffff,     /* PS: PSR30: player capability for text subtitle */
    0x080200,    /* PS: PSR31: Player profile and version */
    0,           /*     PSR32 */
    0,           /*     PSR33 */
    0,           /*     PSR34 */
    0,           /*     PSR35 */
    0xffff,      /*     PSR36: backup PSR4 */
    0xffff,      /*     PSR37: backup PSR5 */
    0,           /*     PSR38: backup PSR6 */
    0,           /*     PSR39: backup PSR7 */
    0,           /*     PSR40: backup PSR8 */
    0,           /*     PSR41: */
    0xffff,      /*     PSR42: backup PSR10 */
    0,           /*     PSR43: backup PSR11 */
    0xff,        /*     PSR44: backup PSR12 */
    0,           /*     PSR45: */
    0,           /*     PSR46: */
    0,           /*     PSR47: */
    0xffffffff,  /* PS: PSR48: Characteristic text caps */
    0xffffffff,  /* PS: PSR49: Characteristic text caps */
    0xffffffff,  /* PS: PSR50: Characteristic text caps */
    0xffffffff,  /* PS: PSR51: Characteristic text caps */
    0xffffffff,  /* PS: PSR52: Characteristic text caps */
    0xffffffff,  /* PS: PSR53: Characteristic text caps */
    0xffffffff,  /* PS: PSR54: Characteristic text caps */
    0xffffffff,  /* PS: PSR55: Characteristic text caps */
    0xffffffff,  /* PS: PSR56: Characteristic text caps */
    0xffffffff,  /* PS: PSR57: Characteristic text caps */
    0xffffffff,  /* PS: PSR58: Characteristic text caps */
    0xffffffff,  /* PS: PSR59: Characteristic text caps */
    0xffffffff,  /* PS: PSR60: Characteristic text caps */
    0xffffffff,  /* PS: PSR61: Characteristic text caps */
    /* 62-95:   reserved */
    /* 96-111:  reserved for BD system use */
    /* 112-127: reserved */
};

/*
 * PSR ids for debugging
 */
static const char * const bd_psr_name[BD_PSR_COUNT] = {
    "IG_STREAM_ID",
    "PRIMARY_AUDIO_ID",
    "PG_STREAM",
    "ANGLE_NUMBER",
    "TITLE_NUMBER",
    "CHAPTER",
    "PLAYLIST",
    "PLAYITEM",
    "TIME",
    "NAV_TIMER",
    "SELECTED_BUTTON_ID",
    "MENU_PAGE_ID",
    "STYLE",
    "PARENTAL",
    "SECONDARY_AUDIO_VIDEO",
    "AUDIO_CAP",
    "AUDIO_LANG",
    "PG_AND_SUB_LANG",
    "PSR_MENU_LANG",
    "PSR_COUNTRY",
    "PSR_REGION",
    "PSR_VIDEO_CAP",
};

/*
 * data
 */

typedef struct {
    void *handle;
    void (*cb)(void *, BD_PSR_EVENT*);
} PSR_CB_DATA;

struct bd_registers_s
{
    uint32_t     psr[BD_PSR_COUNT];
    uint32_t     gpr[BD_GPR_COUNT];

    /* callbacks */
    unsigned     num_cb;
    PSR_CB_DATA *cb;

    BD_MUTEX     mutex;
};

/*
 * init / free
 */

BD_REGISTERS *bd_registers_init(void)
{
    BD_REGISTERS *p = calloc(1, sizeof(BD_REGISTERS));

    memcpy(p->psr, bd_psr_init, sizeof(bd_psr_init));

    bd_mutex_init(&p->mutex);

    return p;
}

void bd_registers_free(BD_REGISTERS *p)
{
    if (p) {
        bd_mutex_destroy(&p->mutex);

        X_FREE(p->cb);
    }

    X_FREE(p);
}

/*
 * PSR lock / unlock
 */

void bd_psr_lock(BD_REGISTERS *p)
{
    bd_mutex_lock(&p->mutex);
}

void bd_psr_unlock(BD_REGISTERS *p)
{
    bd_mutex_unlock(&p->mutex);
}

/*
 * PSR change callback register / unregister
 */

void bd_psr_register_cb  (BD_REGISTERS *p, void (*callback)(void*,BD_PSR_EVENT*), void *cb_handle)
{
    /* no duplicates ! */
    unsigned i;

    bd_psr_lock(p);

    for (i = 0; i < p->num_cb; i++) {
        if (p->cb[i].handle == cb_handle && p->cb[i].cb == callback) {

            bd_psr_unlock(p);
            return;
        }
    }

    p->num_cb++;
    p->cb = realloc(p->cb, p->num_cb * sizeof(PSR_CB_DATA));

    p->cb[p->num_cb - 1].cb     = callback;
    p->cb[p->num_cb - 1].handle = cb_handle;

    bd_psr_unlock(p);
}

void bd_psr_unregister_cb(BD_REGISTERS *p, void (*callback)(void*,BD_PSR_EVENT*), void *cb_handle)
{
    unsigned i = 0;

    bd_psr_lock(p);

    while (i < p->num_cb) {
        if (p->cb[i].handle == cb_handle && p->cb[i].cb == callback) {
            if (--p->num_cb) {
                memmove(p->cb + i, p->cb + i + 1, sizeof(PSR_CB_DATA) * p->num_cb);
                continue;
            }
        }
        i++;
    }

    bd_psr_unlock(p);
}

/*
 * PSR state save / restore
 */

void bd_psr_save_state(BD_REGISTERS *p)
{
    /* store registers 4-8 and 10-12 to backup registers */

    bd_psr_lock(p);

    memcpy(p->psr + 36, p->psr + 4,  sizeof(uint32_t) * 5);
    memcpy(p->psr + 42, p->psr + 10, sizeof(uint32_t) * 3);

    /* generate save event */

    if (p->num_cb) {
        BD_PSR_EVENT ev = {
            .ev_type = BD_PSR_SAVE,
            .psr_idx = -1,
            .old_val = 0,
            .new_val = 0,
        };

        unsigned j;
        for (j = 0; j < p->num_cb; j++) {
            p->cb[j].cb(p->cb[j].handle, &ev);
        }
    }

    bd_psr_unlock(p);
}

void bd_psr_reset_backup_registers(BD_REGISTERS *p)
{
    bd_psr_lock(p);

    /* init backup registers to default */
    memcpy(p->psr + 36, bd_psr_init + 36, sizeof(uint32_t) * 5);
    memcpy(p->psr + 42, bd_psr_init + 42, sizeof(uint32_t) * 3);

    bd_psr_unlock(p);
}

void bd_psr_restore_state(BD_REGISTERS *p)
{
    uint32_t old_psr[13];
    uint32_t new_psr[13];

    bd_psr_lock(p);

    if (p->num_cb) {
        memcpy(old_psr, p->psr, sizeof(old_psr[0]) * 13);
    }

    /* restore backup registers */
    memcpy(p->psr + 4,  p->psr + 36, sizeof(uint32_t) * 5);
    memcpy(p->psr + 10, p->psr + 42, sizeof(uint32_t) * 3);

    if (p->num_cb) {
        memcpy(new_psr, p->psr, sizeof(new_psr[0]) * 13);
    }

    /* init backup registers to default */
    memcpy(p->psr + 36, bd_psr_init + 36, sizeof(uint32_t) * 5);
    memcpy(p->psr + 42, bd_psr_init + 42, sizeof(uint32_t) * 3);

    /* generate restore events */
    if (p->num_cb) {
        BD_PSR_EVENT ev;
        unsigned     i, j;

        ev.ev_type = BD_PSR_RESTORE;

        for (i = 4; i < 13; i++) {
            if (i != PSR_NAV_TIMER) {

                ev.psr_idx = i;
                ev.old_val = old_psr[i];
                ev.new_val = new_psr[i];

                for (j = 0; j < p->num_cb; j++) {
                    p->cb[j].cb(p->cb[j].handle, &ev);
                }
            }
        }
    }

    bd_psr_unlock(p);
}

/*
 * GPR read / write
 */

int bd_gpr_write(BD_REGISTERS *p, int reg, uint32_t val)
{
    if (reg < 0 || reg > BD_GPR_COUNT) {
        BD_DEBUG(DBG_BLURAY, "bd_gpr_write(%d): invalid register\n", reg);
        return -1;
    }

    p->gpr[reg] = val;
    return 0;
}

uint32_t bd_gpr_read(BD_REGISTERS *p, int reg)
{
    if (reg < 0 || reg > BD_GPR_COUNT) {
        BD_DEBUG(DBG_BLURAY, "bd_gpr_read(%d): invalid register\n", reg);
        return -1;
    }

    return p->gpr[reg];
}

/*
 * PSR read / write
 */

uint32_t bd_psr_read(BD_REGISTERS *p, int reg)
{
    uint32_t val;

    if (reg < 0 || reg > BD_PSR_COUNT) {
        BD_DEBUG(DBG_BLURAY, "bd_psr_read(%d): invalid register\n", reg);
        return -1;
    }

    bd_psr_lock(p);

    val = p->psr[reg];

    bd_psr_unlock(p);

    return val;
}

int bd_psr_setting_write(BD_REGISTERS *p, int reg, uint32_t val)
{
    if (reg < 0 || reg > BD_PSR_COUNT) {
        BD_DEBUG(DBG_BLURAY, "bd_psr_write(%d, %d): invalid register\n", reg, val);
        return -1;
    }

    bd_psr_lock(p);

    if (p->psr[reg] == val) {
        BD_DEBUG(DBG_BLURAY, "bd_psr_write(%d, %d): no change in value\n", reg, val);
    } else if (bd_psr_name[reg]) {
        BD_DEBUG(DBG_BLURAY, "bd_psr_write(): PSR%-4d (%s) 0x%x -> 0x%x\n", reg, bd_psr_name[reg], p->psr[reg], val);
    } else {
        BD_DEBUG(DBG_BLURAY, "bd_psr_write(): PSR%-4d 0x%x -> 0x%x\n", reg, p->psr[reg], val);
    }

    if (p->num_cb) {
        BD_PSR_EVENT ev;
        unsigned i;

        ev.ev_type = p->psr[reg] == val ? BD_PSR_WRITE : BD_PSR_CHANGE;
        ev.psr_idx = reg;
        ev.old_val = p->psr[reg];
        ev.new_val = val;

        p->psr[reg] = val;

        for (i = 0; i < p->num_cb; i++) {
            p->cb[i].cb(p->cb[i].handle, &ev);
        }

    } else {

        p->psr[reg] = val;
    }

    bd_psr_unlock(p);

    return 0;
}

int bd_psr_write(BD_REGISTERS *p, int reg, uint32_t val)
{
    if ((reg == 13) ||
        (reg >= 15 && reg <= 20) ||
        (reg >= 29 && reg <= 31) ||
        (reg >= 48 && reg <= 61)) {
      BD_DEBUG(DBG_BLURAY, "bd_psr_write(%d, %d): read-only register !\n", reg, val);
      return -2;
  }

  return bd_psr_setting_write(p, reg, val);
}

