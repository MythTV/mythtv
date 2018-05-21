/*
 * This file is part of libbluray
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

package org.videolan;

import java.io.FileInputStream;
import java.util.LinkedList;

import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;
import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

public class BUMFParser extends DefaultHandler {

    public static BUMFAsset[] parse(String manifestFile) {
        try {
            return new BUMFParser(manifestFile).getAssets();
        } catch (Exception e) {
        }
        return null;
    }

    /*
     *
     */

    private BUMFAsset[] getAssets() {
        return (BUMFAsset[])assets.toArray(new BUMFAsset[assets.size()]);
    }

    private BUMFParser(String manifestFile) throws Exception {

        FileInputStream stream = null;
        try {
            stream = new FileInputStream(manifestFile);
            SAXParser parser = SAXParserFactory.newInstance().newSAXParser();
            parser.parse(stream, this);
        } catch (Exception e) {
            logger.error("Binding unit manifest file parsing failed: " + e);
            throw e;
        } finally {
            if (stream != null) {
                try {
                    stream.close();
                } catch (Exception e) {
                }
            }
        }
    }

    private LinkedList assets = new LinkedList();

    /*
     *
     */

    public void startElement (String uri, String localName, String qName, Attributes attributes)
        throws SAXException {

        if (qName.equalsIgnoreCase("bumf:manifest")) {
            inBudaFile = true;
            return;
        }

        if (!inBudaFile) {
            logger.error("invalid start element: " + qName);
            throw new SAXException("element not supported");
        }

        if (qName.equalsIgnoreCase("Assets")) {
            inDocument = true;
            return;
        }

        if (!inDocument) {
            logger.error("unknown element: " + qName + " (expected Assets)");
            throw new SAXException("element not supported");
        }

        if (qName.equalsIgnoreCase("Asset")) {
            vpFile = null;
            budaFile = null;
            element = ELEMENT_ASSET;
        } else if (qName.equalsIgnoreCase("BUDAFile")) {
            element = ELEMENT_BUDA_FILE;
        } else {
            logger.error("unknown element: " + qName);
            throw new SAXException("element not supported");
        }

        if (element == ELEMENT_ASSET) {
            for (int i = 0; i < attributes.getLength(); i++) {
                String attrName = attributes.getQName(i);
                if (attrName.equals("VPFilename")) {
                    vpFile = attributes.getValue(i);
                } else {
                    logger.error("unknown VPFilename attribute: " + attrName);
                    throw new SAXException("invalid attribute name: " + attrName);
                }
            }
        } else if (element == ELEMENT_BUDA_FILE) {
            for (int i = 0; i < attributes.getLength(); i++) {
                String attrName = attributes.getQName(i);
                if (attrName.equals("name")) {
                    budaFile = attributes.getValue(i);
                } else {
                    logger.error("unknown BUDAFile attribute: " + attrName);
                    throw new SAXException("invalid attribute name: " + attrName);
                }
            }
        }
    }

    public void endElement (String uri, String localName, String qName)
        throws SAXException {

        if (qName.equalsIgnoreCase("Assets")) {
            inDocument = false;
            return;
        }

        if (inDocument) {
            if (qName.equalsIgnoreCase("Asset")) {
                logger.info("Asset: " + vpFile + " <- " + budaFile);
                if (vpFile != null && budaFile != null) {
                    assets.add(new BUMFAsset(vpFile, budaFile));
                }
                vpFile = null;
                budaFile = null;
            }
        }
        element = ELEMENT_NONE;
    }

    private static final int ELEMENT_NONE = 0;
    private static final int ELEMENT_ASSET = 1;
    private static final int ELEMENT_BUDA_FILE = 2;

    private boolean inBudaFile = false;
    private boolean inDocument = false;
    private int element = ELEMENT_NONE;

    private String vpFile = null;
    private String budaFile = null;

    private static final Logger logger = Logger.getLogger(BUMFParser.class.getName());
}




