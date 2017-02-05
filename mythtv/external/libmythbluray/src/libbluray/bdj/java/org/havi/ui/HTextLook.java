/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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
import java.awt.Graphics;
import java.awt.Dimension;
import java.awt.Insets;

import org.videolan.Logger;

public class HTextLook implements HExtendedLook {

    public HTextLook() {
    }

    public void fillBackground(Graphics g, HVisible visible, int state) {
        if (visible.getBackgroundMode() == HVisible.BACKGROUND_FILL) {
            Color color = visible.getBackground();
            Dimension dimension = visible.getSize();
            g.setColor(color);
            g.fillRect(0, 0, dimension.width, dimension.height);
        }
    }

    public void renderBorders(Graphics g, HVisible visible, int state) {
        Insets insets = getInsets(visible);
        Color fg = visible.getForeground();
        Dimension dimension = visible.getSize();

        if (fg != null) {
            g.setColor(fg);
            g.fillRect(0, 0, dimension.width, insets.top);
            g.fillRect(dimension.width - insets.right, 0, insets.right, dimension.height);
            g.fillRect(0, dimension.height - insets.bottom, dimension.width, insets.bottom);
            g.fillRect(0, 0, insets.left, dimension.height);
        }
    }

    public void renderVisible(Graphics g, HVisible visible, int state) {
        String text = visible.getTextContent(state);
        Insets insets = getInsets(visible);
        if (text == null) {
            return;
        }
        logger.unimplemented("renderVisible[text=" + text + "]");
    }

    public void showLook(Graphics g, HVisible visible, int state) {
        fillBackground(g, visible, state);
        renderVisible(g, visible, state);
        renderBorders(g, visible, state);
    }

    public void widgetChanged(HVisible visible, HChangeData[] changes) {
        visible.repaint();
    }

    public Dimension getMinimumSize(HVisible hvisible) {
        logger.unimplemented("getMinimumSize");
        return null;
    }

    public Dimension getPreferredSize(HVisible hvisible) {
        logger.unimplemented("getPreferredSize");
        return null;
    }

    public Dimension getMaximumSize(HVisible hvisible) {
        logger.unimplemented("getMAximumSize");
        return null;
    }

    public boolean isOpaque(HVisible visible) {
        if (visible.getBackgroundMode() != 1) {
            return false;
        }

        Color bg = visible.getBackground();
        if ((bg == null) || (bg.getAlpha() < 255)) {
            return false;
        }

        return true;
    }

    public Insets getInsets(HVisible visible) {
        if (!visible.getBordersEnabled()) {
            return new Insets(0, 0, 0, 0);
        }
        return new Insets(2, 2, 2, 2);
    }

    private static final Logger logger = Logger.getLogger(HTextLook.class.getName());
}
