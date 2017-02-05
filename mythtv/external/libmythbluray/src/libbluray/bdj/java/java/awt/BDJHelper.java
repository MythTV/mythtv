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
                org.videolan.Logger.getLogger("BDRootWindow").error("stopEventQueue() failed for " + t);
                org.videolan.PortingHelper.stopThread(t);
            }
        }
    }

    public static boolean postMouseEvent(int x, int y) {
        return false;
    }

    public static boolean postMouseEvent(int button) {
        return false;
    }

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
                org.videolan.Logger.getLogger("BDJHelper").error("postKeyEvent failed: " + e);
            }
        } else {
            org.videolan.Logger.getLogger("BDJHelper").error("*** KEY event dropped ***");
        }

        return false;
    }
}
