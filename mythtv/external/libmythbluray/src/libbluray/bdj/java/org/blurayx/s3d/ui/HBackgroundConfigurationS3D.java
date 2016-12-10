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

import java.io.IOException;
import org.havi.ui.HBackgroundImage;
import org.havi.ui.HConfigurationException;
import org.havi.ui.HPermissionDeniedException;
import org.havi.ui.HScreenRectangle;
import org.havi.ui.HStillImageBackgroundConfiguration;

public class HBackgroundConfigurationS3D extends HStillImageBackgroundConfiguration {

    public void displayImage(HBackgroundImage leftImage,
                             HBackgroundImage rightImage)
        throws IOException, HPermissionDeniedException, HConfigurationException {

        displayImage(leftImage, rightImage,
                     new HScreenRectangle(0.0f, 0.0f, 1.0f, 1.0f),
                     new HScreenRectangle(0.0f, 0.0f, 1.0f, 1.0f));
    }

    public void displayImage(HBackgroundImage leftImage,
                             HBackgroundImage rightImage,
                             HScreenRectangle leftUpdateArea,
                             HScreenRectangle rightUpdateArea)
        throws IOException, HPermissionDeniedException, HConfigurationException {

        displayImage(leftImage, leftUpdateArea);
        org.videolan.Logger.unimplemented("HBackgroundConfigurationS3D", "displayImage");
    }
}
