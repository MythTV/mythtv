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

import org.havi.ui.event.HAdjustmentEvent;
import org.havi.ui.event.HAdjustmentListener;
import org.havi.ui.event.HFocusEvent;
import org.havi.ui.event.HFocusListener;

public class HRangeValue extends HRange implements HAdjustmentValue {
    public HRangeValue()
    {
        throw new Error("Not implemented");
    }

    public HRangeValue(int orientation, int minimum, int maximum, int value,
            int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public HRangeValue(int orientation, int minimum, int maximum, int value)
    {
        throw new Error("Not implemented");
    }

    public static void setDefaultLook(HRangeLook look)
    {
        throw new Error("Not implemented");
    }

    public static HRangeLook getDefaultLook()
    {
        throw new Error("Not implemented");
    }

    public void setMove(int keyCode, HNavigable target)
    {
        throw new Error("Not implemented");
    }

    public HNavigable getMove(int keyCode)
    {
        throw new Error("Not implemented");
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right)
    {
        throw new Error("Not implemented");
    }

    public boolean isSelected()
    {
        throw new Error("Not implemented");
    }

    public void setGainFocusSound(HSound sound)
    {
        throw new Error("Not implemented");
    }

    public void setLoseFocusSound(HSound sound)
    {
        throw new Error("Not implemented");
    }

    public HSound getGainFocusSound()
    {
        throw new Error("Not implemented");
    }

    public HSound getLoseFocusSound()
    {
        throw new Error("Not implemented");
    }

    public void addHFocusListener(HFocusListener l)
    {
        throw new Error("Not implemented");
    }

    public void removeHFocusListener(HFocusListener l)
    {
        throw new Error("Not implemented");
    }

    public int[] getNavigationKeys()
    {
        throw new Error("Not implemented");
    }

    public void processHFocusEvent(HFocusEvent evt)
    {
        throw new Error("Not implemented");
    }

    public void setUnitIncrement(int increment)
    {
        throw new Error("Not implemented");
    }

    public int getUnitIncrement()
    {
        throw new Error("Not implemented");
    }

    public void setBlockIncrement(int increment)
    {
        throw new Error("Not implemented");
    }

    public int getBlockIncrement()
    {
        throw new Error("Not implemented");
    }

    public void addAdjustmentListener(HAdjustmentListener l)
    {
        throw new Error("Not implemented");
    }

    public void removeAdjustmentListener(HAdjustmentListener l)
    {
        throw new Error("Not implemented");
    }

    public void setAdjustmentSound(HSound sound)
    {
        throw new Error("Not implemented");
    }

    public HSound getAdjustmentSound()
    {
        throw new Error("Not implemented");
    }

    public boolean getAdjustMode()
    {
        throw new Error("Not implemented");
    }

    public void setAdjustMode(boolean adjust)
    {
        throw new Error("Not implemented");
    }

    public void processHAdjustmentEvent(HAdjustmentEvent evt)
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = -7809155734787063596L;
}
