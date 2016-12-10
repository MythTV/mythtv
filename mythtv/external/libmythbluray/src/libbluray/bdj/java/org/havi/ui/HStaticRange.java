/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2016  Petri Hintukainen
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

import org.videolan.Logger;

public class HStaticRange extends HVisible implements HNoInputPreferred,
        HOrientable {

    public HStaticRange() {
        this(0, 0, 100, 0, 0, 0, -1, -1);
    }

    public HStaticRange(int orientation, int minimum, int maximum, int value) {
        this(orientation, minimum, maximum, value, 0, 0, -1, -1);
    }

    public HStaticRange(int orientation, int minimum, int maximum, int value,
            int x, int y, int width, int height) {

        super(getDefaultLook(), x, y, width, height);

        logger.unimplemented("");

        setOrientation(orientation);
        setRange(minimum, maximum);
        setValue(value);
        setHorizontalAlignment(HALIGN_CENTER);
        setVerticalAlignment(VALIGN_CENTER);
    }

    public void setLook(HLook hlook) throws HInvalidLookException {
        if (hlook == null || (hlook instanceof HRangeLook)) {
            super.setLook(hlook);
        } else {
            logger.error("invalid look");
            throw new HInvalidLookException();
        }
    }

    public static void setDefaultLook(HRangeLook look) {
        DefaultLook = look;
    }

    public static HRangeLook getDefaultLook() {
        if (DefaultLook == null)
            Logger.unimplemented("", "getDefaultLook");
        return DefaultLook;
    }

    public int getOrientation() {
        return orientation;
    }

    public void setOrientation(int orient) {
        logger.unimplemented("setOrientation");
        this.orientation = orient;
    }

    public boolean setRange(int minimum, int maximum) {
        logger.unimplemented("setRange");
        return false;
    }

    public int getMinValue() {
        return minimum;
    }

    public int getMaxValue() {
        return maximum;
    }

    public void setValue(int value) {
        logger.unimplemented("setValue");
    }

    public int getValue() {
        return value;
    }

    public void setThumbOffsets(int minOffset, int maxOffset) {
        logger.unimplemented("setThumbOffset");
    }

    public int getThumbMinOffset() {
        return minOffset;
    }

    public int getThumbMaxOffset() {
        return maxOffset;
    }

    public void setBehavior(int behavior) {
        logger.unimplemented("");
        this.behavior = behavior;
    }

    public int getBehavior() {
        return behavior;
    }

    private int value = 0;
    private int minimum = 0;
    private int maximum = 100;
    private int orientation = 0;
    private int behavior = SLIDER_BEHAVIOR;
    private int minOffset = 0;
    private int maxOffset = 0;

    public final static int SLIDER_BEHAVIOR = 0;
    public final static int SCROLLBAR_BEHAVIOR = 1;

    private static HRangeLook DefaultLook = null;

    private static final long serialVersionUID = 3871722305722412744L;

    private static final Logger logger = Logger.getLogger(HStaticRange.class.getName());
}
