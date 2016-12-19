/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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
import java.util.Hashtable;
import java.util.Map;

import org.videolan.Logger;

public class HVisible extends HComponent implements HState {
    public HVisible() {
        this(null);
    }

    public HVisible(HLook hlook) {
        this(hlook, 0, 0, 0, 0);
    }

    public HVisible(HLook hlook, int x, int y, int width, int height) {
        super(x, y, width, height);
        hLook = hlook;
        TextLayoutManager = new HDefaultTextLayoutManager();

        content = new Object[LAST_STATE - FIRST_STATE + 1];
    }

    public boolean isFocusTraversable() {
        if (this instanceof HNavigable || this instanceof HSelectionInputPreferred) {
            return true;
        }
        return false;
    }

    public void paint(Graphics g) {
        if (hLook != null)
            hLook.showLook(g, this, InteractionState);
    }

    public void update(Graphics g) {
        g.setColor(getBackground());
        paint(g);
    }

    public void setTextContent(String string, int state) {
        setContentImpl(string, state, TEXT_CONTENT_CHANGE);
    }

    public void setGraphicContent(Image image, int state) {
        setContentImpl(image, state, GRAPHIC_CONTENT_CHANGE);
    }

    public void setAnimateContent(Image[] imageArray, int state) {
        setContentImpl(imageArray, state, ANIMATE_CONTENT_CHANGE);
    }

    public void setContent(Object object, int state) {
        setContentImpl(object, state, CONTENT_CHANGE);
    }

    private void setContentImpl(Object object, int state, int hint) {
        int states = LAST_STATE - FIRST_STATE + 1;

        Object[] oldData = new Object[states + 1];
        oldData[0] = new Integer(state);
        for (int i = 0; i < states; i++) {
            oldData[(i + 1)] = content[i];
        }

        if (state == ALL_STATES) {
            for (int i = 0; i < states; i++)
                content[i] = object;
        } else {
            if (state < FIRST_STATE || state > LAST_STATE) {
                logger.info("state out of range in getContext()");
                throw new IllegalArgumentException("state out of range");
            }
            content[state - FIRST_STATE] = object;
        }

        visibleChanged(hint, oldData);
    }

    private void visibleChanged(int hint, Object data) {
        if (hLook == null) {
            return;
        }
        HChangeData[] changeData = null;
        if (data != null) {
            changeData = new HChangeData[1];
            changeData[0] = new HChangeData(hint, data);
        }
        hLook.widgetChanged(this, changeData);
    }

    public String getTextContent(int state)
    {
        return (String)getContent(state);
    }

    public Image getGraphicContent(int state)
    {
        return (Image)getContent(state);
    }

    public Image[] getAnimateContent(int state)
    {
        return (Image[])getContent(state);
    }

    public Object getContent(int state)
    {
        if (state == ALL_STATES) {
            logger.info("ALL_STATES not supported in getContent()");
            throw new IllegalArgumentException("ALL_STATES not supported in getContent()");
        }

        if (state < FIRST_STATE || state > LAST_STATE) {
            logger.info("state out of range in getContent()");
            throw new IllegalArgumentException("state out of range");
        }

        return content[state - FIRST_STATE];
    }

    public void setLook(HLook hlook) throws HInvalidLookException {
        hLook = hlook;
    }

    public HLook getLook() {
        return hLook;
    }

    public Dimension getPreferredSize() {
        if (hLook != null) {
            return hLook.getPreferredSize(this);
        }
        return getSize();
    }

    public Dimension getMaximumSize() {
        if (hLook != null) {
            return hLook.getMaximumSize(this);
        }
        return getSize();
    }

    public Dimension getMinimumSize() {
        if (hLook != null) {
            return hLook.getMinimumSize(this);
        }
        return getSize();
    }

    public void requestFocus() {
        super.requestFocus();

        if (isFocusTraversable())
            setInteractionState(InteractionState | FOCUSED_STATE_BIT);
    }

    protected void setInteractionState(int state) {
        if (InteractionState == state)
            return;

        Integer oldState = new Integer(InteractionState);
        InteractionState = state;
        visibleChanged(STATE_CHANGE, oldState);
    }

    public int getInteractionState() {
        return InteractionState;
    }

    public void setTextLayoutManager(HTextLayoutManager manager) {
        TextLayoutManager = manager;
    }

