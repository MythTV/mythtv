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
    public HListGroup() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public HListGroup(HListElement[] items) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public HListGroup(HListElement[] items, int x, int y, int width, int height) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void setLook(HLook hlook) throws HInvalidLookException {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public static void setDefaultLook(HListGroupLook look) {
        DefaultLook = look;
    }

    public static HListGroupLook getDefaultLook() {
        if (DefaultLook == null)
            org.videolan.Logger.unimplemented("", "getDefaultLook");
        return DefaultLook;
    }

    public HListElement[] getListContent() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public void setListContent(HListElement[] elements) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void addItem(HListElement item, int index) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void addItems(HListElement[] items, int index)  {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public HListElement getItem(int index) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public int getIndex(HListElement item) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return 0;
    }

    public int getNumItems() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return 0;
    }

    public HListElement removeItem(int index) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public void removeAllItems() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public int getCurrentIndex() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return 0;
    }

    public HListElement getCurrentItem() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public boolean setCurrentItem(int index) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return false;
    }

    public int[] getSelectionIndices() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public HListElement[] getSelection() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public void clearSelection() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public int getNumSelected() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return 0;
    }

    public boolean getMultiSelection() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return false;
    }

    public void setMultiSelection(boolean multi) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void setItemSelected(int index, boolean sel) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public boolean isItemSelected(int index) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return false;
    }

    public int getScrollPosition() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return 0;
    }

    public void setScrollPosition(int scroll) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public Dimension getIconSize() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public void setIconSize(Dimension size) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public Dimension getLabelSize() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public void setLabelSize(Dimension size) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void setMove(int keyCode, HNavigable target) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public HNavigable getMove(int keyCode) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public boolean isSelected() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return false;
    }

    public void setGainFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void setLoseFocusSound(HSound sound) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public HSound getGainFocusSound() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public HSound getLoseFocusSound() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public void addHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void removeHFocusListener(HFocusListener l) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public int[] getNavigationKeys() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public void processHFocusEvent(HFocusEvent evt) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public int getOrientation() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return 0;
    }

    public void setOrientation(int orient) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void addItemListener(org.havi.ui.event.HItemListener l) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void removeItemListener(org.havi.ui.event.HItemListener l) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void setSelectionSound(HSound sound) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public HSound getSelectionSound() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return null;
    }

    public boolean getSelectionMode() {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
        return false;
    }

    public void setSelectionMode(boolean edit) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public void processHItemEvent(HItemEvent evt) {
        org.videolan.Logger.unimplemented(HListGroup.class.getName(), "");
    }

    public static final int ITEM_NOT_FOUND = -1;
    public static final int ADD_INDEX_END = -1;
    public static final int DEFAULT_LABEL_WIDTH = -1;
    public static final int DEFAULT_LABEL_HEIGHT = -2;
    public static final int DEFAULT_ICON_WIDTH = -3;
    public static final int DEFAULT_ICON_HEIGHT = -4;

    private static HListGroupLook DefaultLook = null;

    private static final long serialVersionUID = 6012900970046475431L;
}
