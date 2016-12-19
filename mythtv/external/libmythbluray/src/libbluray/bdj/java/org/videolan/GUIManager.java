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

import java.awt.BDRootWindow;
import java.awt.AWTException;
import java.awt.Component;
import java.awt.image.BufferedImage;

import org.havi.ui.HScene;

public class GUIManager extends BDRootWindow {
    private GUIManager() {
    }

    private static final Object instanceLock = new Object();

    public static GUIManager createInstance() {
        synchronized (instanceLock) {
            if (instance == null) {
                instance = new GUIManager();
            } else {
                instance.clearOverlay();
                instance.setDefaultFont(null);
            }
            return instance;
        }
    }

    public static GUIManager getInstance() {
        synchronized (instanceLock) {
            if (instance == null) {
                Logger.getLogger("GUIManager").error("getInstance(): no instance !");
                throw new Error("no GUIManager instance");
            }
            return instance;
        }
    }

    public BufferedImage createBufferedImage(int width, int height)
            throws AWTException {
        BufferedImage img = getGraphicsConfiguration().createCompatibleImage(width, height);

        if (img == null)
            throw new AWTException("Failed to create buffered image");

        return img;
    }

    public HScene getFocusHScene() {
        Component component = getFocusOwner();
        while (component != null) {
            if (component instanceof HScene)
                return (HScene)component;
            component = component.getParent();
        }
        return null;
    }

    protected static void shutdown() throws Throwable {
        synchronized (instanceLock) {
            if (instance != null) {
                instance.setVisible(false);
                instance.removeAll();
                instance.dispose();
                //instance.finalize();
                instance = null;
            }
        }
    }

    public void dispose() {
        try {
            super.dispose();
        } finally {
            instance = null;
        }
    }

    private static GUIManager instance = null;
    private static final long serialVersionUID = 8670041014494973439L;
}
