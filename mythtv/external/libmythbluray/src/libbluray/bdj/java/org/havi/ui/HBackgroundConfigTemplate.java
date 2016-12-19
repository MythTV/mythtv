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

public class HBackgroundConfigTemplate extends HScreenConfigTemplate {
    public boolean isConfigSupported(HBackgroundConfiguration hbc) {
        return match(hbc) >= 0;
    }

    protected int getPreferenceCount() {
        return super.getPreferenceCount() + 2;
    }

    protected int getPreferenceIndex(int preference) {
        if (preference == CHANGEABLE_SINGLE_COLOR)
            return super.getPreferenceCount();
        else if (preference == STILL_IMAGE)
            return super.getPreferenceCount() + 1;
        return super.getPreferenceIndex(preference);
    }

    protected int matchPreference(int preference, HScreenConfiguration hsc) {
        if (preference == CHANGEABLE_SINGLE_COLOR)
            return matchPreference(CHANGEABLE_SINGLE_COLOR,
                                   ((HBackgroundConfiguration)hsc).getColor() != null);
        else if (preference == STILL_IMAGE)
            return matchPreference(STILL_IMAGE,
                                   (hsc != null) && (hsc instanceof HStillImageBackgroundConfiguration));
        return super.matchPreference(preference, hsc);
    }

    public static final int CHANGEABLE_SINGLE_COLOR = 0x0A;
    public static final int STILL_IMAGE = 0x0B;
}
