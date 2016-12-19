/*
 * This file is part of libbluray
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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
import java.awt.color.ColorSpace;
import java.awt.image.ColorModel;
import java.awt.image.DataBuffer;
import java.awt.image.DirectColorModel;
import java.awt.image.ImageConsumer;
import java.awt.image.ImageProducer;

class BDOffScreenImageSource implements ImageProducer {
    private ImageConsumer consumer;
    private int width;
    private int height;
    private int[] buffer;

    public BDOffScreenImageSource(int[] buffer, int w, int h) {
        width = w;
        height = h;
        this.buffer = buffer;
    }

    public void addConsumer(ImageConsumer ic) {
        consumer = ic;
        produce();
    }

    public boolean isConsumer(ImageConsumer ic) {
        return (ic == consumer);
    }

    public void removeConsumer(ImageConsumer ic) {
        if (consumer == ic) {
            consumer = null;
        }
    }

    public void startProduction(ImageConsumer ic) {
        addConsumer(ic);
    }

    private ColorModel getColorModel() {
        return new DirectColorModel(ColorSpace.getInstance(ColorSpace.CS_sRGB),
                32, 0x00ff0000, 0x0000ff00,     0x000000ff, 0xff000000, true,
                DataBuffer.TYPE_INT);
    }

    private void sendPixels()
    {
        if (consumer != null) {
            consumer.setPixels(0, 0, width, height, getColorModel(), buffer, 0, width);
        }
    }

    private void produce() {
        if (consumer != null) {
            consumer.setDimensions(width, height);
        }
        if (consumer != null) {
            consumer.setProperties(new Hashtable());
        }
        sendPixels();
        if (consumer != null) {
            consumer.imageComplete(ImageConsumer.SINGLEFRAMEDONE);
        }
    }

    public void requestTopDownLeftRightResend(ImageConsumer ic) {}
}
