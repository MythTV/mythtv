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

import java.awt.Color;
import java.awt.event.KeyEvent;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;

import org.bluray.ui.event.HRcEvent;

public class HRcCapabilities extends HKeyCapabilities {
    protected HRcCapabilities() {
    }

    public static HEventRepresentation getRepresentation(int keyCode) {
        if (!isSupported(keyCode))
            return null;
        return new HEventRepresentation(getKeyText(keyCode), getKeyColor(keyCode), null);
    }

    public static boolean getInputDeviceSupported() {
        return true;
    }

    public static boolean isSupported(int keycode) {
        return Arrays.binarySearch(supportedRcCode, keycode) >= 0;
    }

    private static Color getKeyColor(int keyCode) {
        switch (keyCode) {
        case HRcEvent.VK_COLORED_KEY_0:
            return Color.red;
        case HRcEvent.VK_COLORED_KEY_1:
            return Color.green;
        case HRcEvent.VK_COLORED_KEY_2:
            return Color.yellow;
        case HRcEvent.VK_COLORED_KEY_3:
            return Color.blue;
        }
        return null;
    }

    private static String getKeyText(int keyCode) {
        if ((keyCode >= HRcEvent.RC_FIRST) && (keyCode <= HRcEvent.VK_PG_TEXTST_ENABLE_DISABLE)) {
            switch (keyCode) {
            case HRcEvent.VK_BALANCE_LEFT:
                return "Balance Left";
            case HRcEvent.VK_BALANCE_RIGHT:
                return "Balance Right";
            case HRcEvent.VK_BASS_BOOST_DOWN:
                return "Bass Boost Down";
            case HRcEvent.VK_BASS_BOOST_UP:
                return "Bass Boost Up";
            case HRcEvent.VK_CHANNEL_DOWN:
                return "Channel Down";
            case HRcEvent.VK_CHANNEL_UP:
                return "Channel Up";
            case HRcEvent.VK_CLEAR_FAVORITE_0:
                return "Clear Favorite 1";
            case HRcEvent.VK_CLEAR_FAVORITE_1:
                return "Clear Favorite 1";
            case HRcEvent.VK_CLEAR_FAVORITE_2:
                return "Clear Favorite 1";
            case HRcEvent.VK_CLEAR_FAVORITE_3:
                return "Clear Favorite 1";
            case HRcEvent.VK_COLORED_KEY_0:
                return "Colored Key 0";
            case HRcEvent.VK_COLORED_KEY_1:
                return "Colored Key 1";
            case HRcEvent.VK_COLORED_KEY_2:
                return "Colored Key 2";
            case HRcEvent.VK_COLORED_KEY_3:
                return "Colored Key 3";
            case HRcEvent.VK_COLORED_KEY_4:
                return "Colored Key 4";
            case HRcEvent.VK_COLORED_KEY_5:
                return "Colored Key 5";
            case HRcEvent.VK_DIMMER:
                return "Dimmer";
            case HRcEvent.VK_DISPLAY_SWAP:
                return "Swap Display";
            case HRcEvent.VK_EJECT_TOGGLE:
                return "Toggle Eject";
            case HRcEvent.VK_FADER_FRONT:
                return "Fader Front";
            case HRcEvent.VK_FADER_REAR:
                return "Fader Rear";
            case HRcEvent.VK_FAST_FWD:
                return "Fast Farward";
            case HRcEvent.VK_GO_TO_END:
                return "GGo to End";
            case HRcEvent.VK_GO_TO_START:
                return "Go to Start";
            case HRcEvent.VK_GUIDE:
                return "Guide";
            case HRcEvent.VK_INFO:
                return "Infomation";
            case HRcEvent.VK_MUTE:
                return "Mute";
            case HRcEvent.VK_PINP_TOGGLE:
                return "Toggle Picture-in-picture";
            case HRcEvent.VK_PLAY:
                return "Play";
            case HRcEvent.VK_PLAY_SPEED_DOWN:
                return "Play Speed Down";
            case HRcEvent.VK_PLAY_SPEED_RESET:
                return "Play Speed Reset";
            case HRcEvent.VK_PLAY_SPEED_UP:
                return "Play Speed Up";
            case HRcEvent.VK_POWER:
                return "Power";
            case HRcEvent.VK_RANDOM_TOGGLE:
                return "Toggle Random";
            case HRcEvent.VK_RECALL_FAVORITE_0:
                return "Recall Favorite 0";
            case HRcEvent.VK_RECALL_FAVORITE_1:
                return "Recall Favorite 1";
            case HRcEvent.VK_RECALL_FAVORITE_2:
                return "Recall Favorite 2";
            case HRcEvent.VK_RECALL_FAVORITE_3:
                return "Recall Favorite 3";
            case HRcEvent.VK_RECORD:
                return "Record";
            case HRcEvent.VK_RECORD_SPEED_NEXT:
                return "Next Record Speed";
            case HRcEvent.VK_REWIND:
                return "Rewind";
            case HRcEvent.VK_SCAN_CHANNELS_TOGGLE:
                return "Toggle Scan Channels";
            case HRcEvent.VK_SCREEN_MODE_NEXT:
                return "Next Screen Mode";
            case HRcEvent.VK_SPLIT_SCREEN_TOGGLE:
                return "Toggle Split Screen";
            case HRcEvent.VK_STOP:
                return "Stop";
            case HRcEvent.VK_STORE_FAVORITE_0:
                return "Store Favorite 0";
            case HRcEvent.VK_STORE_FAVORITE_1:
                return "Store Favorite 1";
            case HRcEvent.VK_STORE_FAVORITE_2:
                return "Store Favorite 2";
            case HRcEvent.VK_STORE_FAVORITE_3:
                return "Store Favorite 3";
            case HRcEvent.VK_SUBTITLE:
                return "Subtitle";
            case HRcEvent.VK_SURROUND_MODE_NEXT:
                return "Next Surround Mode";
            case HRcEvent.VK_TELETEXT:
                return "Teletext";
            case HRcEvent.VK_TRACK_NEXT:
                return "Next Track";
            case HRcEvent.VK_TRACK_PREV:
                return "Preview Track";
            case HRcEvent.VK_VIDEO_MODE_NEXT:
                return "Next Video Mode";
            case HRcEvent.VK_VOLUME_DOWN:
                return "Volume Down";
            case HRcEvent.VK_VOLUME_UP:
                return "Volume Up";
            case HRcEvent.VK_WINK:
                return "Win Key";
            }
        }
        return KeyEvent.getKeyText(keyCode);
    }

