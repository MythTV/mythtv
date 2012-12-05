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

package org.havi.ui;

import java.awt.Dimension;
import java.util.Arrays;

public abstract class HScreenConfigTemplate {
    HScreenConfigTemplate() {
        objectArray = new Object[getPreferenceObjectCount()];
        priorityArray = new int[getPreferenceCount()];
        Arrays.fill(priorityArray, DONT_CARE);
    }

    public void setPreference(int preference, int priority) {
        if ((priority < REQUIRED) || (priority > REQUIRED_NOT))
            throw new IllegalArgumentException("invalid priority");
        int index = getPreferenceIndex(preference);
        if (index < 0)
            throw new IllegalArgumentException("invalid preference");
        priorityArray[index] = priority;
    }

    public void setPreference(int preference, Object object, int priority) {
        if ((priority < REQUIRED) || (priority > REQUIRED_NOT))
            throw new IllegalArgumentException("invalid priority");
        int index = getPreferenceObjectIndex(preference);
        if (index < 0)
            throw new IllegalArgumentException("invalid preference");
        objectArray[index] = object;
        priorityArray[getPreferenceIndex(preference)] = priority;
    }

    public int getPreferencePriority(int preference) {
        int index = getPreferenceIndex(preference);
        if (index < 0)
            throw new IllegalArgumentException("invalid preference");
        return priorityArray[getPreferenceIndex(preference)];
    }

    public Object getPreferenceObject(int preference) {
        int index = getPreferenceObjectIndex(preference);
        if (index < 0)
            throw new IllegalArgumentException("invalid preference");
        return objectArray[getPreferenceObjectIndex(preference)];
    }

    protected int getPreferenceCount() {
        return SCREEN_RECTANGLE - ZERO_BACKGROUND_IMPACT + 1;
    }

    protected int getPreferenceObjectCount() {
        return SCREEN_RECTANGLE - VIDEO_GRAPHICS_PIXEL_ALIGNED + 1;
    }

    protected int getPreferenceIndex(int preference) {
        if ((preference < ZERO_BACKGROUND_IMPACT) || (preference > SCREEN_RECTANGLE))
            return -1;
        return preference - ZERO_BACKGROUND_IMPACT;
    }

    protected int getPreferenceObjectIndex(int preference) {
        if ((preference < VIDEO_GRAPHICS_PIXEL_ALIGNED) || (preference > SCREEN_RECTANGLE))
                return -1;
        return preference - VIDEO_GRAPHICS_PIXEL_ALIGNED;
    }

    protected boolean equals(Object obj1, Object obj2) {
        return (obj1 == null) ? (obj2 == null) : obj1.equals(obj2);
    }

    protected int matchPreference(int preference, boolean supported) {
        int Priority = getPreferencePriority(preference);
        if (Priority == REQUIRED) {
            if (!supported)
                return -1;
            return 1;
        } else if (Priority == REQUIRED_NOT) {
            if (supported)
                return -1;
            return 1;
        }
        return 0;
    }

    protected int matchPreference(int preference, HScreenConfiguration hsc) {
        switch (preference) {
        case INTERLACED_DISPLAY:
            return matchPreference(INTERLACED_DISPLAY, hsc.getInterlaced());
        case FLICKER_FILTERING:
            return matchPreference(FLICKER_FILTERING, hsc.getFlickerFilter());
        case PIXEL_ASPECT_RATIO:
            return matchPreference(PIXEL_ASPECT_RATIO, equals(hsc.getPixelAspectRatio(), getPreferenceObject(PIXEL_ASPECT_RATIO)));
        case PIXEL_RESOLUTION:
            return matchPreference(PIXEL_RESOLUTION, equals(hsc.getPixelResolution(), getPreferenceObject(PIXEL_RESOLUTION)));
        case SCREEN_RECTANGLE:
            return matchPreference(SCREEN_RECTANGLE, equals(hsc.getScreenArea(), getPreferenceObject(SCREEN_RECTANGLE)));
        }
        return 0;
    }

    protected int match(HScreenConfiguration hsc) {
        int score = 0;
        int length = getPreferenceCount();
        for (int i = 0; i < length; i++) {
            int s = matchPreference(i, hsc);
            if (s < 0)
                return s;
            score += s;
        }
        return score;
    }

    static void initDefaultConfigTemplate(HScreenConfigTemplate hsct, int index) {
        hsct.setPreference(HScreenConfigTemplate.PIXEL_ASPECT_RATIO,
                new Dimension(defaultConfig[index][2], defaultConfig[index][3]),
                HScreenConfigTemplate.REQUIRED);
        hsct.setPreference(HScreenConfigTemplate.PIXEL_RESOLUTION,
            new Dimension(defaultConfig[index][0], defaultConfig[index][1]),
            HScreenConfigTemplate.REQUIRED);
        hsct.setPreference(HScreenConfigTemplate.SCREEN_RECTANGLE,
            new HScreenRectangle(0.0f, 0.0f, 1.0f, 1.0f),
            HScreenConfigTemplate.REQUIRED);
    }

    public static final int REQUIRED = 0x01;
    public static final int PREFERRED = 0x02;
    public static final int DONT_CARE = 0x03;
    public static final int PREFERRED_NOT = 0x04;
    public static final int REQUIRED_NOT = 0x05;

    public static final int ZERO_BACKGROUND_IMPACT = 0x01;
    public static final int ZERO_GRAPHICS_IMPACT = 0x02;
    public static final int ZERO_VIDEO_IMPACT = 0x03;
    public static final int INTERLACED_DISPLAY = 0x04;
    public static final int FLICKER_FILTERING = 0x05;
    public static final int VIDEO_GRAPHICS_PIXEL_ALIGNED = 0x06;
    public static final int PIXEL_ASPECT_RATIO = 0x07;
    public static final int PIXEL_RESOLUTION = 0x08;
    public static final int SCREEN_RECTANGLE = 0x09;

    private Object[] objectArray;
    private int[] priorityArray;

    static int[][] defaultConfig = {
        { 1920, 1080, 16, 9 },
        { 1280, 720, 16, 9 },
        { 720, 480, 4, 3 },
        { 720, 480, 16, 9 },
        { 720, 576, 4, 3 },
        { 720, 576, 16, 9 },
    };
}
