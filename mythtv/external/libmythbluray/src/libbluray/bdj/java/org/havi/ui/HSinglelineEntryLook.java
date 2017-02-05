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

public class HSinglelineEntryLook implements HExtendedLook {
    public HSinglelineEntryLook() {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
    }

    public void fillBackground(Graphics g, HVisible visible, int state) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
    }

    public void renderBorders(Graphics g, HVisible visible, int state) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
    }

    public void renderVisible(Graphics g, HVisible visible, int state) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
    }

    public void showLook(Graphics g, HVisible visible, int state) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
    }

    public void widgetChanged(HVisible visible, HChangeData[] changes) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
    }

    public Dimension getMinimumSize(HVisible hvisible) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
        return null;
    }

    public Dimension getPreferredSize(HVisible hvisible) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
        return null;
    }

    public Dimension getMaximumSize(HVisible hvisible) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
        return null;
    }

    public boolean isOpaque(HVisible visible) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
        return false;
    }

    public Insets getInsets(HVisible hvisible) {
        org.videolan.Logger.unimplemented(HSinglelineEntryLook.class.getName(), "");
        return null;
    }
}
