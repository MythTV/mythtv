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

import org.havi.ui.event.HActionEvent;
import org.havi.ui.event.HActionListener;
import org.havi.ui.event.HFocusEvent;
import org.havi.ui.event.HFocusListener;

public class HGraphicButton extends HIcon implements HActionable {
    public HGraphicButton() {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public HGraphicButton(Image image, int x, int y, int width, int height) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public HGraphicButton(Image imageNormal, Image imageFocused,
            Image imageActioned, int x, int y, int width, int height) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public HGraphicButton(Image image) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public HGraphicButton(Image imageNormal, Image imageFocused,
            Image imageActioned) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public static void setDefaultLook(HGraphicLook hlook) {
        DefaultLook = hlook;
    }

    public static HGraphicLook getDefaultLook() {
        if (DefaultLook == null)
            org.videolan.Logger.unimplemented("", "getDefaultLook");
        return DefaultLook;
    }

    public void setMove(int keyCode, HNavigable target) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public HNavigable getMove(int keyCode) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
        return null;
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public boolean isSelected() {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
        return false;
    }

    public void setGainFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public void setLoseFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public HSound getGainFocusSound() {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
        return null;
    }

    public HSound getLoseFocusSound() {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
        return null;
    }

    public void addHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public void removeHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public int[] getNavigationKeys() {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
        return null;
    }

    public void processHFocusEvent(HFocusEvent evt) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public void addHActionListener(HActionListener l) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public void removeHActionListener(HActionListener l) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public void setActionCommand(String command) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public void setActionSound(HSound sound) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public HSound getActionSound() {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
        return null;
    }

    public void processHActionEvent(HActionEvent evt) {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
    }

    public String getActionCommand() {
        org.videolan.Logger.unimplemented(HGraphicButton.class.getName(), "");
        return "";
    }

    private static HGraphicLook DefaultLook = null;

    private static final long serialVersionUID = 5167775411684840800L;
}
