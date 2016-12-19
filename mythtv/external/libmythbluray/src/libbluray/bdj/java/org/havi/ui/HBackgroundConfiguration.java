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

public class HBackgroundConfiguration extends HScreenConfiguration {
    protected HBackgroundConfiguration() {
    }

    protected HBackgroundConfiguration(HBackgroundConfigTemplate hbct, Color color) {
        super(hbct);
        this.hbct = hbct;
        this.color = color;
    }

    public HBackgroundDevice getDevice() {
        return HScreen.getDefaultHScreen().getDefaultHBackgroundDevice();
    }

    public HBackgroundConfigTemplate getConfigTemplate() {
        return hbct;
    }

    public Color getColor() {
        return color;
    }

    public void setColor(Color color)
            throws HPermissionDeniedException, HConfigurationException {
        if (color.getAlpha() < 255) {
            org.videolan.Logger.unimplemented("HBackgroundConfiguration", "setColor(alpha<255)");
            //throw new HConfigurationException();
        }
        this.color = color;
    }

    private HBackgroundConfigTemplate hbct;
    private Color color;
}
