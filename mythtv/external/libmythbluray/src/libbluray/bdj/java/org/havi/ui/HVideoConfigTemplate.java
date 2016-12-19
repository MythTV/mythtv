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

public class HVideoConfigTemplate extends HScreenConfigTemplate {
    public boolean isConfigSupported(HVideoConfiguration hvc) {
        return match(hvc) >= 0;
    }

    protected int getPreferenceCount() {
        return super.getPreferenceCount() + 2;
    }

    protected int getPreferenceObjectCount() {
        return super.getPreferenceObjectCount() + 2;
    }

    protected int getPreferenceIndex(int preference) {
        if (preference == GRAPHICS_MIXING)
            return super.getPreferenceCount();
        if (preference == KEEP_RESOLUTION)
            return super.getPreferenceCount() + 1;
        return super.getPreferenceIndex(preference);
    }

    protected int getPreferenceObjectIndex(int preference) {
        if (preference == GRAPHICS_MIXING)
            return super.getPreferenceObjectCount();
        if (preference == KEEP_RESOLUTION)
            return super.getPreferenceObjectCount() + 1;
        return super.getPreferenceObjectIndex(preference);
    }

    public static final int GRAPHICS_MIXING = 0x0F;
    public static final int KEEP_RESOLUTION = 0x10;
}
