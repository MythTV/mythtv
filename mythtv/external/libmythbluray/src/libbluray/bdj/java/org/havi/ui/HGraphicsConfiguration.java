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
import java.awt.GraphicsEnvironment;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.Component;
import java.awt.Container;
import java.awt.Image;
import java.awt.Font;
import java.awt.Color;

public class HGraphicsConfiguration extends HScreenConfiguration {
    protected HGraphicsConfiguration() {
    }

    HGraphicsConfiguration(HGraphicsConfigTemplate hgct) {
        super(hgct);
        this.hgct = hgct;
    }

    public HGraphicsDevice getDevice() {
        return HScreen.getDefaultHScreen().getDefaultHGraphicsDevice();
    }

    public HGraphicsConfigTemplate getConfigTemplate() {
        return hgct;
    }

    public HScreenRectangle getComponentHScreenRectangle(Component component) {
        Point p = component.getLocationOnScreen();
        Dimension r = getPixelResolution();
        HScreenRectangle sa = getScreenArea();
        return new HScreenRectangle((float)p.x / r.width + sa.x,
                (float)p.y / r.height + sa.y,
                (float)component.getWidth() / r.width,
                (float)component.getHeight() / r.height);
    }

    public Rectangle getPixelCoordinatesHScreenRectangle(HScreenRectangle sr, Container cont) {
        return new Rectangle((int)(sr.x * cont.getWidth()),
                (int)(sr.y * cont.getHeight()),
                (int)(sr.width * cont.getWidth()),
                (int)(sr.height * cont.getHeight()));
    }

    public Image getCompatibleImage(Image input, HImageHints ih) {
        return input;
    }

    public Font[] getAllFonts() {
        String[] names = GraphicsEnvironment.getLocalGraphicsEnvironment().getAvailableFontFamilyNames();
        Font[] fontArray = new Font[names.length];
        for (int i = 0; i < names.length; i++)
            fontArray[i] = new Font(names[i], Font.PLAIN, 12);
        return fontArray;
    }

    public Color getPunchThroughToBackgroundColor(int percentage) {
        org.videolan.Logger.unimplemented("HGraphicsConfigTemplate", "");
        return null;
    }

    public Color getPunchThroughToBackgroundColor(int percentage, HVideoDevice hvd) {
        org.videolan.Logger.unimplemented("HGraphicsConfigTemplate", "");
        return null;
    }

    public Color getPunchThroughToBackgroundColor(Color color, int percentage) {
        org.videolan.Logger.unimplemented("HGraphicsConfigTemplate", "");
        return null;
    }

    public Color getPunchThroughToBackgroundColor(Color color, int percentage, HVideoDevice v) {
        org.videolan.Logger.unimplemented("HGraphicsConfigTemplate", "");
        return null;
    }

    public void dispose(Color c) {
    }

    private HGraphicsConfigTemplate hgct;
}
