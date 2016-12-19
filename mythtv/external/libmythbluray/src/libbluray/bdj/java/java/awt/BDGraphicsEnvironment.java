/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

import java.awt.image.BufferedImage;
import java.util.Locale;

import org.videolan.Logger;

class BDGraphicsEnvironment extends GraphicsEnvironment {
    private BDGraphicsDevice graphicsDevice;

    BDGraphicsEnvironment() {
        graphicsDevice = new BDGraphicsDevice(this);
    }

    public GraphicsDevice[] getScreenDevices() {
        return new GraphicsDevice[] { graphicsDevice };
    }

    public GraphicsDevice getDefaultScreenDevice() {
        return graphicsDevice;
    }

    public String[] getAvailableFontFamilyNames() {
        return BDFontMetrics.getFontList();
    }

    public String[] getAvailableFontFamilyNames(Locale l) {
        return BDFontMetrics.getFontList();
    }

    public Graphics2D createGraphics(BufferedImage img) {
        return img.createGraphics();
    }

    /* not in J2ME */
    public Font[] getAllFonts() {
        Logger.unimplemented("BDGraphicsEnvironment", "getAllFonts");
        return null;
    }
}
