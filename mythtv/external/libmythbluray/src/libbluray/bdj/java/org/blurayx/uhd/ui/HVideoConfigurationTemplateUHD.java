/*
 * This file is part of libbluray
 * Copyright (C) 2017  VideoLAN
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

package org.blurayx.uhd.ui;

import org.bluray.ui.BDVideoConfigTemplate;

public class HVideoConfigurationTemplateUHD extends BDVideoConfigTemplate {
    public static final int DYNAMIC_RANGE = 18;
    public static final int COLOR_SPACE = 19;

    protected int getPreferenceCount() {
        return super.getPreferenceCount() + 2;
    }

    protected int getPreferenceObjectCount() {
        return super.getPreferenceObjectCount() + 2;
    }

    protected int getPreferenceIndex(int preference) {
        if (preference == DYNAMIC_RANGE)
            return super.getPreferenceCount();
        else if (preference == COLOR_SPACE)
            return super.getPreferenceCount() + 1;
        return super.getPreferenceIndex(preference);
    }

    protected int getPreferenceObjectIndex(int preference) {
        if (preference == DYNAMIC_RANGE)
            return super.getPreferenceObjectCount();
        else if (preference == COLOR_SPACE)
            return super.getPreferenceObjectCount() + 1;
        return super.getPreferenceObjectIndex(preference);
    }
}
