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

package java.awt.image;

import sun.awt.image.BufferedImagePeer;
import java.awt.Graphics2D;

public class BufferedImage extends java.awt.Image
{
    private transient BufferedImagePeer peer;

    public static final int TYPE_INT_ARGB = 2;

    private BufferedImage (BufferedImagePeer peer) {
        this.peer = peer;
    }

    public Graphics2D createGraphics() {
        return (Graphics2D) peer.getGraphics();
    }

    public void flush() {
        peer.flush();
    }

    public ColorModel getColorModel() {
        return peer.getColorModel();
    }

    public java.awt.Graphics getGraphics() {
        return peer.getGraphics();
    }

    public int getHeight() {
        return peer.getHeight();
    }

    public int getHeight(ImageObserver observer) {
        return peer.getHeight(observer);
    }

    public Object getProperty(String name) {
        return peer.getProperty(name);
    }

    public Object getProperty(String name, ImageObserver observer) {
        return peer.getProperty(name, observer);
    }

    public String[] getPropertyNames() {
        return peer.getPropertyNames();
    }

    public int getRGB(int x, int y) {
        return peer.getRGB(x, y);
    }

    public int[] getRGB(int startX, int startY, int w, int h, int[] rgbArray, int offset, int scansize) {
        return peer.getRGB(startX, startY, w, h, rgbArray, offset, scansize);
    }

    public ImageProducer getSource() {
        return peer.getSource();
    }

    public BufferedImage getSubimage(int x, int y, int w, int h) {
        return peer.getSubimage(x, y, w, h);
    }

    public int getType() {
        return peer.getType();
    }

    public int getWidth() {
        return peer.getWidth();
    }

    public int getWidth(ImageObserver observer) {
        return peer.getWidth(observer);
    }

    public void setRGB(int x, int y, int rgb) {
        peer.setRGB(x, y, rgb);
    }

    public void setRGB(int startX, int startY, int w, int h, int[] rgbArray, int offset, int scansize) {
        peer.setRGB(startX, startY, w, h, rgbArray, offset, scansize);
    }

    public String toString() {
        return peer.toString();
    }
}
