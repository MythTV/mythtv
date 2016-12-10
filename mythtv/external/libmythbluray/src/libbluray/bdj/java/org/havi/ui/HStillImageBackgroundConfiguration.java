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

import java.awt.Color;
import java.io.IOException;

public class HStillImageBackgroundConfiguration extends
        HBackgroundConfiguration {
    protected HStillImageBackgroundConfiguration() {
    }

    protected HStillImageBackgroundConfiguration(HBackgroundConfigTemplate hbct, Color color) {
        super(hbct, color);
    }

    public void displayImage(HBackgroundImage image) throws IOException,
            HPermissionDeniedException, HConfigurationException {
        displayImage(image, new HScreenRectangle(0.0f, 0.0f, 1.0f, 1.0f));
    }

    public void displayImage(HBackgroundImage image, HScreenRectangle r)
            throws IOException, HPermissionDeniedException,
            HConfigurationException {
        this.image = image;
        this.rect = r;
    }

    protected HBackgroundImage getImage() {
        return image;
    }

    protected HScreenRectangle getRectangle() {
        return rect;
    }

    private HBackgroundImage image = null;
    private HScreenRectangle rect = new HScreenRectangle(0.0f, 0.0f, 1.0f, 1.0f);
}
