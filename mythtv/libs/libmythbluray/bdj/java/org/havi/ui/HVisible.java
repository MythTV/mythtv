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

import java.awt.Graphics;
import java.awt.Image;
import java.awt.Dimension;

public class HVisible extends HComponent implements HState {
    public HVisible()
    {
        this(null);
    }

    public HVisible(HLook hlook)
    {
        this(hlook, 0, 0, 0, 0);
    }

    public HVisible(HLook hlook, int x, int y, int width, int height)
    {
        super(x, y, width, height);
        hLook = hlook;
    }

    public boolean isFocusTraversable()
    {
        throw new Error("Not implemented");
    }

    public void paint(Graphics g)
    {
        throw new Error("Not implemented");
    }

    public void update(Graphics g)
    {
        throw new Error("Not implemented");
    }

    public void setTextContent(String string, int state)
    {
        throw new Error("Not implemented");
    }

    public void setGraphicContent(Image image, int state)
    {
        throw new Error("Not implemented");
    }

    public void setAnimateContent(Image[] imageArray, int state)
    {
        throw new Error("Not implemented");
    }

    public void setContent(Object object, int state)
    {
        throw new Error("Not implemented");
    }

    public String getTextContent(int state)
    {
        throw new Error("Not implemented");
    }

    public Image getGraphicContent(int state)
    {
        throw new Error("Not implemented");
    }

    public Image[] getAnimateContent(int state)
    {
        throw new Error("Not implemented");
    }

    public Object getContent(int state)
    {
        throw new Error("Not implemented");
    }

    public void setLook(HLook hlook) throws HInvalidLookException
    {
        throw new Error("Not implemented");
    }

    public HLook getLook()
    {
        throw new Error("Not implemented");
    }

    public Dimension getPreferredSize()
    {
        throw new Error("Not implemented");
    }

    public Dimension getMaximumSize()
    {
        throw new Error("Not implemented");
    }

    public Dimension getMinimumSize()
    {
        throw new Error("Not implemented");
    }

    protected void setInteractionState(int state)
    {
        throw new Error("Not implemented");
    }

    public int getInteractionState()
    {
        throw new Error("Not implemented");
    }

    public void setTextLayoutManager(HTextLayoutManager manager)
    {
        throw new Error("Not implemented");
    }

    public HTextLayoutManager getTextLayoutManager()
    {
        throw new Error("Not implemented");
    }

    public int getBackgroundMode()
    {
        throw new Error("Not implemented");
    }

    public void setBackgroundMode(int mode)
    {
        throw new Error("Not implemented");
    }

    public boolean isOpaque()
    {
        throw new Error("Not implemented");
    }

    public void setDefaultSize(Dimension defaultSize)
    {
        throw new Error("Not implemented");
    }

    public Dimension getDefaultSize()
    {
        throw new Error("Not implemented");
    }

    public Object getLookData(Object key)
    {
        throw new Error("Not implemented");
    }

    public void setLookData(Object key, Object data)
    {
        throw new Error("Not implemented");
    }

    public void setHorizontalAlignment(int halign)
    {
        throw new Error("Not implemented");
    }

    public void setVerticalAlignment(int valign)
    {
        throw new Error("Not implemented");
    }

    public int getHorizontalAlignment()
    {
        throw new Error("Not implemented");
    }

    public int getVerticalAlignment()
    {
        throw new Error("Not implemented");
    }

    public void setResizeMode(int resize)
    {
        throw new Error("Not implemented");
    }

    public int getResizeMode()
    {
        throw new Error("Not implemented");
    }

    public void setEnabled(boolean b)
    {
        super.setEnabled(b);
    }

    public void setBordersEnabled(boolean enable)
    {
        BordersEnabled = enable;
    }

    public boolean getBordersEnabled()
    {
        return BordersEnabled;
    }

    public static final int HALIGN_LEFT = 0;
    public static final int HALIGN_CENTER = 1;
    public static final int HALIGN_RIGHT = 2;
    public static final int HALIGN_JUSTIFY = 3;

    public static final int VALIGN_TOP = 0;
    public static final int VALIGN_CENTER = 4;
    public static final int VALIGN_BOTTOM = 8;
    public static final int VALIGN_JUSTIFY = 12;

    public final static int RESIZE_NONE = 0;
    public static final int RESIZE_PRESERVE_ASPECT = 1;
    public static final int RESIZE_ARBITRARY = 2;

    public static final int NO_BACKGROUND_FILL = 0;
    public static final int BACKGROUND_FILL = 1;

    public static final int FIRST_CHANGE = 0;

    public static final int TEXT_CONTENT_CHANGE = 0;
    public static final int GRAPHIC_CONTENT_CHANGE = 1;
    public static final int ANIMATE_CONTENT_CHANGE = 2;
    public static final int CONTENT_CHANGE = 3;
    public static final int STATE_CHANGE = 4;
    public static final int CARET_POSITION_CHANGE = 5;
    public static final int ECHO_CHAR_CHANGE = 6;
    public static final int EDIT_MODE_CHANGE = 7;
    public static final int MIN_MAX_CHANGE = 8;
    public static final int THUMB_OFFSETS_CHANGE = 9;
    public static final int ORIENTATION_CHANGE = 10;
    public static final int TEXT_VALUE_CHANGE = 11;
    public static final int ITEM_VALUE_CHANGE = 12;
    public static final int ADJUSTMENT_VALUE_CHANGE = 13;
    public static final int LIST_CONTENT_CHANGE = 14;
    public static final int LIST_ICONSIZE_CHANGE = 15;
    public static final int LIST_LABELSIZE_CHANGE = 16;
    public static final int LIST_MULTISELECTION_CHANGE = 17;
    public static final int LIST_SCROLLPOSITION_CHANGE = 18;
    public static final int SIZE_CHANGE = 19;
    public static final int BORDER_CHANGE = 20;
    public static final int REPEAT_COUNT_CHANGE = 21;
    public static final int ANIMATION_POSITION_CHANGE = 22;
    public static final int LIST_SELECTION_CHANGE = 23;
    public static final int UNKNOWN_CHANGE = 24;
    public static final int LAST_CHANGE = UNKNOWN_CHANGE;

    public static final int NO_DEFAULT_WIDTH = -1;
    public static final int NO_DEFAULT_HEIGHT = -1;
    public static final Dimension NO_DEFAULT_SIZE = new Dimension(
            NO_DEFAULT_WIDTH, NO_DEFAULT_HEIGHT);

    private static final long serialVersionUID = -2076075723286676347L;

    private HLook hLook;
    private int BackgroundMode = NO_BACKGROUND_FILL;
    private boolean BordersEnabled = true;
    private int InteractionState = 0;
}
