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

package org.dvb.ui;

import java.awt.Font;
import java.awt.FontFormatException;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.URL;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.logging.Logger;

import org.videolan.BDJException;
import org.videolan.BDJUtil;
import org.videolan.FontIndex;
import org.videolan.FontIndexData;

public class FontFactory {
    public static void loadDiscFonts() {
        unloadDiscFonts();
    }

    public static void unloadDiscFonts() {
    }

    public FontFactory() throws FontFormatException, IOException {
        String path = BDJUtil.discRootToFilesystem("/BDMV/AUXDATA/dvb.fontindex");

        FontIndexData fontIndexData[] = FontIndex.parseIndex(path);

        fonts = new HashMap<String, Font>(fontIndexData.length);
        for (FontIndexData data : fontIndexData) {
            FileInputStream inStream = new FileInputStream(BDJUtil.discRootToFilesystem("/BDMV/AUXDATA/" + data.getFileName()));

            Font font = Font.createFont(Font.TRUETYPE_FONT, inStream);
            font = font.deriveFont(data.getStyle(), data.getMaxSize());

            fonts.put(data.getName(), font);
        }
    }

    public FontFactory(URL u) throws IOException, FontFormatException {
        FileInputStream inStream = new FileInputStream(u.getPath());

        urlFont = Font.createFont(Font.TRUETYPE_FONT, inStream);
    }

    public Font createFont(String name, int style, int size)
            throws FontNotAvailableException, FontFormatException, IOException {
        logger.info("Creating font: " + name + " " + style + " " + size);

        if (urlFont != null && name.equals(urlFont.getName()))
        {
            return urlFont.deriveFont(style, size);
        }

        Font font = fonts.get(name);

        if (font == null)
            throw new FontNotAvailableException();

        return font.deriveFont(style, size);
    }

    private Font urlFont = null;
    private Map<String, Font> fonts = null;
    
    private static final Logger logger = Logger.getLogger(FontFactory.class.getName());
}
