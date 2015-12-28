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

import java.awt.Point;
import java.awt.Dimension;

public abstract class HScreenConfiguration extends Object {
    HScreenConfiguration() {

    }

    HScreenConfiguration(HScreenConfigTemplate hsct) {
        FlickerFilter = hsct.getPreferencePriority(HScreenConfigTemplate.INTERLACED_DISPLAY) == HScreenConfigTemplate.REQUIRED;
        Interlaced = hsct.getPreferencePriority(HScreenConfigTemplate.FLICKER_FILTERING) == HScreenConfigTemplate.REQUIRED;
        AspectRatio = (Dimension)hsct.getPreferenceObject(HScreenConfigTemplate.PIXEL_ASPECT_RATIO);
        Resolution = (Dimension)hsct.getPreferenceObject(HScreenConfigTemplate.PIXEL_RESOLUTION);
        ScreenArea = (HScreenRectangle)hsct.getPreferenceObject(HScreenConfigTemplate.SCREEN_RECTANGLE);
    }

    public Point convertTo(HScreenConfiguration destination, Point source) {
        try {
            Dimension dstResolution = destination.getPixelResolution();
            HScreenRectangle dstScreenArea = destination.getScreenArea();
            return new Point(Math.round((float)source.x + ScreenArea.x * Resolution.width - dstScreenArea.x * dstResolution.width),
                             Math.round((float)source.y + ScreenArea.y * Resolution.height - dstScreenArea.y * dstResolution.height));
        } catch (Exception e) {
            return null;
        }
    }

    public boolean getFlickerFilter() {
        return FlickerFilter;
    }

    public boolean getInterlaced() {
        return Interlaced;
    }

    public Dimension getPixelAspectRatio() {
        return AspectRatio;
    }

    public Dimension getPixelResolution() {
        return Resolution;
    }

    public HScreenRectangle getScreenArea() {
        return ScreenArea;
    }

    public Dimension getOffset(HScreenConfiguration hsc) {
        Point origin = hsc.convertTo(this, new Point(0, 0));
        return (origin == null) ? null : (new Dimension(origin.x, origin.y));
    }

    private boolean FlickerFilter;
    private boolean Interlaced;
    private Dimension AspectRatio;
    private Dimension Resolution;
    private HScreenRectangle ScreenArea;
}
