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

import java.awt.Font;
import java.awt.Color;
import org.havi.ui.event.HFocusEvent;
import org.havi.ui.event.HFocusListener;
import org.havi.ui.event.HKeyEvent;
import org.havi.ui.event.HKeyListener;
import org.havi.ui.event.HTextEvent;
import org.havi.ui.event.HTextListener;

public class HSinglelineEntry extends HVisible implements HTextValue {
    public HSinglelineEntry() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public HSinglelineEntry(String text, int x, int y, int width, int height,
            int maxChars, Font font, Color color) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public HSinglelineEntry(int x, int y, int width, int height, int maxChars) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public HSinglelineEntry(String text, int maxChars, Font font, Color color) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public HSinglelineEntry(int maxChars) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void setTextContent(String string, int state) {
        super.setTextContent(string, ALL_STATES);
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "setTextContent");
    }

    public String getTextContent(int state) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return "";
    }

    public int getCaretCharPosition() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return 0;
    }

    public int setCaretCharPosition(int position) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return 0;
    }

    public void setType(int type) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void setValidInput(char[] inputChars) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public boolean echoCharIsSet() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return false;
    }

    public char getEchoChar() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return 0;
    }

    public void setEchoChar(char c) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public static void setDefaultLook(HSinglelineEntryLook look) {
        DefaultLook = look;
    }

    public static HSinglelineEntryLook getDefaultLook() {
        if (DefaultLook == null)
            org.videolan.Logger.unimplemented("", "getDefaultLook");
        return DefaultLook;
    }

    public void setLook(HLook hlook) throws HInvalidLookException {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public boolean insertChar(char c) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return false;
    }

    public boolean deletePreviousChar() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return false;
    }

    public boolean deleteNextChar() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return false;
    }

    public void caretNextCharacter() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void caretPreviousCharacter() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void setMaxChars(int maxChars) {
        if (maxChars < 0)
            this.maxChars = 0;
        else
            this.maxChars = maxChars;
    }

    public int getMaxChars() {
        return maxChars;
    }

    public void setMove(int keyCode, HNavigable target) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public HNavigable getMove(int keyCode) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return null;
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public boolean isSelected() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return false;
    }

    public void setGainFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void setLoseFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public HSound getGainFocusSound() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return null;
    }

    public HSound getLoseFocusSound() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return null;
    }

    public void addHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void removeHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public int[] getNavigationKeys() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return null;
    }

    public void processHFocusEvent(HFocusEvent evt) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void addHKeyListener(HKeyListener l) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void removeHKeyListener(HKeyListener l) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void addHTextListener(HTextListener l) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void removeHTextListener(HTextListener l) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public boolean getEditMode() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return false;
    }

    public void setEditMode(boolean edit) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public int getType() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return 0;
    }

    public char[] getValidInput() {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
        return null;
    }

    public void processHTextEvent(HTextEvent evt) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    public void processHKeyEvent(HKeyEvent evt) {
        org.videolan.Logger.unimplemented(HSinglelineEntry.class.getName(), "");
    }

    private int maxChars;

    private static HSinglelineEntryLook DefaultLook = null;

    private static final long serialVersionUID = 7577783421311076636L;
}
