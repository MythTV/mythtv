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
    public HSinglelineEntry()
    {
        throw new Error("Not implemented");
    }

    public HSinglelineEntry(String text, int x, int y, int width, int height,
            int maxChars, Font font, Color color)
    {
        throw new Error("Not implemented");
    }

    public HSinglelineEntry(int x, int y, int width, int height, int maxChars)
    {
        throw new Error("Not implemented");
    }

    public HSinglelineEntry(String text, int maxChars, Font font, Color color)
    {
        throw new Error("Not implemented");
    }

    public HSinglelineEntry(int maxChars)
    {
        throw new Error("Not implemented");
    }

    public void setTextContent(String string, int state)
    {
        throw new Error("Not implemented");
    }

    public String getTextContent(int state)
    {
        throw new Error("Not implemented");
    }

    public int getCaretCharPosition()
    {
        throw new Error("Not implemented");
    }

    public int setCaretCharPosition(int position)
    {
        throw new Error("Not implemented");
    }

    public void setType(int type)
    {
        throw new Error("Not implemented");
    }

    public void setValidInput(char[] inputChars)
    {
        throw new Error("Not implemented");
    }

    public boolean echoCharIsSet()
    {
        throw new Error("Not implemented");
    }

    public char getEchoChar()
    {
        throw new Error("Not implemented");
    }

    public void setEchoChar(char c)
    {
        throw new Error("Not implemented");
    }

    public static void setDefaultLook(HSinglelineEntryLook look)
    {
        throw new Error("Not implemented");
    }

    public static HSinglelineEntryLook getDefaultLook()
    {
        throw new Error("Not implemented");
    }

    public void setLook(HLook hlook) throws HInvalidLookException
    {
        throw new Error("Not implemented");
    }

    public boolean insertChar(char c)
    {
        throw new Error("Not implemented");
    }

    public boolean deletePreviousChar()
    {
        throw new Error("Not implemented");
    }

    public boolean deleteNextChar()
    {
        throw new Error("Not implemented");
    }

    public void caretNextCharacter()
    {
        throw new Error("Not implemented");
    }

    public void caretPreviousCharacter()
    {
        throw new Error("Not implemented");
    }

    public void setMaxChars(int maxChars)
    {
        throw new Error("Not implemented");
    }

    public int getMaxChars()
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

    public void addHKeyListener(HKeyListener l)
    {
        throw new Error("Not implemented");
    }

    public void removeHKeyListener(HKeyListener l)
    {
        throw new Error("Not implemented");
    }

    public void addHTextListener(HTextListener l)
    {
        throw new Error("Not implemented");
    }

    public void removeHTextListener(HTextListener l)
    {
        throw new Error("Not implemented");
    }

    public boolean getEditMode()
    {
        throw new Error("Not implemented");
    }

    public void setEditMode(boolean edit)
    {
        throw new Error("Not implemented");
    }

    public int getType()
    {
        throw new Error("Not implemented");
    }

    public char[] getValidInput()
    {
        throw new Error("Not implemented");
    }

    public void processHTextEvent(HTextEvent evt)
    {
        throw new Error("Not implemented");
    }

    public void processHKeyEvent(HKeyEvent evt)
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = 7577783421311076636L;
}
