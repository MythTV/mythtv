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

import java.awt.Image;

import org.havi.ui.event.HFocusEvent;
import org.havi.ui.event.HFocusListener;

import org.videolan.BDJXletContext;
import org.videolan.Logger;

public class HIcon extends HStaticIcon implements HNavigable {
    public HIcon() {
        org.videolan.Logger.unimplemented(HIcon.class.getName(), "");
    }

    public HIcon(Image image) {
        org.videolan.Logger.unimplemented(HIcon.class.getName(), "");
    }

    public HIcon(Image image, int x, int y, int width, int height) {
        org.videolan.Logger.unimplemented(HIcon.class.getName(), "");
    }

    public HIcon(Image imageNormal, Image imageFocus, int x, int y, int width,
            int height) {
        org.videolan.Logger.unimplemented(HIcon.class.getName(), "");
    }

    public static void setDefaultLook(HGraphicLook hlook) {
        BDJXletContext.setXletDefaultLook(PROPERTY_LOOK, hlook);
    }

    public static HGraphicLook getDefaultLook() {
        return (HGraphicLook)BDJXletContext.getXletDefaultLook(PROPERTY_LOOK, DEFAULT_LOOK);
    }

    public void setMove(int keyCode, HNavigable target) {
        Logger.unimplemented("", "");
    }

    public HNavigable getMove(int keyCode) {
        Logger.unimplemented("", "");
        return null;
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right) {
        Logger.unimplemented("", "");
    }

    public boolean isSelected() {
        Logger.unimplemented("", "");
        return false;
    }

    public void setGainFocusSound(HSound sound) {
        Logger.unimplemented("", "");
    }

    public void setLoseFocusSound(HSound sound) {
        Logger.unimplemented("", "");
    }

    public HSound getGainFocusSound() {
        Logger.unimplemented("", "");
        return null;
    }

    public HSound getLoseFocusSound() {
        Logger.unimplemented("", "");
        return null;
    }

    public void addHFocusListener(HFocusListener l) {
        Logger.unimplemented("", "");
    }

    public void removeHFocusListener(HFocusListener l) {
        Logger.unimplemented("", "");
    }

    public int[] getNavigationKeys() {
        Logger.unimplemented("", "");
        return null;
    }

    public void processHFocusEvent(HFocusEvent evt) {
        Logger.unimplemented("", "");
    }

    static final Class DEFAULT_LOOK = HGraphicLook.class;
    private static final String PROPERTY_LOOK = HIcon.class.getName();

    private static final long serialVersionUID = 2006124827619610922L;
}