    public HTextLayoutManager getTextLayoutManager() {
        return TextLayoutManager;
    }

    public int getBackgroundMode() {
        return BackgroundMode;
    }

    public void setBackgroundMode(int mode) {
        if (mode != BACKGROUND_FILL && mode != NO_BACKGROUND_FILL) {
            logger.info("mode out of range in setBackgroundMode()");
            throw new IllegalArgumentException("Unknown background fill mode");
        }
        BackgroundMode = mode;
    }

    public boolean isOpaque() {
        if (hLook == null) {
            return false;
        }
        return hLook.isOpaque(this);
    }

    public void setDefaultSize(Dimension defaultSize) {
        this.defaultSize = defaultSize;
    }

    public Dimension getDefaultSize() {
        return defaultSize;
    }

    public Object getLookData(Object key) {
        if (lookData == null || !lookData.containsKey(key)) {
            return null;
        }
        return lookData.get(key);
    }

    public void setLookData(Object key, Object data) {
        if (lookData == null) {
            lookData = new Hashtable();
        }

        if (lookData.containsKey(key)) {
            lookData.remove(key);
        }

        if (data == null) {
            return;
        }

        lookData.put(key, data);
    }

    public void setHorizontalAlignment(int halign) {
        if (halign != HALIGN_LEFT && halign != HALIGN_CENTER &&
            halign != HALIGN_RIGHT && halign != HALIGN_JUSTIFY) {
            logger.info("align out of range in setHorizontalAlignment()");
            throw new IllegalArgumentException("Unknown halign");
        }

        this.halign = halign;
        visibleChanged(UNKNOWN_CHANGE, new Integer(UNKNOWN_CHANGE));
    }

    public void setVerticalAlignment(int valign) {
        if (valign != VALIGN_TOP && valign != VALIGN_CENTER &&
            valign != VALIGN_BOTTOM && valign != VALIGN_JUSTIFY) {
            logger.info("align out of range in setVerticalAlignment()");
            throw new IllegalArgumentException("Unknown valign");
        }

        this.valign = valign;
        visibleChanged(UNKNOWN_CHANGE, new Integer(UNKNOWN_CHANGE));
    }

    public int getHorizontalAlignment() {
        return halign;
    }

    public int getVerticalAlignment() {
        return valign;
    }

    public void setResizeMode(int resize) {
        if (resize != RESIZE_NONE && resize != RESIZE_PRESERVE_ASPECT &&
            resize != RESIZE_ARBITRARY) {
            logger.info("resize out of range in setResizeMode()");
            throw new IllegalArgumentException("Unknown resize mode");
        }

        resizeMode = resize;
        visibleChanged(UNKNOWN_CHANGE, new Integer(UNKNOWN_CHANGE));
    }

    public int getResizeMode() {
        return resizeMode;
    }

    public void setEnabled(boolean b) {
        super.setEnabled(b);
        if (b) {
            setInteractionState(InteractionState & (~DISABLED_STATE_BIT));
        } else {
            setInteractionState(InteractionState | DISABLED_STATE_BIT);
        }
    }

    public void setBordersEnabled(boolean enable) {
        if (enable == BordersEnabled)
            return;

        if ((hLook instanceof HAnimateLook) ||
            (hLook instanceof HGraphicLook) ||
            (hLook instanceof HTextLook)) {
            Boolean oldMode = new Boolean(BordersEnabled);
            BordersEnabled = enable;
            visibleChanged(BORDER_CHANGE, oldMode);
        }
    }

    public boolean getBordersEnabled() {
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
    public static final Dimension NO_DEFAULT_SIZE = new Dimension(NO_DEFAULT_WIDTH, NO_DEFAULT_HEIGHT);

    private static final long serialVersionUID = -2076075723286676347L;

    private HLook hLook;
    private Map lookData;
    private int BackgroundMode = NO_BACKGROUND_FILL;
    private boolean BordersEnabled = true;
    private int InteractionState = 0;
    private int halign = HALIGN_LEFT;
    private int valign = VALIGN_TOP;
    private int resizeMode = RESIZE_NONE;
    private HTextLayoutManager TextLayoutManager = null;
    private Object content[];
    private Dimension defaultSize = NO_DEFAULT_SIZE;

    private static final Logger logger = Logger.getLogger(HVisible.class.getName());
}
