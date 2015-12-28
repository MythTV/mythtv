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

import java.awt.image.VolatileImage;

class BDGraphicsConfiguration extends BDGraphicsConfigurationBase {
    BDGraphicsConfiguration(BDGraphicsDevice device) {
        super(device);
    }

    public java.awt.geom.AffineTransform getNormalizingTransform() {
        org.videolan.Logger.unimplemented("BDGraphicsConfiguration", "getNormalizingTransform");
        return null;
    }
    public java.awt.geom.AffineTransform getDefaultTransform() {
        // return Identity transformation
        return new java.awt.geom.AffineTransform();
    }
    public java.awt.image.ColorModel getColorModel(int transparency) {
        org.videolan.Logger.unimplemented("BDGraphicsConfiguration", "getColorModel");
        return null;
    }

    public VolatileImage createCompatibleVolatileImage(int width, int height, int trans) {
        org.videolan.Logger.unimplemented("BDGraphicsConfiguration", "createCompatibleVolatileImage");
        return null;
    }

    public VolatileImage createCompatibleVolatileImage(int width, int height, ImageCapabilities caps) {
        org.videolan.Logger.unimplemented("BDGraphicsConfiguration", "createCompatibleVolatileImage");
        return null;
    }

    public VolatileImage createCompatibleVolatileImage(int width, int height, ImageCapabilities caps, int trans) {
        org.videolan.Logger.unimplemented("BDGraphicsConfiguration", "createCompatibleVolatileImage");
        return null;
    }

    public BufferCapabilities getBufferCapabilities(){
        org.videolan.Logger.unimplemented("BDGraphicsConfiguration", "getBufferCapabilities");
        return super.getBufferCapabilities();
    }
    public ImageCapabilities getImageCapabilities(){
        org.videolan.Logger.unimplemented("BDGraphicsConfiguration", "getImageCapabilities");
        return super.getImageCapabilities();
    }

    /* J2SE java 6 */
    public boolean isTranslucencyCapable() {
        return true;
    }
}
