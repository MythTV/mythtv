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

package org.dvb.ui;

import java.awt.Insets;

import org.havi.ui.HVisible;

public class DVBTextLayoutManager implements org.havi.ui.HTextLayoutManager {

    public void render(String markedUpString, java.awt.Graphics g, HVisible v, Insets insets) {
        /* this class is defined in GEM specifications */
        logger.unimplemented("render(" + markedUpString + ")");
    }

    public DVBTextLayoutManager() {
        this(HORIZONTAL_START_ALIGN, VERTICAL_START_ALIGN, LINE_ORIENTATION_HORIZONTAL, START_CORNER_UPPER_LEFT, true,
             /* linespace = (point size of the default font for HVisible) + 7 */ 12 + 7,
             0, 56);
    }

    public DVBTextLayoutManager(int horizontalAlign, int verticalAlign,
            int lineOrientation, int startCorner, boolean wrap, int linespace,
            int letterspace, int horizontalTabSpace) {
        this.horizontalAlign = horizontalAlign;
        this.verticalAlign = verticalAlign;
        this.lineOrientation = lineOrientation;
        this.startCorner = startCorner;
        this.wrap = wrap;
        this.linespace = linespace;
        this.letterspace = letterspace;
        this.horizontalTabSpace = horizontalTabSpace;
    }

    public void setHorizontalAlign(int horizontalAlign) {
        this.horizontalAlign = horizontalAlign;
    }

    public void setVerticalAlign(int verticalAlign) {
        this.verticalAlign = verticalAlign;
    }

    public void setLineOrientation(int lineOrientation) {
        this.lineOrientation = lineOrientation;
    }

    public void setStartCorner(int startCorner) {
        this.startCorner = startCorner;
    }

    public void setTextWrapping(boolean wrap) {
        this.wrap = wrap;
    }

    public void setLineSpace(int lineSpace) {
        this.linespace = lineSpace;
    }

    public void setLetterSpace(int letterSpace) {
        this.letterspace = letterSpace;
    }

    public void setHorizontalTabSpacing(int horizontalTabSpace) {
        this.horizontalTabSpace = horizontalTabSpace;
    }

    public int getHorizontalAlign() {
        return horizontalAlign;
    }

    public int getVerticalAlign() {
        return verticalAlign;
    }

    public int getLineOrientation() {
        return lineOrientation;
    }

    public int getStartCorner() {
        return startCorner;
    }

    public boolean getTextWrapping() {
        return wrap;
    }

    public int getLineSpace() {
        return linespace;
    }

    public int getLetterSpace() {
        return letterspace;
    }

    public int getHorizontalTabSpacing() {
        return horizontalTabSpace;
    }

    public void setInsets(Insets insets) {
        this.insets = insets;
    }

    public Insets getInsets() {
        return this.insets;
    }

    public void addTextOverflowListener(TextOverflowListener listener) {
        logger.unimplemented("addTextOverflowListener");
    }

    public void removeTextOverflowListener(TextOverflowListener listener) {
        logger.unimplemented("removeTextOverflowListener");
    }

    private int horizontalAlign;
    private int verticalAlign;
    private int lineOrientation;
    private int startCorner;
    private boolean wrap;
    private int linespace;
    private int letterspace;
    private int horizontalTabSpace;
    private Insets insets = new Insets(0, 0, 0, 0);

    private static final org.videolan.Logger logger = org.videolan.Logger.getLogger(DVBTextLayoutManager.class.getName());

    public static final int HORIZONTAL_START_ALIGN = 1;
    public static final int HORIZONTAL_END_ALIGN = 2;
    public static final int HORIZONTAL_CENTER = 3;

    public static final int VERTICAL_START_ALIGN = 4;
    public static final int VERTICAL_END_ALIGN = 5;
    public static final int VERTICAL_CENTER = 6;

    public static final int LINE_ORIENTATION_HORIZONTAL = 10;
    public static final int LINE_ORIENTATION_VERTICAL = 11;

    public static final int START_CORNER_UPPER_LEFT = 20;
    public static final int START_CORNER_UPPER_RIGHT = 21;
    public static final int START_CORNER_LOWER_LEFT = 22;
    public static final int START_CORNER_LOWER_RIGHT = 23;
}
