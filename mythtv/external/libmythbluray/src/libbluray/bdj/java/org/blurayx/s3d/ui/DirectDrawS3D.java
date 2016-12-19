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

import java.awt.Image;
import java.awt.Rectangle;

public class DirectDrawS3D {

    public void drawStereoscopic(Image leftImage, int leftX, int leftY,
                                 Rectangle[] leftUpdateAreas,
                                 Image rightImage, int rightX, int rightY,
                                 Rectangle[] rightUpdateAreas) {
    }

    public void drawStereoscopicImages(Image[] leftImages, int[] leftXs, int[] leftYs,
                                       Rectangle[] leftUpdateAreas,
                                       Image[] rightImages, int[] rightXs, int[] rightYs,
                                       Rectangle[] rightUpdateAreas) {
    }

    public static DirectDrawS3D getInstance() {
        org.videolan.Logger.unimplemented("DirectDrawS3D", "getInstance");
        return null;
    }
}
