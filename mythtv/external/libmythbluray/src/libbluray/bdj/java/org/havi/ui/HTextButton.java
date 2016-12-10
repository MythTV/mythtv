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

import java.awt.Color;
import java.awt.Font;
import org.havi.ui.event.HActionEvent;
import org.havi.ui.event.HActionListener;
import org.havi.ui.event.HFocusEvent;
import org.havi.ui.event.HFocusListener;

public class HTextButton extends HText implements HActionable {
    public HTextButton() {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public HTextButton(String text, int x, int y, int width, int height) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public HTextButton(String text, int x, int y, int width, int height,
            Font font, Color foreground, Color background,
            HTextLayoutManager tlm) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public HTextButton(String text) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public HTextButton(String text, Font font, Color foreground,
            Color background, HTextLayoutManager tlm) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public static void setDefaultLook(HTextLook hlook) {
        DefaultLook = hlook;
    }

    public static HTextLook getDefaultLook() {
        if (DefaultLook == null)
            org.videolan.Logger.unimplemented("", "getDefaultLook");
        return DefaultLook;
    }

    public void setMove(int keyCode, HNavigable target) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public HNavigable getMove(int keyCode)
    {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
        return null;
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public boolean isSelected() {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
        return false;
    }

    public void setGainFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public void setLoseFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public HSound getGainFocusSound() {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
        return null;
    }

    public HSound getLoseFocusSound() {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
        return null;
    }

    public void addHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public void removeHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public int[] getNavigationKeys() {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
        return null;
    }

    public void processHFocusEvent(HFocusEvent evt) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public void addHActionListener(HActionListener l) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public void removeHActionListener(HActionListener l) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public void setActionCommand(String command) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public void setActionSound(HSound sound) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public HSound getActionSound() {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
        return null;
    }

    public void processHActionEvent(HActionEvent evt) {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
    }

    public String getActionCommand() {
        org.videolan.Logger.unimplemented(HTextButton.class.getName(), "");
        return null;
    }

    private static HTextLook DefaultLook = null;

    private static final long serialVersionUID = 7563558661769889160L;
}
