/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

package com.aacsla.bluray.online;

import org.videolan.Libbluray;
import org.videolan.Logger;

public class MediaAttribute {
    public MediaAttribute() {
    }

    public byte[] getPMSN() {
        byte pmsn[] = Libbluray.getAacsData(Libbluray.AACS_MEDIA_PMSN);
        if (pmsn == null) {
            logger.warning("getPMSN() failed");
            return new byte[16];
        }
        return pmsn;
    }

    public byte[] getVolumeID() {
        byte vid[] = Libbluray.getAacsData(Libbluray.AACS_MEDIA_VID);
        if (vid == null) {
            logger.warning("getVolumeID() failed");
            return new byte[16];
        }
        return vid;
    }

    private static final Logger logger = Logger.getLogger(MediaAttribute.class.getName());
}
