/*
 * This file is part of libbluray
 * Copyright (C) 2012  Libbluray
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

package java.awt;

import java.awt.color.ColorSpace;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.DataBuffer;
import java.awt.image.DirectColorModel;
import java.awt.image.VolatileImage;

abstract class BDGraphicsConfigurationBase extends GraphicsConfiguration {
    private BDGraphicsDevice device;

    BDGraphicsConfigurationBase(BDGraphicsDevice device) {
        this.device = device;
    }

    public GraphicsDevice getDevice() {
        return device;
    }

    public Rectangle getBounds() {
        return device.getBounds();
    }

    int getCompatibleImageType() {
        return BufferedImage.TYPE_INT_ARGB;
    }

    public ColorModel getColorModel() {
        return new DirectColorModel(ColorSpace.getInstance(ColorSpace.CS_sRGB),
                                    32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000, true,
                                    DataBuffer.TYPE_INT);
    }

    public BufferedImage createCompatibleImage(int width, int height) {
        if (width <= 0 || height <= 0)
            return null;
        return BDImage.getBuffededImage(width, height, this);
    }

    public BufferedImage createCompatibleImage(int width, int height, int trans) {
        if (width <= 0 || height <= 0)
            return null;
        return BDImage.getBuffededImage(width, height, this);
    }

    public VolatileImage createCompatibleVolatileImage(int width, int height) {
        org.videolan.Logger.unimplemented("BDGraphicsConfiguration", "createCompatibleVolatileImage");
        return null;
    }
}
