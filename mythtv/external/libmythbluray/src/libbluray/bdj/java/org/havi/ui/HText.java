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

import java.awt.Color;
import java.awt.Font;

import org.havi.ui.event.HFocusEvent;
import org.havi.ui.event.HFocusListener;

import org.videolan.BDJXletContext;
import org.videolan.Logger;

public class HText extends HStaticText implements HNavigable {
    public HText() {
        this(null);
    }

    public HText(String text) {
        this(text, text, 0, 0, 0, 0);
    }

    public HText(String textNormal, String textFocus) {
        this(textNormal, textFocus, 0, 0, 0, 0);
    }

    public HText(String text, int x, int y, int width, int height) {
        this(text, text, x, y, width, height);
    }

    public HText(String textNormal, String textFocus, int x, int y, int width,
            int height) {
        super(textNormal, x, y, width, height);
        try {
            setLook(getDefaultLook());
        } catch (HInvalidLookException e) {
            logger.error("failed setting default look");
        }

        if (textFocus != null) {
            super.setTextContent(textFocus, FOCUSED_STATE);
            super.setTextContent(textFocus, ACTIONED_FOCUSED_STATE);
            super.setTextContent(textFocus, DISABLED_FOCUSED_STATE);
            super.setTextContent(textFocus, DISABLED_ACTIONED_FOCUSED_STATE);
        }
    }

    public HText(String text, Font font, Color foreground, Color background,
            HTextLayoutManager tlm) {
        this(text, text, 0, 0, 0, 0, font, foreground, background, tlm);
    }

    public HText(String textNormal, String textFocus, Font font,
            Color foreground, Color background, HTextLayoutManager tlm) {
        this(textNormal, textFocus, 0, 0, 0, 0, font, foreground, background, tlm);
    }

    public HText(String text, int x, int y, int width, int height, Font font,
            Color foreground, Color background, HTextLayoutManager tlm) {
        this(text, text, x, y, width, height, font, foreground, background, tlm);
    }

    public HText(String textNormal, String textFocus, int x, int y, int width,
            int height, Font font, Color foreground, Color background,
            HTextLayoutManager tlm) {
        this(textNormal, textFocus, x, y, width, height);
        setFont(font);
        setForeground(foreground);
        setBackground(background);
        setTextLayoutManager(tlm);
    }

    public void setMove(int keyCode, HNavigable target) {
        logger.unimplemented("setMove");
    }

    public HNavigable getMove(int keyCode) {
        logger.unimplemented("getMove");
        return this;
    }

    public boolean isFocusable() {
        return true;
    }

    public void setFocusTraversal(HNavigable up, HNavigable down,
            HNavigable left, HNavigable right) {
        logger.unimplemented("setFocusTraversal");
    }

    public boolean isSelected() {
        logger.unimplemented("isSelected");
        return false;
    }

    public void setGainFocusSound(HSound sound) {
        logger.unimplemented("setGainFocusSound");
    }

    public void setLoseFocusSound(HSound sound) {
        logger.unimplemented("setLoseFocusSound");
    }

    public HSound getGainFocusSound() {
        logger.unimplemented("getGainFocusSound");
        return null;
    }

    public HSound getLoseFocusSound() {
        logger.unimplemented("getLoseFocusSound");
        return null;
    }

    public void addHFocusListener(HFocusListener l) {
        logger.unimplemented("addHFocusListener");
    }

    public void removeHFocusListener(HFocusListener l) {
        logger.unimplemented("removeHFocusListener");
    }

    public int[] getNavigationKeys() {
        logger.unimplemented("getNavigationKeys");
        return null;
    }

    public void processHFocusEvent(HFocusEvent evt) {
        logger.unimplemented("processHFocusEvent");
    }

    public static void setDefaultLook(HTextLook hlook) {
        BDJXletContext.setXletDefaultLook(PROPERTY_LOOK, hlook);
    }

    public static HTextLook getDefaultLook() {
        return (HTextLook) BDJXletContext.getXletDefaultLook(PROPERTY_LOOK, DEFAULT_LOOK);
    }

    static final Class DEFAULT_LOOK = HTextLook.class;
    private static final String PROPERTY_LOOK = HText.class.getName();

    private static final long serialVersionUID = -8178609258303529066L;

    private static final Logger logger = Logger.getLogger(HText.class.getName());
}
