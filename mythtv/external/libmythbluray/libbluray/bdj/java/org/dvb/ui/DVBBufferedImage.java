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

import java.awt.Graphics;
import java.awt.GraphicsConfiguration;
import java.awt.GraphicsDevice;
import java.awt.GraphicsEnvironment;
import java.awt.Image;
import java.awt.image.BufferedImage;
import java.awt.image.ImageObserver;
import java.awt.image.ImageProducer;

public class DVBBufferedImage extends Image {
    public DVBBufferedImage(int width, int height)
    {
        this(width, height, TYPE_BASE);
    }

    public DVBBufferedImage(int width, int height, int type)
    {
        this.type = type;
        GraphicsEnvironment ge = GraphicsEnvironment.getLocalGraphicsEnvironment();
        GraphicsDevice gd = ge.getDefaultScreenDevice();
        GraphicsConfiguration gc = gd.getDefaultConfiguration();
        bufferedImage = gc.createCompatibleImage(width, height);
    }

    private DVBBufferedImage(BufferedImage image, int type) {
        this.type = type;
        this.bufferedImage = image;
    }

    public DVBGraphics createGraphics() {
        DVBGraphics gfx = new DVBGraphicsImpl(bufferedImage.createGraphics());
        gfx.type = type;
        return gfx;
    }

    public void dispose() {
        bufferedImage = null;
    }

    public void flush() {
        bufferedImage.flush();
    }

    public Graphics getGraphics() {
        return bufferedImage.getGraphics();
    }

    public int getHeight() {
        return bufferedImage.getHeight();
    }

    public int getHeight(ImageObserver observer) {
        return bufferedImage.getHeight(observer);
    }

    public Image getImage() {
        return bufferedImage;
    }

    public Object getProperty(String name, ImageObserver observer) {
        return bufferedImage.getProperty(name, observer);
    }

    public int getRGB(int x, int y) {
        return bufferedImage.getRGB(x, y);
    }

    public int[] getRGB(int startX, int startY, int w, int h, int[] rgbArray, int offset, int scansize) {
        return bufferedImage.getRGB(startX, startY, w, h, rgbArray, offset, scansize);
    }

    public Image getScaledInstance(int width, int height, int hints) {
        return bufferedImage.getScaledInstance(width, height, hints);
    }

    public ImageProducer getSource() {
        return bufferedImage.getSource();
    }

    public DVBBufferedImage getSubimage(int x, int y, int w, int h) throws DVBRasterFormatException {
        BufferedImage subImage = bufferedImage.getSubimage(x, y, w, h);
        return new DVBBufferedImage(subImage, this.type);
    }

    public int getWidth() {
        return bufferedImage.getWidth();
    }

    public int getWidth(ImageObserver observer) {
        return bufferedImage.getWidth(observer);
    }

    public void setRGB(int x, int y, int rgb) {
        this.bufferedImage.setRGB(x, y, rgb);
    }

    public void setRGB(int startX, int startY, int w, int h, int[] rgbArray, int offset, int scansize) {
        bufferedImage.setRGB(startX, startY, w, h, rgbArray, offset, scansize);
    }

    public String toString()
    {
        return new String("DVBBufferedImage");
    }

    public static final int TYPE_ADVANCED = 20;
    public static final int TYPE_BASE = 21;

    private int type = TYPE_BASE;
    private BufferedImage bufferedImage;
}
