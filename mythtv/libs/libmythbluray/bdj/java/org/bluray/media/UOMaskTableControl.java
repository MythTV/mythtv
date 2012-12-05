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

package org.bluray.media;

import javax.media.Control;

public abstract interface UOMaskTableControl extends Control {
    public abstract void addUOMaskTableEventListener(
            UOMaskTableListener listener);

    public abstract void removeUOMaskTableEventListener(
            UOMaskTableListener listener);

    public abstract boolean[] getMaskedUOTable();

    public static final int CHAPTER_SEARCH_MASK_INDEX = 2;
    public static final int TIME_SEARCH_MASK_INDEX = 3;
    public static final int SKIP_TO_NEXT_POINT_MASK_INDEX = 4;
    public static final int SKIP_BACK_TO_PREVIOUS_POINT_MASK_INDEX = 5;
    public static final int STOP_MASK_INDEX = 7;
    public static final int PAUSE_ON_MASK_INDEX = 8;
    public static final int STILL_OFF_MASK_INDEX = 10;
    public static final int FORWARD_PLAY_MASK_INDEX = 11;
    public static final int BACKWARD_PLAY_MASK_INDEX = 12;
    public static final int RESUME_MASK_INDEX = 13;
    public static final int MOVE_UP_SELECTED_BUTTON_MASK_INDEX = 14;
    public static final int MOVE_DOWN_SELECTED_BUTTON_MASK_INDEX = 15;
    public static final int MOVE_LEFT_SELECTED_BUTTON_MASK_INDEX = 16;
    public static final int MOVE_RIGHT_SELECTED_BUTTON_MASK_INDEX = 17;
    public static final int SELECT_BUTTON_MASK_INDEX = 18;
    public static final int ACTIVATE_BUTTON_MASK_INDEX = 19;
    public static final int SELECT_AND_ACTIVATE_MASK_INDEX = 20;
    @Deprecated
    public static final int AUDIO_CHANGE_MASK_INDEX = 21;
    public static final int PRIMARY_AUDIO_CHANGE_MASK_INDEX = 21;
    public static final int ANGLE_CHANGE_MASK_INDEX = 23;
    public static final int POPUP_ON_MASK_INDEX = 24;
    public static final int POPUP_OFF_MASK_INDEX = 25;
    public static final int PG_TEXTST_ENABLE_DISABLE_MASK_INDEX = 26;
    public static final int PG_TEXTST_CHANGE_MASK_INDEX = 27;
    public static final int SECONDARY_VIDEO_ENABLE_DISABLE_MASK_INDEX = 28;
    public static final int SECONDARY_VIDEO_CHANGE_MASK_INDEX = 29;
    public static final int SECONDARY_AUDIO_ENABLE_DISABLE_MASK_INDEX = 30;
    public static final int SECONDARY_AUDIO_CHANGE_MASK_INDEX = 31;
    public static final int PIP_PG_TEXTST_CHANGE_MASK_INDEX = 33;
}
