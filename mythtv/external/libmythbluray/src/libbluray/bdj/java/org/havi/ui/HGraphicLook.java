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
import java.awt.Graphics;
import java.awt.Insets;

public class HGraphicLook implements HExtendedLook {
    public HGraphicLook() {
    }

    public void fillBackground(Graphics g, HVisible visible, int state) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "fillBackground");
    }

    public void renderBorders(Graphics g, HVisible visible, int state) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "renderBorders");
    }

    public void renderVisible(Graphics g, HVisible visible, int state) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "renderVisible");
    }

    public void showLook(Graphics g, HVisible visible, int state) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "showLook");
    }

    public void widgetChanged (HVisible visible, HChangeData[] changes) {
        if (visible.isVisible()) visible.repaint();
    }

    public Dimension getMinimumSize(HVisible hvisible) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "getMinimumSize");
        return null;
    }

    public Dimension getPreferredSize(HVisible hvisible) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "getPreferredSize");
        return null;
    }

    public Dimension getMaximumSize(HVisible hvisible) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "getMaximumSize");
        return null;
    }

    public boolean isOpaque(HVisible visible) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "isOpaque");
        return false;
    }

    public Insets getInsets(HVisible hvisible) {
        org.videolan.Logger.unimplemented(HGraphicLook.class.getName(), "getInsets");
        return new Insets(0, 0, 0, 0);
    }
}









