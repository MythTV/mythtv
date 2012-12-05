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

#if !defined(_BD_KEYS_H_)
#define _BD_KEYS_H_

/*
 * User input
 */

typedef enum {
    BD_VK_NONE      = 0xffff,

    /* numeric key events */
    BD_VK_0         = 0,
    BD_VK_1         = 1,
    BD_VK_2         = 2,
    BD_VK_3         = 3,
    BD_VK_4         = 4,
    BD_VK_5         = 5,
    BD_VK_6         = 6,
    BD_VK_7         = 7,
    BD_VK_8         = 8,
    BD_VK_9         = 9,

    /* */
    BD_VK_ROOT_MENU = 10,  /* open root menu */
    BD_VK_POPUP     = 11,  /* toggle popup menu */

    /* interactive key events */
    BD_VK_UP        = 12,
    BD_VK_DOWN      = 13,
    BD_VK_LEFT      = 14,
    BD_VK_RIGHT     = 15,
    BD_VK_ENTER     = 16,

    /* Mouse click */
    /* Translated to BD_VK_ENTER if mouse is over valid button */
    BD_VK_MOUSE_ACTIVATE = 17,

} bd_vk_key_e;

#endif // _BD_KEYS_H_
