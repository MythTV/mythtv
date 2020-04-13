/*
    Public ivtv API header
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo.com>

    VBI portions:
    Copyright (C) 2004  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef LINUX_IVTV_H
#define LINUX_IVTV_H

/* Good for version 0.2.0 through 0.3.7 for IVTV_IOC_G_VBI_MODE */
/* Good for version 0.3.8 through 0.7.x for IVTV_IOC_S_VBI_PASSTHROUGH */
#define VBI_TYPE_TELETEXT 0x1 // Teletext (uses lines 6-22 for PAL, 10-21 for NTSC)
#define VBI_TYPE_CC       0x4 // Closed Captions (line 21 NTSC, line 22 PAL)
#define VBI_TYPE_WSS      0x5 // Wide Screen Signal (line 20 NTSC, line 23 PAL)
#define VBI_TYPE_VPS      0x7 // Video Programming System (PAL) (line 16)

#endif /* LINUX_IVTV_H */
