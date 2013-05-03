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

package org.videolan;

import java.awt.Font;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.ArrayList;

import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;
import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

public class FontIndex extends DefaultHandler {
    public static FontIndexData[] parseIndex(String path) {
        return new FontIndex(path).getFontIndexData();
    }

    private FontIndex(String path) {
        try {
            FileInputStream stream = new FileInputStream(path);
            SAXParser parser = SAXParserFactory.newInstance().newSAXParser();
            parser.parse(stream, this);
        } catch (FileNotFoundException e) {

        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            fontData = null;
        }
    }

    public FontIndexData[] getFontIndexData() {
        return (FontIndexData[])fontDatas.toArray(new FontIndexData[fontDatas.size()]);
    }

    public void startElement (String uri, String localName, String qName, Attributes attributes)
        throws SAXException {
        if (qName.equalsIgnoreCase("fontdirectory")) {
            inDocument = true;
            return;
        }
        if (inDocument) {
            if (qName.equalsIgnoreCase("font"))
                fontData = new FontIndexData();
            else if (qName.equalsIgnoreCase("name"))
                element = ELEMENT_NAME;
            else if (qName.equalsIgnoreCase("fontformat"))
                element = ELEMENT_FORMAT;
            else if (qName.equalsIgnoreCase("filename"))
                element = ELEMENT_FILENAME;
            else if (qName.equalsIgnoreCase("style"))
                element = ELEMENT_STYLE;
            else if (qName.equalsIgnoreCase("size"))
                element = ELEMENT_SIZE;
            else
                throw new SAXException("element not supported");
            if (element == ELEMENT_SIZE) {
                for (int i = 0; i < attributes.getLength(); i++) {
                    String attrName = attributes.getQName(i);
                    if (attrName.equals("min"))
                        fontData.minSize = Integer.parseInt(attributes.getValue(i));
                    else if (attrName.equals("max"))
                        fontData.maxSize = Integer.parseInt(attributes.getValue(i));
                    else
                        throw new SAXException("invalid attribute name: " + attrName);
                }
            } else {
                if (attributes.getLength() != 0)
                    throw new SAXException("invalid attribute for state");
            }
        }
    }

    public void endElement (String uri, String localName, String qName)
        throws SAXException {
        if (qName.equalsIgnoreCase("fontdirectory")) {
            inDocument = false;
            return;
        }
        if (inDocument) {
            if (qName.equalsIgnoreCase("font")) {
                fontDatas.add(fontData);
                fontData = null;
            }
        }
        element = ELEMENT_NONE;
    }

    public void characters (char ch[], int start, int length) throws SAXException {
        switch (element) {
        case ELEMENT_NAME:
            fontData.name = new String(ch, start, length);
            break;
        case ELEMENT_FORMAT:
            fontData.format = new String(ch, start, length);
            break;
        case ELEMENT_FILENAME:
            fontData.filename = new String(ch, start, length);
            break;
        case ELEMENT_STYLE:
            String style = new String(ch, start, length);
            if (style.equals("PLAIN"))
                fontData.style = Font.PLAIN;
            else if (style.equals("BOLD"))
                fontData.style = Font.BOLD;
            else if (style.equals("ITALIC"))
                fontData.style = Font.ITALIC;
            else if (style.equals("BOLD_ITALIC"))
                fontData.style = Font.BOLD | Font.ITALIC;
            else
                throw new SAXException("invalid font style");
            break;
        }
    }

    private static final int ELEMENT_NONE = 0;
    private static final int ELEMENT_NAME = 1;
    private static final int ELEMENT_FORMAT = 2;
    private static final int ELEMENT_FILENAME = 3;
    private static final int ELEMENT_SIZE = 4;
    private static final int ELEMENT_STYLE = 5;

    private boolean inDocument = false;
    private int element = ELEMENT_NONE;
    private FontIndexData fontData = null;
    private ArrayList fontDatas = new ArrayList();
}
