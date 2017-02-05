/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

import org.videolan.BDJUtil;
import org.videolan.FontIndex;
import org.videolan.FontIndexData;
import org.videolan.Logger;

public class FontFactory {
    public static void loadDiscFonts() {
        unloadDiscFonts();

        // fonts are loaded on demand
    }

    public static void unloadDiscFonts() {
        synchronized (fontsLock) {
            fonts = null;
            fontIds = null;
        }
    }

    private static void readDiscFonts() throws FontFormatException, IOException {
        synchronized (fontsLock) {

        if (fonts != null)
            return;

        String path = BDJUtil.discRootToFilesystem("/BDMV/AUXDATA/dvb.fontindex");
        FontIndexData fontIndexData[] = FontIndex.parseIndex(path);

        fonts = new HashMap(fontIndexData.length);
        fontIds = new HashMap(fontIndexData.length);
        for (int i = 0; i < fontIndexData.length; i++) {
            FontIndexData data = fontIndexData[i];
            try {
                File fontFile = org.videolan.BDJLoader.addFont(data.getFileName());
                if (fontFile == null) {
                    logger.error("error caching font");
                    throw new IOException("error caching font");
                }

                if (data.getStyle() == -1) {
                    logger.unimplemented("readDiscFonts(): font with all styles not supported");
                }

                Font font = Font.createFont(Font.TRUETYPE_FONT, fontFile);
                font = font.deriveFont(data.getStyle(), 1);

                fonts.put(data.getName() + "." + font.getStyle(), font);
                fontIds.put(data.getFileName().substring(0, 5), font);

            } catch (IOException ex) {
                logger.error("Failed reading font " + data.getName() + " from " + data.getFileName() + ": " + ex);
                if (i == fontIndexData.length - 1 && fonts.size() < 1) {
                    logger.error("didn't load any fonts !");
                    throw ex;
                }
            } catch (java.awt.FontFormatException ex) {
                logger.error("Failed reading font " + data.getName() + " from " + data.getFileName() + ": " + ex);
                if (i == fontIndexData.length - 1 && fonts.size() < 1) {
                    logger.error("didn't load any fonts !");
                    throw new FontFormatException();
                }
            }
        }
        }
    }

    public FontFactory() throws FontFormatException, IOException {
        readDiscFonts();
    }

    public FontFactory(URL u) throws IOException, FontFormatException {
        InputStream inStream = null;

        try {
            inStream = u.openStream();

            File fontFile = org.videolan.BDJLoader.addFont(inStream);
            if (fontFile == null) {
                logger.error("error caching font");
                throw new IOException("error caching font");
            }

            urlFont = Font.createFont(Font.TRUETYPE_FONT, fontFile);
            urlFont = urlFont.deriveFont(urlFont.getStyle(), 1);

        } catch (IOException ex) {
            logger.error("Failed reading font from " + u.getPath() + ": " + ex);
            throw ex;
        } catch (java.awt.FontFormatException ex) {
            logger.error("Failed reading font from " + u.getPath() + ": " + ex);
            throw new FontFormatException();
        } finally {
            if (inStream != null) {
                inStream.close();
            }
        }
    }

    // used by root window / .bdjo default font
    public Font createFont(String fontId) {
        Font font = null;
        synchronized (fontsLock) {
            if (fontIds == null) {
                logger.error("no disc fonts loaded");
            } else {
                font = (Font)fontIds.get(fontId);
            }
        }
        if (font != null) {
            return font.deriveFont(0, 12);
        }
        return null;
    }

    public Font createFont(String name, int style, int size)
            throws FontNotAvailableException, FontFormatException, IOException {
        logger.info("Creating font: " + name + " " + style + " " + size);

        if (style < 0 || size <= 0 || (style & ~3) != 0) {
            logger.error("invalid font size / style");
            throw new IllegalArgumentException();
        }

        /* Factory created only for single font ? */
        if (urlFont != null) {
            if (name.equals(urlFont.getName()) && style == urlFont.getStyle()) {
                return urlFont.deriveFont(style, size);
            }
            logger.info("createFont(URL): request " + name + "." + style + " does not match with " + urlFont.getName() + "." + urlFont.getStyle());
            throw new FontNotAvailableException();
        }

        /* Factory created for fonts in dvb.fontindex */
        Font font = null;
        synchronized (fontsLock) {
            if (fonts != null)
                font = (Font)fonts.get(name + "." + style);
        }

        if (font == null) {
            logger.info("Font " + name + "." + style + " not found");
            throw new FontNotAvailableException();
        }

        return font.deriveFont(style, size);
    }

    private Font urlFont = null;

    private static final Object fontsLock = new Object();

    private static Map fonts = null;
    private static Map fontIds = null;

    private static final Logger logger = Logger.getLogger(FontFactory.class.getName());
}
