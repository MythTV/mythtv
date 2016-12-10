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

import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Insets;

import org.videolan.StrUtil;

public class HDefaultTextLayoutManager implements HTextLayoutManager {
    static final Insets ZERO_INSETS = new Insets(0, 0, 0, 0);

    public HDefaultTextLayoutManager() {
    }

    public Dimension getMinimumSize(HVisible hvisible)
    {
        Dimension size = new Dimension(0, 0);

        for (int state = HVisible.FIRST_STATE; state <= HVisible.LAST_STATE; state++) {
            String text = hvisible.getTextContent(state);
            if (text != null && !text.equals("")) {
                String[] lines = StrUtil.split(text, '\n');

                Graphics g = hvisible.getGraphics();
                if (g == null)
                    continue;
                FontMetrics fontMetrics = g.getFontMetrics(hvisible.getFont());
                g.dispose();

                int lineHeight = fontMetrics.getHeight();
                int textHeight = lines.length * lineHeight;
                if (textHeight > size.height) {
                    size.height = textHeight;
                }

                for (int i = 0; i < lines.length; i++) {
                    int lineWidth = fontMetrics.stringWidth(lines[i]);

                    if (lineWidth > size.width)
                        size.width = lineWidth;
                }
            }
        }
        return size;
    }

    public Dimension getMaximumSize(HVisible hvisible) {
        return new Dimension(Short.MAX_VALUE, Short.MAX_VALUE);
    }

    public Dimension getPreferredSize(HVisible hvisible) {
        return getMinimumSize(hvisible);
    }

    public void render(String markedUpString, Graphics g, HVisible v, Insets insets) {
        if (markedUpString == null)
            return;

        if (insets == null)
            insets = ZERO_INSETS;

        String[] lines = StrUtil.split(markedUpString, '\n');

        Font font = v.getFont();
        g.setFont(font);
        FontMetrics fontMetrics = g.getFontMetrics();

        int ascent = fontMetrics.getAscent();
        int descent = Math.abs(fontMetrics.getDescent());
        int leading = fontMetrics.getLeading();
        int stringHeight = ascent + descent + leading;
        int textHeight = lines.length * stringHeight;

        int x = 0;
        int y = 0;
        for (int i = 0; i < lines.length; i++) {
            int lineWidth = fontMetrics.stringWidth(lines[i]);

            switch (v.getHorizontalAlignment()) {
                case HVisible.HALIGN_LEFT:
                    x = insets.left;
                    break;
                case HVisible.HALIGN_RIGHT:
                    x = v.getWidth() - lineWidth - insets.right;
                    break;
                case HVisible.HALIGN_CENTER:
                case HVisible.HALIGN_JUSTIFY:
                    x = insets.left + (v.getWidth() - insets.left - insets.right - lineWidth) / 2;
                    break;
            }

            switch (v.getVerticalAlignment()) {

            case HVisible.VALIGN_TOP:
                y = insets.top + ascent + descent + i * stringHeight;
                break;
            case HVisible.VALIGN_BOTTOM:
                y = v.getHeight() - insets.bottom - textHeight +
                    ascent + descent + i * stringHeight;
                break;
            case HVisible.VALIGN_CENTER:
            case HVisible.VALIGN_JUSTIFY:
                y = insets.top +
                    (v.getHeight() - insets.top - insets.bottom - textHeight) / 2 +
                    ascent + descent + i * stringHeight;
                break;
            }

            g.drawString(lines[i], x, y);
        }
    }
}
