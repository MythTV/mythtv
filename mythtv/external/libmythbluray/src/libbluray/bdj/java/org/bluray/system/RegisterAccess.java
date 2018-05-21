/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
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
package org.bluray.system;

import org.videolan.Libbluray;
import org.videolan.Logger;

public class RegisterAccess {
    private RegisterAccess() {
    }

    public static RegisterAccess getInstance() {
        return instance;
    }

    public int getGPR(int num) {
        if (num < 0 || num > 4095) {
            logger.error("getGPR(" + num + "): invalid GPR");
            throw new IllegalArgumentException("GPR " + num + " is not supported.");
        }

        return Libbluray.readGPR(num);
    }

    public int getPSR(int num) {
        if (num < 0 || num > 127) {
            logger.error("getPSR(" + num + "): invalid PSR");
            throw new IllegalArgumentException("PSR " + num + " is not supported.");
        }

        return Libbluray.readPSR(num);
    }

    public void setGPR(int num, int value) {
        if (num < 0 || num > 4095) {
            logger.error("setGPR(" + num + ", " + value + "): invalid GPR");
            throw new IllegalArgumentException("GPR " + num + " is not supported.");
        }
        Libbluray.writeGPR(num, value);
    }

    public static final int PSR_AUDIO_STN = 1;
    public static final int PSR_PG_TXTST_STN = 2;
    public static final int PSR_ANGLE_NR = 3;
    public static final int PSR_TITLE_NR = 4;
    public static final int PSR_CHAPTER_NR = 5;
    public static final int PSR_PLAYLIST_ID = 6;
    public static final int PSR_PLAYITEM_ID = 7;
    public static final int PSR_PRES_TIME = 8;

    public static final int PSR_USER_STYLE_NR = 12;
    public static final int PSR_PARENTAL_LVL = 13;
    public static final int PSR_SECONDARY_AUDIO_STN = 14;
    public static final int PSR_PLAYER_CONFIG_AUDIO = 15;
    public static final int PSR_LANG_CODE_AUDIO = 16;
    public static final int PSR_LANG_CODE_PG_TXTST = 17;
    public static final int PSR_MENU_DESCR_LANG_CODE = 18;
    public static final int PSR_COUNTRY_CODE = 19;
    public static final int PSR_REGION_PLAYBACK_CODE = 20;

    public static final int PSR_VIDEO_CAPABILITY = 29;
    public static final int PSR_PLAYER_CAP_TXTST = 30;
    public static final int PSR_PLAYER_PROFILE = 31;

    private static final RegisterAccess instance = new RegisterAccess();

    private static final Logger logger = Logger.getLogger(RegisterAccess.class.getName());
}
