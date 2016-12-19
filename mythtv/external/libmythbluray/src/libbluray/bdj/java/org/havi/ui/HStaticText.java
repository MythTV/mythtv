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

import org.videolan.BDJXletContext;
import org.videolan.Logger;

public class HStaticText extends HVisible implements HNoInputPreferred {
    public HStaticText() {
        this(null, 0, 0, 0, 0);
    }

    public HStaticText(String textNormal, int x, int y, int width, int height) {
        super(getDefaultLook(), x, y, width, height);
        setTextContent(textNormal, ALL_STATES);
        logger.info("HStaticText " + textNormal + " at " + x + "," + y + " " + width + "x" + height);
    }

    public HStaticText(String textNormal, int x, int y, int width, int height,
            Font font, Color foreground, Color background,
            HTextLayoutManager tlm) {
        this(textNormal, x, y, width, height);
        setFont(font);
        setForeground(foreground);
        setBackground(background);
        setTextLayoutManager(tlm);
    }

    public HStaticText(String textNormal) {
        this(textNormal, 0, 0, 0, 0);
        setTextContent(textNormal, NORMAL_STATE);
    }

    public HStaticText(String textNormal, Font font, Color foreground,
            Color background, HTextLayoutManager tlm) {
        this(textNormal, 0, 0, 0, 0);
        setFont(font);
        setForeground(foreground);
        setBackground(background);
        setTextLayoutManager(tlm);
    }

    public void setLook(HLook hlook) throws HInvalidLookException {
        if ((hlook != null) && !(hlook instanceof HTextLook))
            throw new HInvalidLookException();
        super.setLook(hlook);
    }

    public static void setDefaultLook(HTextLook hlook) {
        BDJXletContext.setXletDefaultLook(PROPERTY_LOOK,hlook);
    }

    public static HTextLook getDefaultLook() {
        return (HTextLook) BDJXletContext.getXletDefaultLook(PROPERTY_LOOK,DEFAULT_LOOK);
    }

    static final Class DEFAULT_LOOK = HTextLook.class;
    private static final String PROPERTY_LOOK = HStaticText.class.getName();
    private static final Logger logger = Logger.getLogger(HStaticText.class.getName());
    private static final long serialVersionUID = 4352450387189482885L;
}
