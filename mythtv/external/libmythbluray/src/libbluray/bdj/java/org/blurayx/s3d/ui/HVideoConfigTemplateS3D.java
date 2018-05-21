/*
 * This file is part of libbluray
 * Copyright (C) 2014  Libbluray
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

package org.blurayx.s3d.ui;

import org.bluray.ui.BDVideoConfigTemplate;

public class HVideoConfigTemplateS3D extends BDVideoConfigTemplate {

    public static final int S3D = 17;

    protected int getPreferenceCount() {
        return super.getPreferenceCount() + 1;
    }

    protected int getPreferenceObjectCount() {
        return super.getPreferenceObjectCount() + 1;
    }

    protected int getPreferenceIndex(int preference) {
        if (preference == S3D)
            return super.getPreferenceCount();
        return super.getPreferenceIndex(preference);
    }

    protected int getPreferenceObjectIndex(int preference) {
        if (preference == S3D)
            return super.getPreferenceObjectCount();
        return super.getPreferenceObjectIndex(preference);
    }
}
