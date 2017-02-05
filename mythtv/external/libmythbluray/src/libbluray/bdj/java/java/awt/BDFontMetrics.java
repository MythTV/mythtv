/*
 * This file is part of libbluray
 * Copyright (C) 2012  libbluray
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

package java.awt;

import java.io.File;
import java.lang.ref.WeakReference;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.videolan.Logger;

public class BDFontMetrics extends sun.font.FontDesignMetrics {
    static final long serialVersionUID = -4956160226949100590L;

    private static long ftLib = 0;
    private static long fcLib = 0;
    private static Map  systemFontNameMap = null;

    private static final Logger logger = Logger.getLogger(BDFontMetrics.class.getName());

    private static native long initN();
    private static native void destroyN(long ftLib);
    private static native String[] getFontFamilyAndStyleN(long ftLib, String fontName);

    protected synchronized static String[] getFontFamilyAndStyle(String fontFile) {
        return getFontFamilyAndStyleN(ftLib, fontFile);
    }

    private native static String resolveFontN(String fontFamily, int fontStyle);
    private native static void   unloadFontConfigN();

    private static void addSystemFont(String alias, int style, String family, String defaultPath) {

        alias = alias + "." + style;

        /* default to jre fonts, if available */
        if (new File(defaultPath).exists()) {
            logger.info("mapping " + alias + " (" + family + ") to " + defaultPath);
            systemFontNameMap.put(alias, defaultPath);
            return;
        }

        /* try fontconfig */
        String path = resolveFontN(family, style);
        if (path != null) {
            logger.info("fontconfig: mapping " + alias + " (" + family + ") to " + path);
            systemFontNameMap.put(alias, path);
            return;
        }

        logger.error("Can't resolve font " + alias + ": file " + defaultPath + " does not exist");
        /* useless ? font file does not exist ... */
        systemFontNameMap.put(alias, defaultPath);
    }

    private static void initSystemFonts() {
        String javaHome = (String) AccessController.doPrivileged(new PrivilegedAction() {
                public Object run() {
                    return System.getProperty("java.home");
                }
            }
        );
        File f = new File(javaHome, "lib" + File.separator + "fonts");
        String defaultFontPath = f.getAbsolutePath() + File.separator;

        final Object[][] sfd = {
            { "serif",       "Arial",           new String[] {"LucidaBrightRegular.ttf",     "LucidaBrightDemiBold.ttf", "LucidaBrightItalic.ttf",      "LucidaBrightDemiItalic.ttf"}},
            { "sansserif",   "Times New Roman", new String[] {"LucidaSansRegular.ttf",       "LucidaSansDemiBold.ttf",   "LucidaSansOblique.ttf",       "LucidaSansDemiOblique.ttf"}},
            { "monospaced",  "Courier New",     new String[] {"LucidaTypewriterRegular.ttf", "LucidaTypewriterBold.ttf", "LucidaTypewriterOblique.ttf", "LucidaTypewriterBoldOblique.ttf"}},
            { "dialog",      "Times New Roman", new String[] {"LucidaSansRegular.ttf",       "LucidaSansDemiBold.ttf",   "LucidaSansOblique.ttf",       "LucidaSansDemiOblique.ttf"}},
            { "dialoginput", "Courier New",     new String[] {"LucidaTypewriterRegular.ttf", "LucidaTypewriterBold.ttf", "LucidaTypewriterOblique.ttf", "LucidaTypewriterBoldOblique.ttf"}},
            { "default",     "Times New Roman", new String[] {"LucidaSansRegular.ttf",       "LucidaSansDemiBold.ttf",   "LucidaSansOblique.ttf",       "LucidaSansDemiOblique.ttf"}},
        };

        systemFontNameMap = new HashMap(24);

        for (int type = 0; type < sfd.length; type++) {
            for (int style = 0; style < 4; style++) {
                addSystemFont((String)sfd[type][0], style, (String)sfd[type][1],
                              defaultFontPath + ((String[])sfd[type][2])[style]);
            }
        }

        unloadFontConfigN();
    }

    public synchronized static void init() {

        if (ftLib == 0) {
            ftLib = initN();
        }
        if (ftLib == 0) {
            logger.error("freetype library not loaded");
            throw new AWTError("freetype lib not loaded");
        }

        if (systemFontNameMap == null) {
            initSystemFonts();
        }
    }

    public synchronized static void shutdown() {
        Iterator it = fontMetricsMap.values().iterator();
        while (it.hasNext()) {
            try {
                WeakReference ref = (WeakReference)it.next();
                BDFontMetrics fm = (BDFontMetrics)ref.get();
                it.remove();
                if (fm != null) {
                    fm.destroy();
                }
            } catch (Exception e) {
                logger.error("shutdown() failed: " + e);
            }
        }
        destroyN(BDFontMetrics.ftLib);
        ftLib = 0;
    }

    /** A map which maps a native font name and size to a font metrics object. This is used
     as a cache to prevent loading the same fonts multiple times. */
    private static Map fontMetricsMap = new HashMap();

    /** Gets the BDFontMetrics object for the supplied font. This method caches font metrics
     to ensure native fonts are not loaded twice for the same font. */
    static synchronized BDFontMetrics getFontMetrics(Font font) {
        /* See if metrics has been stored in font already. */
        BDFontMetrics fm = (BDFontMetrics)font.metrics;
        if (fm == null || fm.ftFace == 0) {
            /* See if a font metrics of the same native name and size has already been loaded.
             If it has then we use that one. */
            String nativeName;
            if (font.fontFile != null) {
                nativeName = font.fontFile.getPath();
            } else {
                nativeName = (String)systemFontNameMap.get(font.getName().toLowerCase() + "." + font.getStyle());
                if (nativeName == null) {
                    nativeName = (String)systemFontNameMap.get("default." + font.getStyle());
                }
            }

            String key = nativeName + "." + font.getSize();
            WeakReference ref = (WeakReference)fontMetricsMap.get(key);
            if (ref != null) {
                fm = (BDFontMetrics)ref.get();
            }

            if (fm == null) {
                fm = new BDFontMetrics(font, nativeName);
                fontMetricsMap.put(key, new WeakReference(fm));
            }
            font.metrics = fm;
        }
        return fm;
    }

    static {
        sun.font.FontDesignMetrics.setGetFontMetricsAccess(
            new sun.font.FontDesignMetrics.GetFontMetricsAccess() {
                public sun.font.FontDesignMetrics getFontMetrics(Font font) {
                    return BDFontMetrics.getFontMetrics(font);
                }
            });
    }

    static String stripAttributes(String fontname) {
        int dotidx;
        if ((dotidx = fontname.indexOf('.')) == -1)
            return fontname;
        return fontname.substring(0, dotidx);
    }

    static synchronized String[] getFontList() {
        try {
            init();
        } catch (ThreadDeath td) {
            throw td;
        } catch (Throwable t) {
            logger.error("getFontList() failed: " + t);
            return new String[0];
        }

        ArrayList fontNames = new ArrayList();

        Iterator fonts = systemFontNameMap.keySet().iterator();
        while (fonts.hasNext()) {
            String fontname = stripAttributes((String)fonts.next());
            if (!fontNames.contains(fontname))
                fontNames.add(fontname);
        }

        return (String[])fontNames.toArray(new String[fontNames.size()]);
    }

    private long ftFace = 0;
    private int ascent = 0;
    private int descent = 0;
    private int leading = 0;
    private int maxAdvance = 0;

    /** Cache of first 256 Unicode characters as these map to ASCII characters and are often used. */
    private int[] widths;

    /* synchronize access to ftFace (native functions) */
    private final Object faceLock = new Object();

    /**
     * Creates a font metrics for the supplied font. To get a font metrics for a font
     * use the static method getFontMetrics instead which does caching.
     */
    private BDFontMetrics(Font font, String nativeName) {
        super(font);

        ftFace = loadFontN(ftLib, nativeName, font.getSize());
        if (ftFace == 0) {
            logger.error("Error loading font");
            throw new AWTError("font face:" + nativeName + " not loaded");
        }
        widths = null;
    }

    private native long loadFontN(long ftLib, String fontName, int size);
    private native void destroyFontN(long ftFace);
    private native int charWidthN(long ftFace, char c);
    private native int stringWidthN(long ftFace, String string);
    private native int charsWidthN(long ftFace, char chars[], int offset, int len);

    private void loadWidths() {
        /* Cache first 256 char widths for use by the getWidths method and for faster metric
           calculation as they are commonly used (ASCII) characters. */
        if (widths == null) {
            int[] widths = new int[256];
            synchronized (faceLock) {
                for (int i = 0; i < 256; i++) {
                    widths[i] = charWidthN(ftFace, (char)i);
                }
            }
            this.widths = widths;
        }
    }

    protected void drawString(BDGraphics g, String string, int x, int y, int rgb) {
        synchronized (faceLock) {
            g.drawStringN(ftFace, string, x, y, rgb);
        }
    }

    public int getAscent() {
        return ascent;
    }

    public int getDescent() {
        return descent;
    }

    public int getLeading() {
        return leading;
    }

    public int getMaxAdvance() {
        return maxAdvance;
    }

    /**
     * Fast lookup of first 256 chars as these are always the same eg. ASCII charset.
     */
    public int charWidth(char c) {
        if (c < 256) {
            loadWidths();
            return widths[c];
        }
        synchronized (faceLock) {
            return charWidthN(ftFace, c);
        }
    }

    /**
     * Return the width of the specified string in this Font.
     */
    public int stringWidth(String string) {
        /* Allow only one call at time.
         * (calling this function from multiple threads caused crashes in freetype)
         */
        synchronized (BDFontMetrics.class) {
            synchronized (faceLock) {
                return stringWidthN(ftFace, string);
            }
        }
    }

    /**
     * Return the width of the specified char[] in this Font.
     */
    public int charsWidth(char chars[], int offset, int length) {
        synchronized (faceLock) {
            return charsWidthN(ftFace, chars, offset, length);
        }
    }

    /**
     * Get the widths of the first 256 characters in the font.
     */
    public int[] getWidths() {
        loadWidths();
        int[] newWidths = new int[widths.length];
        System.arraycopy(widths, 0, newWidths, 0, widths.length);
        return newWidths;
    }

    private void destroy() {
        if (ftFace != 0) {
            destroyFontN(ftFace);
            ftFace = 0;
        }
    }

    protected void finalize() throws Throwable {
        try {
            destroy();
        } catch (Throwable t) {
            throw t;
        } finally {
            super.finalize();
        }
    }
}
