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
    public HRangeValue() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public HRangeValue(int orientation, int minimum, int maximum, int value,
            int x, int y, int width, int height) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public HRangeValue(int orientation, int minimum, int maximum, int value) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public static void setDefaultLook(HRangeLook look) {
        DefaultLook = look;
    }

    public static HRangeLook getDefaultLook() {
        if (DefaultLook == null)
            org.videolan.Logger.unimplemented("", "getDefaultLook");
        return DefaultLook;
    }

    public void setMove(int keyCode, HNavigable target) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public HNavigable getMove(int keyCode) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return null;
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public boolean isSelected() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return false;
    }

    public void setGainFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public void setLoseFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public HSound getGainFocusSound() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return null;
    }

    public HSound getLoseFocusSound() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return null;
    }

    public void addHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public void removeHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public int[] getNavigationKeys() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return null;
    }

    public void processHFocusEvent(HFocusEvent evt) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public void setUnitIncrement(int increment) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public int getUnitIncrement() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return 0;
    }

    public void setBlockIncrement(int increment) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public int getBlockIncrement() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return 0;
    }

    public void addAdjustmentListener(HAdjustmentListener l) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public void removeAdjustmentListener(HAdjustmentListener l) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public void setAdjustmentSound(HSound sound) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public HSound getAdjustmentSound() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return null;
    }

    public boolean getAdjustMode() {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
        return false;
    }

    public void setAdjustMode(boolean adjust) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    public void processHAdjustmentEvent(HAdjustmentEvent evt) {
        org.videolan.Logger.unimplemented(HRangeValue.class.getName(), "");
    }

    private static HRangeLook DefaultLook = null;

    private static final long serialVersionUID = -7809155734787063596L;
}
