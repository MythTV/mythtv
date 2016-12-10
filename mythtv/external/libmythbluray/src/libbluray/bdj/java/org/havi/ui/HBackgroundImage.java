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

package org.havi.ui;

import java.awt.Image;
import java.awt.Toolkit;
import java.awt.image.ImageObserver;
import java.awt.image.ImageProducer;
import java.net.URL;
import java.util.ArrayList;

import org.havi.ui.event.HBackgroundImageEvent;
import org.havi.ui.event.HBackgroundImageListener;

import sun.awt.image.ByteArrayImageSource;
import sun.awt.image.FileImageSource;
import sun.awt.image.URLImageSource;

public class HBackgroundImage implements ImageObserver {
    public HBackgroundImage(String filename) {
        producer = new FileImageSource(filename);
        img = Toolkit.getDefaultToolkit().createImage(producer);
    }

    public HBackgroundImage(byte pixels[]) {
        if (pixels == null) {
            System.err.println("null pixels from " + org.videolan.Logger.dumpStack());
            return;
        }
        producer = new ByteArrayImageSource(pixels);
        img = Toolkit.getDefaultToolkit().createImage(producer);
    }

    public HBackgroundImage(URL contents) {
        producer = new URLImageSource(contents);
        img = Toolkit.getDefaultToolkit().createImage(producer);
    }

   public void load(HBackgroundImageListener listener) {
        synchronized(listeners) {
            listeners.add(listener);
        }
        Toolkit.getDefaultToolkit().prepareImage(img, -1, -1, this);
    }

    public int getHeight() {
        return img.getHeight(null);
    }

    public int getWidth() {
        return img.getWidth(null);
    }

    public void flush() {
        img.flush();
    }

    public boolean imageUpdate(Image img, int infoflags, int x, int y,
                               int width, int height) {
        switch(infoflags) {
        case ImageObserver.ALLBITS:
        case ImageObserver.FRAMEBITS:
            notifyListeners(new HBackgroundImageEvent(this,
                                                      HBackgroundImageEvent.BACKGROUNDIMAGE_LOADED));
            return false;
        case ImageObserver.ERROR:
            notifyListeners(new HBackgroundImageEvent(this,
                                                      HBackgroundImageEvent.BACKGROUNDIMAGE_INVALID));
            return false;
        case ImageObserver.ABORT:
            notifyListeners(new HBackgroundImageEvent(this,
                                                      HBackgroundImageEvent.BACKGROUNDIMAGE_IOERROR));
            return false;
        }
        return true;
    }

    protected Image getImage() {
        return img;
    }

    protected void notifyListeners(HBackgroundImageEvent event) {
        ArrayList list;
        synchronized(listeners) {
            list = (ArrayList)listeners.clone();
            listeners.clear();
        }
        for (int i = 0; i < list.size(); i++) {
            HBackgroundImageListener listener = (HBackgroundImageListener)list.get(i);
            if (event.getID() == HBackgroundImageEvent.BACKGROUNDIMAGE_LOADED)
                listener.imageLoaded(event);
            else
                listener.imageLoadFailed(event);
        }
    }

    private ImageProducer producer;
    private Image img;
    private ArrayList listeners = new ArrayList();
}
