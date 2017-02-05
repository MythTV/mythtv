/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2012  Petri Hintukainen <phintuka@users.sourceforge.net>
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

package org.bluray.ui;

import java.awt.Component;
import java.awt.Graphics;

import org.videolan.BDJXletContext;
import org.videolan.Logger;

public abstract class FrameAccurateAnimation extends Component {

    public static final float getDefaultFrameRate() {
        return defaultRate;
    }

    public static boolean setDefaultFrameRate(float framerate) {
        if (framerate != FRAME_RATE_23_976 &&
            framerate != FRAME_RATE_24 &&
            framerate != FRAME_RATE_25 &&
            framerate != FRAME_RATE_29_97 &&
            framerate != FRAME_RATE_50 &&
            framerate != FRAME_RATE_59_94) {

            return false;
        }

        defaultRate = framerate;
        return true;
    }

    public FrameAccurateAnimation() {
        logger.unimplemented("FrameAccurateAnimation");
    }

    public FrameAccurateAnimation(AnimationParameters params) {
        context = BDJXletContext.getCurrentContext();
        if (context != null) {
            context.addFAA(this);
        } else {
            logger.error("FrameAccurateAnimation created from wrong thread: " + Logger.dumpStack());
        }

        this.params = new AnimationParameters(params);
    }

    public synchronized void destroy() {
        if (context != null) {
            context.removeFAA(this);
            context = null;
        }

        destroyImpl();
    }

    public long getCompletedFrameCount() {
        logger.unimplemented("getCompletedFrameCount");
        return 0;
    }

    public float getFrameRate() {
        logger.unimplemented("getFrameRate");
        // TODO: rate of background video. if none, defaultRate.
        return getDefaultFrameRate();
    }

    public Graphics getGraphics() {
        logger.unimplemented("getGraphics");
        return super.getGraphics();
    }

    public int[] getRepeatCounts() {
        int[] repeatCount = null;
        if (params != null && params.repeatCount != null) {
            repeatCount = (int[])params.repeatCount.clone();
        }
        return repeatCount;
    }

    public int getThreadPriority() {
        return params.threadPriority;
    }

    public synchronized boolean isAnimated() {
        return running;
    }

    public void paint(Graphics g) {
        // should be implemented in derived classes
        logger.unimplemented("paint");
    }

    public synchronized void resetStartStopTime(
            FrameAccurateAnimationTimer newTimer) {
        params.faaTimer = new FrameAccurateAnimationTimer(newTimer);
        logger.unimplemented("resetStartStopTime");
    }

    public void setBounds(int x, int y, int width, int height) {
        super.setBounds(x, y, width, height);
    }

    public void setLocation(int x, int y) {
        super.setLocation(x, y);
    }

    public void setThreadPriority(int p) {
        params.threadPriority = p;
    }

    protected void startImpl() {
        // should be implemented in derived classes
        logger.unimplemented("startImpl");
    }

    protected void stopImpl() {
        // should be implemented in derived classes
        logger.unimplemented("stopImpl");
    }

    protected void destroyImpl() {
        // should be implemented in derived classes
        logger.unimplemented("destroyImpl");
    }

    public synchronized void start() {
        if (!running) {
            running = true;
            // TODO: compare timer against video

            if (params.faaTimer != null) {
                logger.unimplemented("start(faaTimer)");
            }

            startImpl();
        }
    }

    public synchronized void stop() {
        if (running) {
            running = false;
            stopImpl();
        }
    }

    public String toString() {
        return "FrameAccurateAnimation";
    }

    private BDJXletContext context;
    protected boolean running;
    protected AnimationParameters params;

    public static final float FRAME_RATE_23_976 = 23.976F;
    public static final float FRAME_RATE_24 = 24.0F;
    public static final float FRAME_RATE_25 = 25.0F;
    public static final float FRAME_RATE_29_97 = 29.969999F;
    public static final float FRAME_RATE_50 = 50.0F;
    public static final float FRAME_RATE_59_94 = 59.939999F;

    private static final long serialVersionUID = 76982966057159330L;

    private static float defaultRate = FRAME_RATE_25;

    private static final Logger logger = Logger.getLogger(FrameAccurateAnimation.class.getName());
}
