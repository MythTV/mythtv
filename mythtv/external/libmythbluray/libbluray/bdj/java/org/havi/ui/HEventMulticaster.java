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

import java.awt.event.ActionEvent;
import java.awt.event.FocusEvent;
import java.awt.event.KeyEvent;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.util.EventListener;
import org.davic.resources.ResourceStatusEvent;
import org.davic.resources.ResourceStatusListener;
import org.havi.ui.event.HActionListener;
import org.havi.ui.event.HAdjustmentEvent;
import org.havi.ui.event.HAdjustmentListener;
import org.havi.ui.event.HBackgroundImageEvent;
import org.havi.ui.event.HBackgroundImageListener;
import org.havi.ui.event.HFocusListener;
import org.havi.ui.event.HItemEvent;
import org.havi.ui.event.HItemListener;
import org.havi.ui.event.HKeyListener;
import org.havi.ui.event.HScreenConfigurationEvent;
import org.havi.ui.event.HScreenConfigurationListener;
import org.havi.ui.event.HScreenLocationModifiedEvent;
import org.havi.ui.event.HScreenLocationModifiedListener;
import org.havi.ui.event.HTextEvent;
import org.havi.ui.event.HTextListener;

public class HEventMulticaster implements HBackgroundImageListener,
        HScreenConfigurationListener, HScreenLocationModifiedListener,
        WindowListener, HActionListener, HAdjustmentListener, HFocusListener,
        HItemListener, HTextListener, HKeyListener, ResourceStatusListener {


    protected HEventMulticaster(EventListener a, EventListener b)
    {
        this.a = a;
        this.b = b;
    }

    protected EventListener remove(EventListener oldListener)
    {
        if (oldListener == a) 
            return b;
        else if (oldListener == b) 
            return a;
        EventListener a2 = removeInternal(a, oldListener);
        EventListener b2 = removeInternal(b, oldListener);
        if (a2 == a && b2 == b)
            return this;
        else
            return addInternal(a2, b2);
    }

    protected static EventListener addInternal(EventListener a, EventListener b)
    {
        if (a == null)
            return b;
        else if (b == null)
            return a;
        else
            return new HEventMulticaster(a, b);
    }

    protected static EventListener removeInternal(EventListener listener,
            EventListener oldListener)
    {
        if (listener == null || listener.equals(oldListener))
            return null;
        else if (listener instanceof HEventMulticaster)
            return ((HEventMulticaster) listener).remove(oldListener);
        else
            return listener;
    }

    public static HBackgroundImageListener add(HBackgroundImageListener a,
            HBackgroundImageListener b)
    {
        return (HBackgroundImageListener) addInternal(a, b);
    }

    public static HBackgroundImageListener remove(
            HBackgroundImageListener listener,
            HBackgroundImageListener oldListener)
    {
        return (HBackgroundImageListener) removeInternal(listener, oldListener);
    }

    public static WindowListener add(WindowListener a, WindowListener b)
    {
        return (WindowListener) addInternal(a, b);
    }

    public static WindowListener remove(WindowListener listener,
            WindowListener oldListener)
    {
        return (WindowListener) removeInternal(listener, oldListener);
    }

    public static HScreenConfigurationListener add(
            HScreenConfigurationListener a, HScreenConfigurationListener b)
    {
        return (HScreenConfigurationListener) addInternal(a, b);
    }

    public static HScreenConfigurationListener add(
            HScreenConfigurationListener a, HScreenConfigurationListener b,
            HScreenConfigTemplate template)
    {
        
        return (HScreenConfigurationListener) addInternal(a, b);
    }

    public static HScreenConfigurationListener remove(
            HScreenConfigurationListener listener,
            HScreenConfigurationListener oldListener)
    {
        return (HScreenConfigurationListener) removeInternal(listener,
                oldListener);
    }

    public static HScreenLocationModifiedListener add(
            HScreenLocationModifiedListener a, HScreenLocationModifiedListener b)
    {
        return (HScreenLocationModifiedListener) addInternal(a, b);
    }

    public static HScreenLocationModifiedListener remove(
            HScreenLocationModifiedListener listener,
            HScreenLocationModifiedListener oldListener)
    {
        return (HScreenLocationModifiedListener) removeInternal(listener,
                oldListener);
    }

    public static HTextListener add(HTextListener a, HTextListener b)
    {
        return (HTextListener) addInternal(a, b);
    }

    public static HTextListener remove(HTextListener listener,
            HTextListener oldListener)
    {
        return (HTextListener) removeInternal(listener, oldListener);
    }

    public static HItemListener add(HItemListener a, HItemListener b)
    {
        return (HItemListener) addInternal(a, b);
    }

    public static HItemListener remove(HItemListener listener,
            HItemListener oldListener)
    {
        return (HItemListener) removeInternal(listener, oldListener);
    }

    public static HFocusListener add(HFocusListener a, HFocusListener b)
    {
        return (HFocusListener) addInternal(a, b);
    }

    public static HFocusListener remove(HFocusListener listener,
            HFocusListener oldListener)
    {
        return (HFocusListener) removeInternal(listener, oldListener);
    }

    public static HAdjustmentListener add(HAdjustmentListener a,
            HAdjustmentListener b)
    {
        return (HAdjustmentListener) addInternal(a, b);
    }

    public static HAdjustmentListener remove(HAdjustmentListener listener,
            HAdjustmentListener oldListener)
    {
        return (HAdjustmentListener) removeInternal(listener, oldListener);
    }

    public static HActionListener add(HActionListener a, HActionListener b)
    {
        return (HActionListener) addInternal(a, b);
    }

    public static HActionListener remove(HActionListener listener,
            HActionListener oldListener)
    {
        return (HActionListener) removeInternal(listener, oldListener);
    }

    public static HKeyListener add(HKeyListener a, HKeyListener b)
    {
        return (HKeyListener) addInternal(a, b);
    }

    public static HKeyListener remove(HKeyListener listener,
            HKeyListener oldListener)
    {
        return (HKeyListener) removeInternal(listener, oldListener);
    }

    public static ResourceStatusListener add(ResourceStatusListener a,
            ResourceStatusListener b)
    {
        return (ResourceStatusListener) addInternal(a, b);
    }

    public static ResourceStatusListener remove(
            ResourceStatusListener listener, ResourceStatusListener oldListener)
    {
        return (ResourceStatusListener) removeInternal(listener, oldListener);
    }

    public void imageLoaded(HBackgroundImageEvent event)
    {
        if (a != null) ((HBackgroundImageListener)a).imageLoaded(event);
        if (b != null) ((HBackgroundImageListener)b).imageLoaded(event);
    }

    public void imageLoadFailed(HBackgroundImageEvent event)
    {
        if (a != null) ((HBackgroundImageListener)a).imageLoadFailed(event);
        if (b != null) ((HBackgroundImageListener)b).imageLoadFailed(event);
    }

    public void report(HScreenConfigurationEvent event)
    {
        if (a != null) ((HScreenConfigurationListener)a).report(event);
        if (b != null) ((HScreenConfigurationListener)b).report(event);
    }

    public void report(HScreenLocationModifiedEvent event)
    {
        if (a != null) ((HScreenLocationModifiedListener)a).report(event);
        if (b != null) ((HScreenLocationModifiedListener)b).report(event);
    }

    public void windowOpened(WindowEvent event)
    {
        if (a != null) ((WindowListener)a).windowOpened(event);
        if (b != null) ((WindowListener)b).windowOpened(event);
    }

    public void windowClosing(WindowEvent event)
    {
        if (a != null) ((WindowListener)a).windowClosing(event);
        if (b != null) ((WindowListener)b).windowClosing(event);
    }

    public void windowClosed(WindowEvent event)
    {
        if (a != null) ((WindowListener)a).windowClosed(event);
        if (b != null) ((WindowListener)b).windowClosed(event);
    }

    public void windowIconified(WindowEvent event)
    {
        if (a != null) ((WindowListener)a).windowIconified(event);
        if (b != null) ((WindowListener)b).windowIconified(event);
    }

    public void windowDeiconified(WindowEvent event)
    {
        if (a != null) ((WindowListener)a).windowDeiconified(event);
        if (b != null) ((WindowListener)b).windowDeiconified(event);
    }

    public void windowActivated(WindowEvent event)
    {
        if (a != null) ((WindowListener)a).windowActivated(event);
        if (b != null) ((WindowListener)b).windowActivated(event);
    }

    public void windowDeactivated(WindowEvent event)
    {
        if (a != null) ((WindowListener)a).windowDeactivated(event);
        if (b != null) ((WindowListener)b).windowDeactivated(event);
    }

    public void actionPerformed(ActionEvent event)
    {
        if (a != null) ((HActionListener)a).actionPerformed(event);
        if (b != null) ((HActionListener)b).actionPerformed(event);
    }

    public void focusLost(FocusEvent event)
    {
        if (a != null) ((HFocusListener)a).focusLost(event);
        if (b != null) ((HFocusListener)b).focusLost(event);
    }

    public void focusGained(FocusEvent event)
    {
        if (a != null) ((HFocusListener)a).focusGained(event);
        if (b != null) ((HFocusListener)b).focusGained(event);
    }

    public void valueChanged(HAdjustmentEvent event)
    {
        if (a != null) ((HAdjustmentListener)a).valueChanged(event);
        if (b != null) ((HAdjustmentListener)b).valueChanged(event);
    }

    public void selectionChanged(HItemEvent event)
    {
        if (a != null) ((HItemListener)a).selectionChanged(event);
        if (b != null) ((HItemListener)b).selectionChanged(event);
    }

    public void currentItemChanged(HItemEvent event)
    {
        if (a != null) ((HItemListener)a).currentItemChanged(event);
        if (b != null) ((HItemListener)b).currentItemChanged(event);
    }

    public void textChanged(HTextEvent event)
    {
        if (a != null) ((HTextListener)a).textChanged(event);
        if (b != null) ((HTextListener)b).textChanged(event);
    }

    public void caretMoved(HTextEvent event)
    {
        if (a != null) ((HTextListener)a).caretMoved(event);
        if (b != null) ((HTextListener)b).caretMoved(event);
    }

    public void keyTyped(KeyEvent event)
    {
        if (a != null) ((HKeyListener)a).keyTyped(event);
        if (b != null) ((HKeyListener)b).keyTyped(event);
    }

    public void keyPressed(KeyEvent event)
    {
        if (a != null) ((HKeyListener)a).keyPressed(event);
        if (b != null) ((HKeyListener)b).keyPressed(event);
    }

    public void keyReleased(KeyEvent event)
    {
        if (a != null) ((HKeyListener)a).keyReleased(event);
        if (b != null) ((HKeyListener)b).keyReleased(event);
    }

    public void statusChanged(ResourceStatusEvent event)
    {
        if (a != null) ((ResourceStatusListener)a).statusChanged(event);
        if (b != null) ((ResourceStatusListener)b).statusChanged(event);
    }

    protected final EventListener a, b;
}
