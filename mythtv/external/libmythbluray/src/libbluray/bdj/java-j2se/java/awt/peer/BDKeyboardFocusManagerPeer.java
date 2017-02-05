/*
 * This file is part of libbluray
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

package java.awt.peer;

import java.awt.Component;
import java.awt.KeyboardFocusManager;
import java.awt.Window;
import java.lang.reflect.Field;

public class BDKeyboardFocusManagerPeer implements KeyboardFocusManagerPeer {
    private static boolean java7 = false;
    static BDKeyboardFocusManagerPeer instance = null;

    /* used in java 7 only */
    public static KeyboardFocusManagerPeer getInstance() {
        java7 = true;

        if (instance == null) {
            instance = new BDKeyboardFocusManagerPeer();
        }
        return instance;
    }

    public static void shutdown()
    {
        if (instance != null) {
            instance.dispose();
            instance = null;
        }
    }

    public static void init(Window window)
    {
        /* running in java 7 ? */
        if (java7 == true)
            return;

        if (instance == null)
            instance = new BDKeyboardFocusManagerPeer();
        instance.focusOwner = null;
        instance.window = window;

        /* replace default keyboard focus manager peer */
        Field kbPeer;
        try {
            Class c = Class.forName("java.awt.KeyboardFocusManager");
            kbPeer = c.getDeclaredField("peer");
            kbPeer.setAccessible(true);
        } catch (ClassNotFoundException e) {
            throw new Error("java.awt.KeyboardFocusManager not found");
        } catch (SecurityException e) {
            throw new Error("java.awt.KeyboardFocusManager not accessible");
        } catch (NoSuchFieldException e) {
            throw new Error("java.awt.KeyboardFocusManager.peer not found");
        }
        try {
            kbPeer.set(KeyboardFocusManager.getCurrentKeyboardFocusManager(),
                       instance);
        } catch (java.lang.IllegalAccessException e) {
            throw new Error("java.awt.KeyboardFocusManager.peer not accessible:" + e);
        }
    }

    private Component focusOwner = null;
    private Window window = null; /* used in java 6 only */
    private boolean disposed = false;

    public void dispose()
    {
        focusOwner = null;
        window = null;
        disposed = true;
    }

    private BDKeyboardFocusManagerPeer() {
    }

    public void clearGlobalFocusOwner(Window w) {
    }

    public Component getCurrentFocusOwner() {
        return focusOwner;
    }

    public void setCurrentFocusOwner(Component c) {
        if (!disposed) {
            focusOwner = c;
        }
    }

    /* java 6 only */
    public void setCurrentFocusedWindow(Window w) {
        if (!disposed) {
            window = w;
        }
    }

    /* java 6 only */
    public Window getCurrentFocusedWindow() {
        return window;
    }
};


