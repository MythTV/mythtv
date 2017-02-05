/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2016  Petri Hintukainen <phintuka@users.sourceforge.net>
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
import java.awt.image.RasterFormatException;

public class DVBBufferedImage extends Image {
    public DVBBufferedImage(int width, int height) {
        this(width, height, TYPE_BASE);
    }

    public DVBBufferedImage(int width, int height, int type) {
        if (type != TYPE_BASE && type != TYPE_ADVANCED) {
            throw new IllegalArgumentException(err("Unknown image type " + type));
        }
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException(err("Invalid size:" + width + "x" + height));
        }
        this.type = type;

        GraphicsEnvironment ge = GraphicsEnvironment.getLocalGraphicsEnvironment();
        GraphicsDevice gd = ge.getDefaultScreenDevice();
        GraphicsConfiguration gc = gd.getDefaultConfiguration();
        bufferedImage = gc.createCompatibleImage(width, height);

        if (bufferedImage == null) {
            err("Error creating buffered image");
        }
    }

    private DVBBufferedImage(BufferedImage image, int type) {
        if (image == null) {
            throw new IllegalArgumentException(err("null image"));
        }
        this.type = type;
        this.bufferedImage = image;
    }

    public DVBGraphics createGraphics() {
        if (bufferedImage == null) {
            err("disposed");
            return null;
        }
        DVBGraphics gfx = new DVBGraphicsImpl(bufferedImage.createGraphics());
        gfx.type = type;
        return gfx;
    }

    public void dispose() {
        bufferedImage = null;
    }

    public void flush() {
        if (bufferedImage != null) {
            bufferedImage.flush();
        }
    }

    public Graphics getGraphics() {
        if (bufferedImage == null) {
            return null;
        }
        return bufferedImage.getGraphics();
    }

    public int getHeight() {
        if (bufferedImage == null) {
            return -1;
        }
        return bufferedImage.getHeight();
    }

    public int getHeight(ImageObserver observer) {
        if (bufferedImage == null) {
            return -1;
        }
        return bufferedImage.getHeight(observer);
    }

    public Image getImage() {
        return bufferedImage;
    }

    public Object getProperty(String name, ImageObserver observer) {
        if (bufferedImage == null) {
            return null;
        }
        return bufferedImage.getProperty(name, observer);
    }

    public int getRGB(int x, int y) {
        if (bufferedImage == null) {
             throw new ArrayIndexOutOfBoundsException(err("disposed"));
        }

        int width = bufferedImage.getWidth();
        int height = bufferedImage.getHeight();
        if (x < 0 || y < 0 || x >= width || y >= height) {
            throw new ArrayIndexOutOfBoundsException(err("getRGB out of bounds"));
        }

        return bufferedImage.getRGB(x, y);
    }

    public int[] getRGB(int startX, int startY, int w, int h, int[] rgbArray, int offset, int scansize) {
        if (bufferedImage == null) {
             throw new ArrayIndexOutOfBoundsException(err("disposed"));
        }

        int width = bufferedImage.getWidth();
        int height = bufferedImage.getHeight();
        if (startX < 0 || startY < 0 || startX + w >= width || startY + height >= height) {
            throw new ArrayIndexOutOfBoundsException(err("getRGB out of bounds"));
        }

        return bufferedImage.getRGB(startX, startY, w, h, rgbArray, offset, scansize);
    }

    public Image getScaledInstance(int width, int height, int hints) {
        if (bufferedImage == null) {
            return null;
        }
        return bufferedImage.getScaledInstance(width, height, hints);
    }

    public ImageProducer getSource() {
        if (bufferedImage == null) {
            return null;
        }
        return bufferedImage.getSource();
    }

    public DVBBufferedImage getSubimage(int x, int y, int w, int h) throws DVBRasterFormatException {
        if (bufferedImage == null) {
            return null;
        }
        try {
            BufferedImage subImage = bufferedImage.getSubimage(x, y, w, h);
            return new DVBBufferedImage(subImage, this.type);
        } catch (RasterFormatException e) {
            throw new DVBRasterFormatException(err(e.getMessage()));
        }
    }

    public int getWidth() {
        if (bufferedImage == null) {
            return -1;
        }
        return bufferedImage.getWidth();
    }

    public int getWidth(ImageObserver observer) {
        if (bufferedImage == null) {
            return -1;
        }
        return bufferedImage.getWidth(observer);
    }

    public void setRGB(int x, int y, int rgb) {
        if (bufferedImage == null) {
            throw new ArrayIndexOutOfBoundsException(err("disposed"));
        }
        bufferedImage.setRGB(x, y, rgb);
    }

    public void setRGB(int startX, int startY, int w, int h, int[] rgbArray, int offset, int scansize) {
        if (bufferedImage == null) {
            throw new ArrayIndexOutOfBoundsException(err("disposed"));
        }
        bufferedImage.setRGB(startX, startY, w, h, rgbArray, offset, scansize);
    }

    public String toString()
    {
        return getClass().getName() + "[img=" + bufferedImage + "]";
    }

    private String err(String msg) {
        System.err.println("DVBBufferedImage: " + msg);
        return msg;
    }

    public static final int TYPE_ADVANCED = 20;
    public static final int TYPE_BASE = 21;

    private int type = TYPE_BASE;
    private BufferedImage bufferedImage;
}
