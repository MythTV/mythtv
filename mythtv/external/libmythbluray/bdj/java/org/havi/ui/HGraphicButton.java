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
    public HGraphicButton()
    {
        throw new Error("Not implemented");
    }

    public HGraphicButton(Image image, int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public HGraphicButton(Image imageNormal, Image imageFocused,
            Image imageActioned, int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public HGraphicButton(Image image)
    {
        throw new Error("Not implemented");
    }

    public HGraphicButton(Image imageNormal, Image imageFocused,
            Image imageActioned)
    {
        throw new Error("Not implemented");
    }

    public static void setDefaultLook(HGraphicLook hlook)
    {
        throw new Error("Not implemented");
    }

    public static HGraphicLook getDefaultLook()
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

    public void addHActionListener(HActionListener l)
    {
        throw new Error("Not implemented");
    }

    public void removeHActionListener(HActionListener l)
    {
        throw new Error("Not implemented");
    }

    public void setActionCommand(String command)
    {
        throw new Error("Not implemented");
    }

    public void setActionSound(HSound sound)
    {
        throw new Error("Not implemented");
    }

    public HSound getActionSound()
    {
        throw new Error("Not implemented");
    }

    public void processHActionEvent(HActionEvent evt)
    {
        throw new Error("Not implemented");
    }

    public String getActionCommand()
    {
        throw new Error("Not implemented");
    }

    private static final long serialVersionUID = 5167775411684840800L;
}
