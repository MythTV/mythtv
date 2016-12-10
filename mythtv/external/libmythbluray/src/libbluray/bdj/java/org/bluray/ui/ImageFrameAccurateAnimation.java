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

import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.MediaTracker;
import java.awt.Toolkit;
import java.awt.image.ImageObserver;

import java.lang.InterruptedException;
import java.lang.Thread;

import java.awt.BDWindowGraphics;

import org.videolan.GUIManager;
import org.videolan.Logger;

public class ImageFrameAccurateAnimation extends FrameAccurateAnimation
    implements ImageObserver/*, java.awt.MenuContainer*/ {
    public static ImageFrameAccurateAnimation getInstance(Image[] images,
            Dimension size, AnimationParameters params, int playmode)
            throws NullPointerException, IllegalArgumentException {

        if (images == null || size == null || params == null)
            throw new NullPointerException();
        if (playmode < PLAY_REPEATING || playmode > PLAY_ONCE)
            throw new IllegalArgumentException();
        if (images.length < 1)
            throw new IllegalArgumentException();
        for (int i = 0; i < images.length; i++) {
            if (images[i] == null) {
                throw new NullPointerException();
            }
        }

        if (params.scaleFactor <= 0) {
            throw new IllegalArgumentException("the scaleFactor is neither 1 or 2");
        }

        if (params.repeatCount != null) {
            if (images.length != params.repeatCount.length) {
                throw new IllegalArgumentException();
            }

            for (int i = 0; i < params.repeatCount.length; i++) {
                if (params.repeatCount[i] < 0) {
                    throw new IllegalArgumentException();
                }
            }
        }

        // TODO: check params.threadPriority

        return new ImageFrameAccurateAnimation(images, size, params, playmode);
    }

    private ImageFrameAccurateAnimation(Image[] images,
            Dimension size, AnimationParameters params, int playmode) {

        super(params);
        this.images = ((Image[])images.clone());
        this.size = ((Dimension)size.clone());
        this.playmode = playmode;
    }

    public AnimationParameters getAnimationParameters() {
        return new AnimationParameters(params);
    }

    public Image[] getImages() {
        return (Image[])images.clone();
    }

    public int getPlayMode() {
        return playmode;
    }

    public int getPosition() {
        return position;
    }

    public void prepareImages() {
        if (prepared) {
            return;
        }

        MediaTracker mt = new MediaTracker(this);
        for (int i = 0; i < images.length; i++) {
            mt.addImage(images[i], i);
        }
        try {
            mt.waitForAll();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        for (int i = 0; i < images.length; i++) {
            mt.removeImage(images[i], i);
        }

        if (params.scaleFactor != 1) {
            // TODO: scale if needed
            logger.unimplemented("image_scaling");
        }

        prepared = true;
    }

    public void setPlayMode(int mode) throws IllegalArgumentException {
        if (mode < PLAY_REPEATING  || mode > PLAY_ONCE)
            throw new IllegalArgumentException();
        this.playmode = mode;
    }

    public void setPosition(int position) {
        if (position < 0 || position >= images.length)
            throw new IllegalArgumentException();
        this.position = position;
    }


    protected void destroyImpl() {
        stopImpl();

        images = null;
        scaledImages = null;
    }

    protected void startImpl() {
        if (!prepared) {
            prepareImages();
        }

        if (animation == null) {
            animation = new AnimationThread(this);
        }
    }

    protected void stopImpl() {
        if (animation != null) {
            animation.stop();
            animation = null;
        }
    }

    public void paint(Graphics g) {
        if (images != null && images[position] != null) {
            if (!g.drawImage(images[position], super.getBounds().x, super.getBounds().y, this)) {
                logger.warning("paint(" + position + ") failed");
            }
        }
    }

    private class AnimationThread implements Runnable {
        private Thread thread = null;
        private boolean ready = false;
        private ImageFrameAccurateAnimation faa;

        AnimationThread(ImageFrameAccurateAnimation faa) {
            this.faa = faa;

            // TODO: use params.threadPriority

            thread = new Thread(this);
            thread.start();
        }

        public void stop() {
            ready = true;
            if (thread != null) {
                try {
                    thread.join();
                } catch (java.lang.InterruptedException e) {
                }
                thread = null;
            }
            faa = null;
        }

        public void run() {
            int increment = 1;

            while (!ready) {

                // paint image
                Graphics g = new BDWindowGraphics(GUIManager.getInstance());
                faa.paint(g);
                g.dispose();

                // sleep before next image
                int count = 1;
                if (faa.params.repeatCount != null && faa.params.repeatCount.length < position) {
                    count += faa.params.repeatCount[faa.position];
                }
                for (; !ready && count > 0; count--) {
                    try {
                        Thread.sleep(40);
                    } catch (InterruptedException e) {
                        return;
                    }
                }

                // next image index
                int position = faa.position + increment;
                if (position >= faa.images.length) {
                    if (faa.playmode == PLAY_REPEATING) {
                        position = 0;
                    } else if (playmode == PLAY_ALTERNATING) {
                        increment = -1;
                        position = faa.images.length - 2;
                    } else if (playmode == PLAY_ONCE) {
                        ready = true;
                        break;
                    }
                }
                if (position < 0) {
                    position = 1;
                    increment = 1;
                }
                faa.position = position;
            }

            faa.running = false;
        }
    }

    public static final int PLAY_REPEATING = 1;
    public static final int PLAY_ALTERNATING = 2;
    public static final int PLAY_ONCE = 3;

    private int playmode;
    private int position = 0;
    private Image[] images = null;
    private Image[] scaledImages = null;
    private boolean prepared = false;
    private Dimension size = null;
    private AnimationThread animation = null;

    private static final long serialVersionUID = 2691302238670178111L;

    private static final Logger logger = Logger.getLogger(FrameAccurateAnimation.class.getName());
}
