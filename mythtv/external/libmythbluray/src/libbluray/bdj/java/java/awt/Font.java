/*
 * This file is part of libbluray
 * Copyright (C) 2014  libbluray
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
import java.util.Hashtable;
import java.util.Map;
import java.awt.font.TextAttribute;
import java.text.AttributedCharacterIterator.Attribute;

public class Font implements java.io.Serializable {

    /*
     * Java ME 1.4 compatible Font class
     *
     * https://docs.oracle.com/javame/config/cdc/ref-impl/pbp1.1.2/jsr217/
     */

    public static final int PLAIN  = 0;
    public static final int BOLD   = 1;
    public static final int ITALIC = 2;

    protected String name;
    protected int    style;
    protected int    size;

    public Font(String name, int style, int size) {
        this(name, style, size, null, null);
    }

    public Font(Map attributes) {
        this.name  = "Default";
        this.style = PLAIN;
        this.size  = 12;

        org.videolan.Logger.unimplemented("Font", "Font(Map)");

        setFamily();
    }

    public static Font decode(String str) {

        if (str == null) {
            return new Font("Dialog", PLAIN, 12);
        }

        org.videolan.Logger.unimplemented("Font", "decode");

        return new Font("Dialog", PLAIN, 12);
    }

    public boolean equals(Object obj) {
        if (obj == this) {
            return true;
        }
        if (obj == null) {
            return false;
        }
        if (!(obj instanceof Font)) {
            return false;
        }
        Font font = (Font)obj;
        if (size != font.size || style != font.style || !name.equals(font.name)) {
            return false;
        }
        if (fontFile != null && font.fontFile != null &&
            !fontFile.equals(font.fontFile)) {
            return false;
        }
        return true;
    }

    public Attribute[] getAvailableAttributes() {
        Attribute attributes[] = {
            TextAttribute.FAMILY,
            TextAttribute.WEIGHT,
            TextAttribute.POSTURE,
            TextAttribute.SIZE,
        };
        return attributes;
    }

    public Map getAttributes() {
        Hashtable map = new Hashtable();
        map.put(TextAttribute.FAMILY,  name);
        map.put(TextAttribute.SIZE,    new Float(size));
        map.put(TextAttribute.WEIGHT,  (style & BOLD)   != 0 ? TextAttribute.WEIGHT_BOLD     : TextAttribute.WEIGHT_REGULAR);
        map.put(TextAttribute.POSTURE, (style & ITALIC) != 0 ? TextAttribute.POSTURE_OBLIQUE : TextAttribute.POSTURE_REGULAR);
        return (Map)map;
    }

    public String getFamily() {
        return family;
    }

    public static Font getFont(Map attributes) {
        Font font = (Font)attributes.get(TextAttribute.FONT);
        if (font != null) {
            return font;
        }
        return new Font(attributes);
    }

    public static Font getFont(String nm) {
        return getFont(nm, null);
    }

    public static Font getFont(String nm, Font font) {
        String str = System.getProperty(nm);
        if (str == null) {
            return font;
        }
        return decode(str);
    }

    public String getName() {
        return name;
    }

    public int getSize() {
        return size;
    }

    public int getStyle() {
        return style;
    }

    public int hashCode() {
        return name.hashCode() ^ style ^ size;
    }

    public boolean isBold() {
        return (style & BOLD) != 0;
    }

    public boolean isItalic() {
        return (style & ITALIC) != 0;
    }

    public boolean isPlain() {
        return style == 0;
    }

    public String toString() {
        String strStyle[] = { "plain", "bold", "italic", "bolditalic" };
        return getClass().getName() + "[family=" + getFamily() + ",name=" + name + ",style=" + strStyle[style] + ",size=" + size + "]";
    }

    /*
     * libbluray implementation-specific extensions
     */

    transient FontMetrics metrics = null;
    private transient String family = null;
    protected transient File fontFile = null;

    public static final int TRUETYPE_FONT = 0;

    /* used by org.dvb.ui.FontFacrtory */
    public static Font createFont(int type, File fontFile) throws FontFormatException {
        if (type != TRUETYPE_FONT) {
            throw new FontFormatException("unsupported font format");
        }

        if (fontFile == null) {
            throw new NullPointerException("fontFile is null");
        }

        String data[] = BDFontMetrics.getFontFamilyAndStyle(fontFile.getPath());
        if (data == null || data.length < 2) {
            throw new FontFormatException("error loading font " + fontFile.getPath());
        }

        String family = data[0];
        int    style  = parseStyle(data[1]);

        return new Font(family, style, 1, fontFile, family);
    }

    /* used by org.dvb.ui.FontFacrtory */
    public Font deriveFont(int style, int size) {
        return new Font(name, style, size, fontFile, family);
    }
    public Font deriveFont(int style, float size) {
        return new Font(name, style, (int)size, fontFile, family);
    }

    /* constructor */
    private Font(String name, int style, int size, File fontFile, String family) {
        this.name     = (name != null) ? name : "Default";
        this.style    = (style & ~0x03) == 0 ? style : 0;
        this.size     = size;
        this.fontFile = fontFile;
        this.family   = family;
        if (family == null) {
            setFamily();
        }
    }

    /*
     * private
     */

    private static final long serialVersionUID = -4206021311591459213L; /* JDK 1.1 serialVersionUID */

    private void writeObject(java.io.ObjectOutputStream s)
        throws java.lang.ClassNotFoundException, java.io.IOException {
        s.defaultWriteObject();
    }

    private void readObject(java.io.ObjectInputStream s)
        throws java.lang.ClassNotFoundException, java.io.IOException {
        s.defaultReadObject();
        setFamily();
    }

    private static int parseStyle(String styleName) {
        int style = PLAIN;

        if (styleName != null && styleName.length() > 0) {
            String[] styles = org.videolan.StrUtil.split(styleName, ' ');
            if (styles.length == 1) {
                styles = org.videolan.StrUtil.split(styles[0], ',');
            }

            for (int i = 0; i < styles.length; i++) {
                styleName = styles[i].toLowerCase();
                if (styleName.equals("bolditalic")) {
                    style |= BOLD | ITALIC;
                } else if (styleName.equals("italic")) {
                    style |= ITALIC;
                } else if (styleName.equals("bold")) {
                    style |= BOLD;
                } else if (styleName.equals("plain")) {
                } else if (styleName.equals("serif")) {
                } else if (styleName.equals("regular")) {
                } else if (styleName.equals("roman")) {
                } else {
                    org.videolan.Logger.getLogger("Font").info("unregonized style: " + styleName);
                }
            }
        }

        return style;
    }

    private void setFamily() {
        String[] names = GraphicsEnvironment.getLocalGraphicsEnvironment().getAvailableFontFamilyNames();
        if (names.length == 0) {
            family = "Default";
            return;
        }
        for (int i = 0; i < names.length; i++) {
            if (names[i].equalsIgnoreCase(name)) {
                family = names[i];
                return;
            }
            if (names[i].equalsIgnoreCase("Dialog")) {
                family = names[i];
            }
        }
        if (family == null) {
            family = names[0];
        }
    }
}
