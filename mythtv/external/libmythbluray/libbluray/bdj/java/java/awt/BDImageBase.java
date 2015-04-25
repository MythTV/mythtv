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

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Vector;
import java.util.Enumeration;
import java.awt.color.ColorSpace;
import java.awt.image.AreaAveragingScaleFilter;
import java.awt.image.DataBuffer;
import java.awt.image.DirectColorModel;
import java.awt.image.ImageConsumer;
import java.awt.image.ImageProducer;
import java.awt.image.ImageObserver;
import java.awt.image.ColorModel;
import java.awt.image.BufferedImage;

class BDImageBase extends Image {
    private static Constructor bufferedImageConstructor;

    private Component component;
    protected int width, height;
    protected int[] backBuffer;
    protected Area dirty;
    private GraphicsConfiguration gc;
    private Vector observers = new Vector();
    private ImageProducer offscreenSource = null;

    static {
        try {
            final Class c = Class.forName("java.awt.image.BufferedImage");
            AccessController.doPrivileged(
                new PrivilegedAction() {
                    public Object run() {
                        bufferedImageConstructor = c.getDeclaredConstructors()[0];
                        bufferedImageConstructor.setAccessible(true);
                        return null;
                    }
                }
           );
        } catch (ClassNotFoundException e) {
            throw new AWTError("java.awt.image.BufferedImage not found");
        }
    }

    private static BufferedImage createBuffededImage(Image image) {
        try {
            return (BufferedImage)bufferedImageConstructor.newInstance(new Object[] { image });
        } catch (IllegalArgumentException e) {
            e.printStackTrace();
        } catch (InstantiationException e) {
            e.printStackTrace();
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
        return null;
    }

    BDImageBase(Component component, int width, int height, GraphicsConfiguration gc) {

        this.component = component;
        this.width = width;
        this.height = height;
        this.gc = gc;
        if (width > 0 && height > 0)
            backBuffer = new int[width * height];

        dirty = new Area(width, height);

        offscreenSource = new BDOffScreenImageSource(backBuffer, width, height);
    }

    public void flush() {
    }

    public Graphics getGraphics() {
        return new BDGraphics((BDImage)this);
    }

    public int getWidth() {
        return width;
    }

    public int getWidth(ImageObserver observer) {
        if (width < 0)
            addObserver(observer);
        return width;
    }

    public int getHeight() {
        return height;
    }

    public int getHeight(ImageObserver observer) {
        if (height < 0)
            addObserver(observer);
        return height;
    }

    public Object getProperty(String name) {
        return null;
    }

    public Object getProperty(String name, ImageObserver observer) {
        return null;
    }

    public String[] getPropertyNames() {
        return null;
    }

    public Image getScaledInstance(int width, int height, int hints) {
        BDImageConsumer scaledImage = new BDImageConsumer(null);
        AreaAveragingScaleFilter scaleFilter =
            new AreaAveragingScaleFilter(width, height);
        scaleFilter = (AreaAveragingScaleFilter)scaleFilter.getFilterInstance(scaledImage);
        scaleFilter.setDimensions(this.width, this.height);
        scaleFilter.setPixels(0, 0, this.width, this.height,
                              ColorModel.getRGBdefault(), backBuffer, 0, this.width);
        scaleFilter.imageComplete(ImageConsumer.STATICIMAGEDONE);
        return scaledImage;
    }

    public ImageProducer getSource() {
        return offscreenSource;
    }

    public Component getComponent() {
        return component;
    }

    protected int[] getBdBackBuffer() {
        return backBuffer;
    }

    protected int[] getBackBuffer() {
        System.err.println("**** BDIMAGE GETBACKBUFFER ****");
        return backBuffer;
    }

    protected Area getDirtyArea() {
        return dirty;
    }

    public GraphicsConfiguration getGraphicsConfiguration() {
        return gc;
    }

    protected void addObserver(ImageObserver observer) {
        if (observer != null && !isObserver(observer))
            observers.addElement(observer);
    }

    protected boolean isObserver(ImageObserver observer) {
        return observers.contains(observer);
    }

    protected void removeObserver(ImageObserver observer) {
        observers.removeElement(observer);
    }

    protected void notifyObservers(Image img, int info, int x, int y, int w, int h) {
        Enumeration enumeration = observers.elements();
        Vector acquired = null;
        while (enumeration.hasMoreElements()) {
            ImageObserver observer = (ImageObserver)enumeration.nextElement();
            if (!observer.imageUpdate(img, info, x, y, w, h)) {
                if (acquired == null) {
                    acquired = new Vector();
                }
                acquired.addElement(observer);
            }
        }
        if (acquired != null) {
            enumeration = acquired.elements();
            while (enumeration.hasMoreElements()) {
                ImageObserver observer = (ImageObserver)enumeration.nextElement();
                removeObserver(observer);
            }
        }
    }

    public int getType() {
        return BufferedImage.TYPE_INT_ARGB;
    }

    public ColorModel getColorModel() {
        return new DirectColorModel(ColorSpace.getInstance(ColorSpace.CS_sRGB),
                32, 0x00ff0000, 0x0000ff00,     0x000000ff, 0xff000000, true,
                DataBuffer.TYPE_INT);
    }

    public int getRGB(int x, int y) {
        return backBuffer[y * width + x];
    }

    public int[] getRGB(int x, int y, int w, int h, int[] rgbArray, int offset, int scansize) {
        if (rgbArray == null)
            rgbArray = new int[offset + h * scansize];

        for (int i = 0; i < h; i++)
            System.arraycopy(backBuffer, (y + i) * width + x,
                             rgbArray, i * scansize + offset,
                             w);
        return rgbArray;
    }

    public void setRGB(int x, int y, int rgb) {
        backBuffer[y * width + x] = rgb;

        dirty.add(x, y);
    }

    public void setRGB(int x, int y, int w, int h, int[] rgbArray, int offset, int scansize) {
        for (int i = 0; i < h; i++)
            System.arraycopy(rgbArray, i * scansize + offset,
                             backBuffer, (y + i) * width + x,
                             w);
        dirty.add(new Rectangle(x, y, w, h));
    }

    public BufferedImage getSubimage(int x, int y, int w, int h) {
        BDImage image = new BDImage(component, w, h, gc);
        int[] rgbArray = image.getBdBackBuffer();
        getRGB(x, y, w, h, rgbArray, 0, w);
        return createBuffededImage(image);
    }

    public static BufferedImage getBuffededImage(int w, int h, GraphicsConfiguration gc) {
        BDImage image = new BDImage(null, w, h, gc);
        return createBuffededImage(image);
    }
}
