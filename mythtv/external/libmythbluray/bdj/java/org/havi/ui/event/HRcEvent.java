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

package org.havi.ui.event;

import java.awt.Component;
import java.awt.event.KeyEvent;

public class HRcEvent extends KeyEvent {
    @Deprecated
    public HRcEvent(Component source, int id, long when, int modifiers,
            int keyCode)
    {
        super(source, id, when, modifiers, keyCode, (char)keyCode);
    }

    public HRcEvent(Component source, int id, long when, int modifiers,
            int keyCode, char keyChar)
    {
        super(source, id, when, modifiers, keyCode, keyChar);
    }

    @Deprecated
    public static final int RC_FIRST = 400;
    public static final int VK_COLORED_KEY_0 = 403;
    public static final int VK_COLORED_KEY_1 = VK_COLORED_KEY_0 + 1;
    public static final int VK_COLORED_KEY_2 = VK_COLORED_KEY_1 + 1;
    public static final int VK_COLORED_KEY_3 = VK_COLORED_KEY_2 + 1;
    public static final int VK_COLORED_KEY_4 = VK_COLORED_KEY_3 + 1;
    public static final int VK_COLORED_KEY_5 = VK_COLORED_KEY_4 + 1;
    public static final int VK_POWER = VK_COLORED_KEY_5 + 1;
    public static final int VK_DIMMER = VK_POWER + 1;
    public static final int VK_WINK = VK_DIMMER + 1;
    public static final int VK_REWIND = VK_WINK + 1;
    public static final int VK_STOP = VK_REWIND + 1;
    public static final int VK_EJECT_TOGGLE = VK_STOP + 1;
    public static final int VK_PLAY = VK_EJECT_TOGGLE + 1;
    public static final int VK_RECORD = VK_PLAY + 1;
    public static final int VK_FAST_FWD = VK_RECORD + 1;
    public static final int VK_PLAY_SPEED_UP = VK_FAST_FWD + 1;
    public static final int VK_PLAY_SPEED_DOWN = VK_PLAY_SPEED_UP + 1;
    public static final int VK_PLAY_SPEED_RESET = VK_PLAY_SPEED_DOWN + 1;
    public static final int VK_RECORD_SPEED_NEXT = VK_PLAY_SPEED_RESET + 1;
    public static final int VK_GO_TO_START = VK_RECORD_SPEED_NEXT + 1;
    public static final int VK_GO_TO_END = VK_GO_TO_START + 1;
    public static final int VK_TRACK_PREV = VK_GO_TO_END + 1;
    public static final int VK_TRACK_NEXT = VK_TRACK_PREV + 1;
    public static final int VK_RANDOM_TOGGLE = VK_TRACK_NEXT + 1;
    public static final int VK_CHANNEL_UP = VK_RANDOM_TOGGLE + 1;
    public static final int VK_CHANNEL_DOWN = VK_CHANNEL_UP + 1;
    public static final int VK_STORE_FAVORITE_0 = VK_CHANNEL_DOWN + 1;
    public static final int VK_STORE_FAVORITE_1 = VK_STORE_FAVORITE_0 + 1;
    public static final int VK_STORE_FAVORITE_2 = VK_STORE_FAVORITE_1 + 1;
    public static final int VK_STORE_FAVORITE_3 = VK_STORE_FAVORITE_2 + 1;
    public static final int VK_RECALL_FAVORITE_0 = VK_STORE_FAVORITE_3 + 1;
    public static final int VK_RECALL_FAVORITE_1 = VK_RECALL_FAVORITE_0 + 1;
    public static final int VK_RECALL_FAVORITE_2 = VK_RECALL_FAVORITE_1 + 1;
    public static final int VK_RECALL_FAVORITE_3 = VK_RECALL_FAVORITE_2 + 1;
    public static final int VK_CLEAR_FAVORITE_0 = VK_RECALL_FAVORITE_3 + 1;
    public static final int VK_CLEAR_FAVORITE_1 = VK_CLEAR_FAVORITE_0 + 1;
    public static final int VK_CLEAR_FAVORITE_2 = VK_CLEAR_FAVORITE_1 + 1;
    public static final int VK_CLEAR_FAVORITE_3 = VK_CLEAR_FAVORITE_2 + 1;
    public static final int VK_SCAN_CHANNELS_TOGGLE = VK_CLEAR_FAVORITE_3 + 1;
    public static final int VK_PINP_TOGGLE = VK_SCAN_CHANNELS_TOGGLE + 1;
    public static final int VK_SPLIT_SCREEN_TOGGLE = VK_PINP_TOGGLE + 1;
    public static final int VK_DISPLAY_SWAP = VK_SPLIT_SCREEN_TOGGLE + 1;
    public static final int VK_SCREEN_MODE_NEXT = VK_DISPLAY_SWAP + 1;
    public static final int VK_VIDEO_MODE_NEXT = VK_SCREEN_MODE_NEXT + 1;
    public static final int VK_VOLUME_UP = VK_VIDEO_MODE_NEXT + 1;
    public static final int VK_VOLUME_DOWN = VK_VOLUME_UP + 1;
    public static final int VK_MUTE = VK_VOLUME_DOWN + 1;
    public static final int VK_SURROUND_MODE_NEXT = VK_MUTE + 1;
    public static final int VK_BALANCE_RIGHT = VK_SURROUND_MODE_NEXT + 1;
    public static final int VK_BALANCE_LEFT = VK_BALANCE_RIGHT + 1;
    public static final int VK_FADER_FRONT = VK_BALANCE_LEFT + 1;
    public static final int VK_FADER_REAR = VK_FADER_FRONT + 1;
    public static final int VK_BASS_BOOST_UP = VK_FADER_REAR + 1;
    public static final int VK_BASS_BOOST_DOWN = VK_BASS_BOOST_UP + 1;
    public static final int VK_INFO = VK_BASS_BOOST_DOWN + 1;
    public static final int VK_GUIDE = VK_INFO + 1;
    public static final int VK_TELETEXT = VK_GUIDE + 1;
    public static final int VK_SUBTITLE = VK_TELETEXT + 1;
    @Deprecated
    public static final int RC_LAST = VK_SUBTITLE;

    private static final long serialVersionUID = 2427692646491162046L;
}
