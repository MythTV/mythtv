 /*
 * This file is part of libbluray
 * Copyright (C) 2012  Libbluray
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

import java.awt.event.InvocationEvent;
import java.awt.event.KeyEvent;
import java.awt.event.MouseEvent;

import org.videolan.Logger;

public class BDJHelper {

    public static EventDispatchThread getEventDispatchThread(EventQueue eq) {
        if (eq != null) {
            return eq.getDispatchThread();
        }
        return null;
    }

    public static void stopEventQueue(EventQueue eq) {
        EventDispatchThread t = eq.getDispatchThread();
        if (t != null && t.isAlive()) {

            final long DISPOSAL_TIMEOUT = 5000;
            final Object notificationLock = new Object();
            Runnable runnable = new Runnable() { public void run() {
                synchronized(notificationLock) {
                    notificationLock.notifyAll();
                }
            } };

            synchronized (notificationLock) {
                eq.postEvent(new InvocationEvent(Toolkit.getDefaultToolkit(), runnable));
                try {
                    notificationLock.wait(DISPOSAL_TIMEOUT);
                } catch (InterruptedException e) {
                }
            }

            t.stopDispatching();
            if (t.isAlive()) {
                t.interrupt();
            }

            try {
                t.join(1000);
            } catch (InterruptedException e) {
            }
            if (t.isAlive()) {
                logger.error("stopEventQueue() failed for " + t);
                org.videolan.PortingHelper.stopThread(t);
            }
        }
    }

    /*
     * Mouse events
     */

    private static int mouseX = 0, mouseY = 0, mouseMask = 0;

    public static boolean postMouseEvent(int x, int y) {
        mouseX = x;
        mouseY = y;
        return postMouseEventImpl(MouseEvent.MOUSE_MOVED, MouseEvent.NOBUTTON);
    }

    public static boolean postMouseEvent(int id) {
        boolean r;

        if (id == MouseEvent.MOUSE_PRESSED)
            mouseMask = MouseEvent.BUTTON1_MASK;

        r = postMouseEventImpl(id, MouseEvent.BUTTON1);

        if (id == MouseEvent.MOUSE_RELEASED)
            mouseMask = 0;

        return r;
    }

    private static boolean postMouseEventImpl(int id, int button) {
        Component focusOwner = KeyboardFocusManager.getCurrentKeyboardFocusManager().getGlobalFocusOwner();
        if (focusOwner != null) {
            EventQueue eq = BDToolkit.getEventQueue(focusOwner);
            if (eq != null) {
                long when = System.currentTimeMillis();
                try {
                    eq.postEvent(new MouseEvent(focusOwner, id, when, mouseMask, mouseX, mouseY,
                                                (id == MouseEvent.MOUSE_CLICKED) ? 1 : 0, false, button));
                    return true;
                } catch (Exception e) {
                    logger.error("postMouseEvent failed: " + e);
                }
            }
        }
        return false;
    }

    /*
     * Key events
     */

    public static boolean postKeyEvent(int id, int modifiers, int keyCode) {
        Component focusOwner = KeyboardFocusManager.getCurrentKeyboardFocusManager().getGlobalFocusOwner();
        if (focusOwner != null) {
            long when = System.currentTimeMillis();
            KeyEvent event;
            try {
                if (id == KeyEvent.KEY_TYPED)
                    event = new KeyEvent(focusOwner, id, when, modifiers, KeyEvent.VK_UNDEFINED, (char)keyCode);
                else
                    event = new KeyEvent(focusOwner, id, when, modifiers, keyCode, KeyEvent.CHAR_UNDEFINED);

                EventQueue eq = BDToolkit.getEventQueue(focusOwner);
                if (eq != null) {
                    eq.postEvent(event);
                    return true;
                }
            } catch (Exception e) {
                logger.error("postKeyEvent failed: " + e);
            }
        } else {
            logger.error("KEY event dropped (no focus owner)");
        }

        return false;
    }

    private static final Logger logger = Logger.getLogger(BDJHelper.class.getName());
}
