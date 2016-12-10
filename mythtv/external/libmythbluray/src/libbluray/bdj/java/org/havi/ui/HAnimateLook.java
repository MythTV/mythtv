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

import java.awt.Graphics;
import java.awt.Dimension;
import java.awt.Insets;

import org.videolan.Logger;

public class HAnimateLook implements HExtendedLook {
    public HAnimateLook() {
        logger.unimplemented("");
    }

    public void fillBackground(Graphics g, HVisible visible, int state) {
        logger.unimplemented("");
    }

    public void renderBorders(Graphics g, HVisible visible, int state) {
        logger.unimplemented("");
    }

    public void renderVisible(Graphics g, HVisible visible, int state) {
        logger.unimplemented("renderVisible");
    }

    public void showLook(Graphics g, HVisible visible, int state) {
        logger.unimplemented("renderVisible");
    }

    public void widgetChanged(HVisible visible, HChangeData[] changes) {
        logger.unimplemented("renderVisible");
    }

    public Dimension getMinimumSize(HVisible hvisible) {
        logger.unimplemented("getMinimumSize");
        return hvisible.getSize();
    }

    public Dimension getPreferredSize(HVisible hvisible) {
        logger.unimplemented("getPreferredSize");
        return hvisible.getSize();
    }

    public Dimension getMaximumSize(HVisible hvisible) {
        logger.unimplemented("getMaximumSize");
        return hvisible.getSize();
    }

    public boolean isOpaque(HVisible visible) {
        logger.unimplemented("getMaximumSize");
        return false;
    }

    public Insets getInsets(HVisible hvisible) {
        logger.unimplemented("getMaximumSize");
        return new Insets(0, 0, 0, 0);
    }

    private static final Logger logger = Logger.getLogger(HAnimateLook.class.getName());
}