    private static final int[] supportedRcCode;

    static {
        ArrayList list = new ArrayList();

        list.add(new Integer(HRcEvent.VK_ENTER));
        list.add(new Integer(HRcEvent.VK_LEFT));
        list.add(new Integer(HRcEvent.VK_UP));
        list.add(new Integer(HRcEvent.VK_RIGHT));
        list.add(new Integer(HRcEvent.VK_DOWN));
        for (int i = 0; i < 9; i++)
            list.add(new Integer(HRcEvent.VK_0 + i));

        Field[] fields = org.bluray.ui.event.HRcEvent.class.getFields();
        for (int i = 0; i < fields.length; i++) {
            String name = fields[i].getName();
            if ((name.startsWith("VK_")) && !(name.equals("VK_UNDEFINED"))) {
                try {
                    int keyCode = fields[i].getInt(null);
                    if ((keyCode > HRcEvent.RC_FIRST) && (keyCode <= HRcEvent.VK_PG_TEXTST_ENABLE_DISABLE))
                        list.add(new Integer(keyCode));
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        }
        supportedRcCode = new int[list.size()];
        for (int i = 0; i < list.size(); i++)
            supportedRcCode[i] = ((Integer)list.get(i)).intValue();
        Arrays.sort(supportedRcCode);
    }
}
