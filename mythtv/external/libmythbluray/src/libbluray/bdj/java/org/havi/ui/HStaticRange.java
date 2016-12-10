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

public class HStaticRange extends HVisible implements HNoInputPreferred,
        HOrientable {
    public HStaticRange()
    {
        org.videolan.Logger.unimplemented(HStaticRange.class.getName(), "");
    }

    public HStaticRange(int orientation, int minimum, int maximum, int value,
            int x, int y, int width, int height)
    {
        org.videolan.Logger.unimplemented(HStaticRange.class.getName(), "");
    }

    public HStaticRange(int orientation, int minimum, int maximum, int value)
    {
        org.videolan.Logger.unimplemented(HStaticRange.class.getName(), "");
    }

    public void setLook(HLook hlook) throws HInvalidLookException
    {
        throw new Error("Not implemented");
    }

    public static void setDefaultLook(HRangeLook look)
    {
        DefaultLook = look;
    }

    public static HRangeLook getDefaultLook()
    {
        if (DefaultLook == null)
            org.videolan.Logger.unimplemented("", "getDefaultLook");
        return DefaultLook;
    }

    public int getOrientation()
    {
        throw new Error("Not implemented");
    }

    public void setOrientation(int orient)
    {
        throw new Error("Not implemented");
    }

    public boolean setRange(int minimum, int maximum)
    {
        throw new Error("Not implemented");
    }

    public int getMinValue()
    {
        throw new Error("Not implemented");
    }

    public int getMaxValue()
    {
        throw new Error("Not implemented");
    }

    public void setValue(int value)
    {
        throw new Error("Not implemented");
    }

    public int getValue()
    {
        throw new Error("Not implemented");
    }

    public void setThumbOffsets(int minOffset, int maxOffset)
    {
        throw new Error("Not implemented");
    }

    public int getThumbMinOffset()
    {
        throw new Error("Not implemented");
    }

    public int getThumbMaxOffset()
    {
        throw new Error("Not implemented");
    }

    public void setBehavior(int behavior)
    {
        throw new Error("Not implemented");
    }

    public int getBehavior()
    {
        throw new Error("Not implemented");
    }

    public final static int SLIDER_BEHAVIOR = 0;
    public final static int SCROLLBAR_BEHAVIOR = 1;

    private static HRangeLook DefaultLook = null;

    private static final long serialVersionUID = 3871722305722412744L;
}
