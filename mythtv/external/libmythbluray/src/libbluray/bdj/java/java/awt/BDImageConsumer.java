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

import java.util.Hashtable;
import java.awt.image.ImageProducer;
import java.awt.image.ImageObserver;
import java.awt.image.ImageConsumer;
import java.awt.image.ColorModel;

class BDImageConsumer extends BDImage implements ImageConsumer {
    private Hashtable properties;
    private ImageProducer producer;
    private int status;
    private boolean started;

    public BDImageConsumer(ImageProducer producer) {
        super(null, -1, -1, null);
        this.producer = producer;
    }

    public int getWidth(ImageObserver observer) {
        if (width < 0) {
            addObserver(observer);
            startProduction();
        }
        return width;
    }

    public int getHeight(ImageObserver observer) {
        if (height < 0) {
            addObserver(observer);
            startProduction();
        }
        return height;
    }

    public boolean isComplete(ImageObserver observer) {
        if ((status & ImageObserver.ALLBITS) != 0)
            return true;
        if (observer != null)
            if (observer.imageUpdate(this, status, 0, 0, width, height))
                addObserver(observer);
        return false;
    }

    public int checkImage(ImageObserver observer) {
        if (observer != null)
            if (observer.imageUpdate(this, status, 0, 0, width, height))
                addObserver(observer);
        return status;
    }

    public boolean prepareImage(ImageObserver observer) {
        if (started) {
            if (observer != null)
                if (observer.imageUpdate(this, status, 0, 0, width, height))
                    addObserver(observer);
        } else {
            addObserver(observer);
            startProduction();
        }
        return ((status & ImageObserver.ALLBITS) != 0);
    }

    public ImageProducer getSource() {
        return producer;
    }

    public void flush() {
        width = -1;
        height = -1;
        backBuffer = null;
        status = 0;
        started = false;
        producer.removeConsumer(this);
        BDToolkit.clearCache(this);
    }

    public void imageComplete(int stat) {
        switch (stat) {
        case IMAGEERROR:
            status |= ImageObserver.ERROR;
            notifyObservers(this, ImageObserver.ERROR, 0, 0, width, height);
            break;
        case SINGLEFRAMEDONE:
            status |= ImageObserver.FRAMEBITS;
            notifyObservers(this, ImageObserver.FRAMEBITS, 0, 0, width, height);
            break;
        case STATICIMAGEDONE:
            status |= ImageObserver.ALLBITS;
            notifyObservers(this, ImageObserver.ALLBITS, 0, 0, width, height);
            break;
        case IMAGEABORTED:
            status |= ImageObserver.ABORT;
            notifyObservers(this, ImageObserver.ABORT, 0, 0, width, height);
            break;
        }
    }

    public void setColorModel(ColorModel cm) {

    }

    public void setDimensions(int width, int height) {
        if ((width >= 0) && (height >= 0)) {
            this.width = width;
            this.height = height;
            backBuffer = new int[width * height];
            status |= ImageObserver.WIDTH | ImageObserver.HEIGHT;
            notifyObservers(this, ImageObserver.WIDTH | ImageObserver.HEIGHT, 0, 0, width, height);
        }
    }

    public Object getProperty(String name) {
        if (properties == null)
            return null;
        return properties.get(name);
    }

    public Object getProperty(String name, ImageObserver observer) {
        if (properties == null) {
            addObserver(observer);
            return null;
        }
        return properties.get(name);
    }

    public void setProperties(Hashtable props) {
        properties = props;
        status |= ImageObserver.PROPERTIES;
        notifyObservers(this, ImageObserver.PROPERTIES, 0, 0, width, height);
    }

    public void setHints(int hints) {

    }

    public void setPixels(int x, int y, int w, int h, ColorModel cm, byte[] pixels, int offset, int scansize) {
        int X, Y;
        for (Y = y; Y < (y + h); Y++)
            for (X = x; X < (x + w); X++)
                backBuffer[Y * width + X] = cm.getRGB(pixels[offset + (Y - y) * scansize + (X - x)] & 0xFF);
        dirty.add(new Rectangle(x, y, w, h));
        status |= ImageObserver.SOMEBITS;
        notifyObservers(this, ImageObserver.SOMEBITS, x, y, w, h);
    }

    public void setPixels(int x, int y, int w, int h, ColorModel cm, int[] pixels, int offset, int scansize) {
        int X, Y;
        for (Y = y; Y < (y + h); Y++)
            for (X = x; X < (x + w); X++)
                backBuffer[Y * width + X] = cm.getRGB(pixels[offset + (Y - y) * scansize + (X - x)]);
        dirty.add(new Rectangle(x, y, w, h));
        status |= ImageObserver.SOMEBITS;
        notifyObservers(this, ImageObserver.SOMEBITS, x, y, w, h);
    }

    protected synchronized void startProduction() {
        if (producer != null && !started) {
            if (!producer.isConsumer(this))
                producer.addConsumer(this);
            started = true;
            producer.startProduction(this);
        }
    }

}
