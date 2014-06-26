/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 *
 * Original Project
 *      MythTV      http://www.mythtv.org
 *
 * Copyright (c) 2004, 2005 John Pullan <john@pullan.org>
 * Copyright (c) 2009, Janne Grunau <janne-mythtv@grunau.be>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

extern "C" {
#include "libswscale/swscale.h"
}

#include <QMutex>
#include <QMutexLocker>

#include "mythlogging.h"
#include "myth_imgconvert.h"

int myth_sws_img_convert(AVPicture *dst, PixelFormat dst_pix_fmt,
                         const AVPicture *src, PixelFormat pix_fmt,
                         int width, int height)
{
    static QMutex ctx_lock;
    static struct SwsContext *convert_ctx = NULL;

    QMutexLocker locker(&ctx_lock);
    convert_ctx = sws_getCachedContext(convert_ctx, width, height, pix_fmt,
                                       width, height, dst_pix_fmt,
                                       SWS_FAST_BILINEAR, NULL, NULL, NULL);
    if (convert_ctx == NULL)
    {
        LOG(VB_GENERAL, LOG_ERR, "myth_sws_img_convert: Cannot initialize "
                                 "the image conversion context");
        return -1;
    }

    sws_scale(convert_ctx, src->data, src->linesize,
              0, height, dst->data, dst->linesize);

    return 0;
}
