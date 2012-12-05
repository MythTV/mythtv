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

import java.awt.Dimension;
import org.havi.ui.event.HFocusEvent;
import org.havi.ui.event.HFocusListener;
import org.havi.ui.event.HItemEvent;

public class HListGroup extends HVisible implements HItemValue {
    public HListGroup()
    {
        throw new Error("Not implemented");
    }

    public HListGroup(HListElement[] items)
    {
        throw new Error("Not implemented");
    }

    public HListGroup(HListElement[] items, int x, int y, int width, int height)
    {
        throw new Error("Not implemented");
    }

    public void setLook(HLook hlook) throws HInvalidLookException
    {
        throw new Error("Not implemented");
    }

    public static void setDefaultLook(HListGroupLook look)
    {
        throw new Error("Not implemented");
    }

    public static HListGroupLook getDefaultLook()
    {
        throw new Error("Not implemented");
    }

    public HListElement[] getListContent()
    {
        throw new Error("Not implemented");
    }

    public void setListContent(HListElement[] elements)
    {
        throw new Error("Not implemented");
    }

    public void addItem(HListElement item, int index)
    {
        throw new Error("Not implemented");
    }

    public void addItems(HListElement[] items, int index)
    {
        throw new Error("Not implemented");
    }

    public HListElement getItem(int index)
    {
        throw new Error("Not implemented");
    }

    public int getIndex(HListElement item)
    {
        throw new Error("Not implemented");
    }

    public int getNumItems()
    {
        throw new Error("Not implemented");
    }

    public HListElement removeItem(int index)
    {
        throw new Error("Not implemented");
    }

    public void removeAllItems()
    {
        throw new Error("Not implemented");
    }

    public int getCurrentIndex()
    {
        throw new Error("Not implemented");
    }

    public HListElement getCurrentItem()
    {
        throw new Error("Not implemented");
    }

    public boolean setCurrentItem(int index)
    {
        throw new Error("Not implemented");
    }

    public int[] getSelectionIndices()
    {
        throw new Error("Not implemented");
    }

    public HListElement[] getSelection()
    {
        throw new Error("Not implemented");
    }

    public void clearSelection()
    {
        throw new Error("Not implemented");
    }

    public int getNumSelected()
    {
        throw new Error("Not implemented");
    }

    public boolean getMultiSelection()
    {
        throw new Error("Not implemented");
    }

    public void setMultiSelection(boolean multi)
    {
        throw new Error("Not implemented");
    }

    public void setItemSelected(int index, boolean sel)
    {
        throw new Error("Not implemented");
    }

    public boolean isItemSelected(int index)
    {
        throw new Error("Not implemented");
    }

    public int getScrollPosition()
    {
        throw new Error("Not implemented");
    }

    public void setScrollPosition(int scroll)
    {
        throw new Error("Not implemented");
    }

    public Dimension getIconSize()
    {
        throw new Error("Not implemented");
    }

    public void setIconSize(Dimension size)
    {
        throw new Error("Not implemented");
    }

    public Dimension getLabelSize()
    {
        throw new Error("Not implemented");
    }

    public void setLabelSize(Dimension size)
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

    public int getOrientation()
    {
        throw new Error("Not implemented");
    }

    public void setOrientation(int orient)
    {
        throw new Error("Not implemented");
    }

    public void addItemListener(org.havi.ui.event.HItemListener l)
    {
        throw new Error("Not implemented");
    }

    public void removeItemListener(org.havi.ui.event.HItemListener l)
    {
        throw new Error("Not implemented");
    }

    public void setSelectionSound(HSound sound)
    {
        throw new Error("Not implemented");
    }

    public HSound getSelectionSound()
    {
        throw new Error("Not implemented");
    }

    public boolean getSelectionMode()
    {
        throw new Error("Not implemented");
    }

    public void setSelectionMode(boolean edit)
    {
        throw new Error("Not implemented");
    }

    public void processHItemEvent(HItemEvent evt)
    {
        throw new Error("Not implemented");
    }

    public static final int ITEM_NOT_FOUND = -1;
    public static final int ADD_INDEX_END = -1;
    public static final int DEFAULT_LABEL_WIDTH = -1;
    public static final int DEFAULT_LABEL_HEIGHT = -2;
    public static final int DEFAULT_ICON_WIDTH = -3;
    public static final int DEFAULT_ICON_HEIGHT = -4;

    private static final long serialVersionUID = 6012900970046475431L;
}
