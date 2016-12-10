/*
 * This file is part of libbluray
 * Copyright (C) 2012  Libbluray
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

import java.util.Arrays;
import java.util.Timer;
import java.util.TimerTask;

import org.videolan.Logger;
import org.videolan.Libbluray;

public class BDRootWindow extends Frame {

    public BDRootWindow () {
        super();
        setUndecorated(true);
        setBackground(new Color(0, 0, 0, 0));
        BDToolkit.setFocusedWindow(this);
    }

    public Area getDirtyArea() {
        return dirty;
    }

    public Font getDefaultFont() {
        return defaultFont;
    }

    public void setDefaultFont(String fontId) {
        if (fontId == null || fontId.equals("*****")) {
            defaultFont = null;
        } else {
            try {
                defaultFont = (new org.dvb.ui.FontFactory()).createFont(fontId);
            } catch (Exception ex) {
                logger.error("Failed setting default font " + fontId + ".otf: " + ex);
            }
        }
        logger.info("setting default font to " + fontId + ".otf (" + defaultFont + ")");
        setFont(defaultFont);
    }


    public void setBounds(int x, int y, int width, int height) {
        if (!isVisible()) {
            if ((width > 0) && (height > 0)) {
                if ((backBuffer == null) || (getWidth() * getHeight() < width * height)) {
                    backBuffer = new int[width * height];
                    Arrays.fill(backBuffer, 0);
                }
            }
            super.setBounds(x, y, width, height);
        } else if (width != getWidth() || height != getHeight()){
            logger.error("setBounds(" + x + "," + y + "," + width + "," + height + ") FAILED: already visible");
        }
    }

    public int[] getBdBackBuffer() {
        return backBuffer;
    }

    public Image getBackBuffer() {
        /* exists only in J2SE */
        logger.unimplemented("getBackBuffer");
        return null;
    }

    private boolean isBackBufferClear() {
        int v = 0;
        for (int i = 0; i < height * width; i++)
            v |= backBuffer[i];
        return v == 0;
    }

    public void notifyChanged() {
        if (!isVisible()) {
            logger.error("sync(): not visible");
            return;
        }
        synchronized (this) {
            if (timer == null) {
                logger.error("notifyChanged(): window already disposed");
                return;
            }
            changeCount++;
            if (timerTask == null) {
                timerTask = new RefreshTimerTask(this);
                timer.schedule(timerTask, 40, 40);
            }
        }
    }

    public void sync() {
        synchronized (this) {
            if (timerTask != null) {
                timerTask.cancel();
                timerTask = null;
            }
            changeCount = 0;

            if (!isVisible()) {
                if (overlay_open) {
                    logger.info("sync(): close OSD (not visible)");
                    close();
                }
                logger.info("sync() ignored (not visible)");
                return;
            }

            Area a = dirty.getBoundsAndClear();

            if (!a.isEmpty()) {
                if (!overlay_open) {

                    /* delay opening overlay until something has been drawn */
                    if (isBackBufferClear()) {
                        logger.info("sync() ignored (overlay not open, empty overlay)");
                        return;
                    }

                    Libbluray.updateGraphic(getWidth(), getHeight(), null);
                    overlay_open = true;
                    a = new Area(getWidth(), getHeight()); /* force full plane update */
                }
                Libbluray.updateGraphic(getWidth(), getHeight(), backBuffer, a.x0, a.y0, a.x1, a.y1);
            }
        }
    }

    private class RefreshTimerTask extends TimerTask {
        public RefreshTimerTask(BDRootWindow window) {
            this.window = window;
            this.changeCount = window.changeCount;
        }

        public void run() {
            synchronized (window) {
                if (this.changeCount == window.changeCount)
                    window.sync();
                else
                    this.changeCount = window.changeCount;
            }
        }

        private BDRootWindow window;
        private int changeCount;
    }

    private void close() {
        synchronized (this) {
            if (overlay_open) {
                Libbluray.updateGraphic(0, 0, null);
                overlay_open = false;
            }
        }
    }

    public void setVisible(boolean visible) {

        super.setVisible(visible);

        if (!visible) {
            close();
        }
    }

    /* called when new title starts (window is "created" again) */
    public void clearOverlay() {
        synchronized (this) {
            if (overlay_open) {
                logger.error("clearOverlay() ignored (overlay is visible)");
            } else {
                dirty.getBoundsAndClear();
                Arrays.fill(backBuffer, 0);
            }
        }
    }

    public void dispose()
    {
        synchronized (this) {
            if (timerTask != null) {
                timerTask.cancel();
                timerTask = null;
            }
            if (timer != null) {
                timer.cancel();
                timer = null;
            }
        }

        if (isVisible()) {
            hide();
        }

        BDToolkit.setFocusedWindow(null);

        super.dispose();

        backBuffer = null;
    }

    private int[] backBuffer = null;
    private Area dirty = new Area();
    private int changeCount = 0;
    private Timer timer = new Timer();
    private TimerTask timerTask = null;
    private boolean overlay_open = false;
    private Font defaultFont = null;

    private static final Logger logger = Logger.getLogger(BDRootWindow.class.getName());

    private static final long serialVersionUID = -8325961861529007953L;
}
