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

package org.dvb.event;

import java.util.Iterator;
import java.util.LinkedList;

import javax.tv.xlet.XletContext;

import org.davic.resources.ResourceClient;
import org.davic.resources.ResourceServer;
import org.davic.resources.ResourceStatusEvent;
import org.davic.resources.ResourceStatusListener;
import org.havi.ui.HScene;
import org.videolan.BDJAction;
import org.videolan.BDJActionManager;
import org.videolan.BDJXletContext;
import org.videolan.GUIManager;

public class EventManager implements ResourceServer {
    public static EventManager getInstance() {
        synchronized (EventManager.class) {
            if (instance == null)
                instance = new EventManager();
        }
        return instance;
    }

    public boolean addUserEventListener(UserEventListener listener, ResourceClient client, UserEventRepository userEvents)
        throws IllegalArgumentException {
        if (client == null)
            throw new IllegalArgumentException();
        synchronized (this) {
            if (!cleanupReservedEvents(userEvents))
                return false;
            exclusiveUserEventListener.add(new UserEventItem(BDJXletContext.getCurrentContext(), listener, client, userEvents));
            sendResourceStatusEvent(new UserEventUnavailableEvent(userEvents));
            return true;
        }
    }

    public void addUserEventListener(UserEventListener listener, UserEventRepository userEvents) {
        if (listener == null || userEvents == null)
            throw new NullPointerException();
        synchronized (this) {
            sharedUserEventListener.add(new UserEventItem(BDJXletContext.getCurrentContext(), listener, null, userEvents));
        }
    }

    public void removeUserEventListener(UserEventListener listener) {
        XletContext context = BDJXletContext.getCurrentContext();
        synchronized (this) {
            for (Iterator it = sharedUserEventListener.iterator(); it.hasNext(); ) {
                UserEventItem item = (UserEventItem)it.next();
                if ((item.context == context) && (item.listener == listener))
                    it.remove();
            }
            for (Iterator it = exclusiveUserEventListener.iterator(); it.hasNext(); ) {
                UserEventItem item = (UserEventItem)it.next();
                if ((item.context == context) && (item.listener == listener)) {
                    sendResourceStatusEvent(new UserEventAvailableEvent(item.userEvents));
                    it.remove();
                }
            }
        }
    }

    public boolean addExclusiveAccessToAWTEvent(ResourceClient client, UserEventRepository userEvents)
            throws IllegalArgumentException {
        if (client == null)
            throw new IllegalArgumentException();
        synchronized (this) {
            if (!cleanupReservedEvents(userEvents))
                return false;
            exclusiveAWTEventListener.add(new UserEventItem(BDJXletContext.getCurrentContext(), null, client, userEvents));
            sendResourceStatusEvent(new UserEventUnavailableEvent(userEvents));
            return true;
        }
    }

    public void removeExclusiveAccessToAWTEvent(ResourceClient client) {
        XletContext context = BDJXletContext.getCurrentContext();
        synchronized (this) {
            for (Iterator it = exclusiveAWTEventListener.iterator(); it.hasNext(); ) {
                UserEventItem item = (UserEventItem)it.next();
                if ((item.context == context) && (item.client == client)) {
                    sendResourceStatusEvent(new UserEventAvailableEvent(item.userEvents));
                    it.remove();
                }
            }
        }
    }

    public void addResourceStatusEventListener(ResourceStatusListener listener) {
        synchronized (this) {
            resourceStatusEventListeners.add(listener);
        }
    }

    public void removeResourceStatusEventListener(ResourceStatusListener listener) {
        synchronized (this) {
            resourceStatusEventListeners.remove(listener);
        }
    }

    private void sendResourceStatusEvent(ResourceStatusEvent event) {
        for (Iterator it = resourceStatusEventListeners.iterator(); it.hasNext(); )
            ((ResourceStatusListener)it.next()).statusChanged(event);
    }

    public void receiveKeyEvent(int type, int modifiers, int keyCode) {
        HScene focusHScene = GUIManager.getInstance().getFocusHScene();
        if (focusHScene != null) {
            XletContext context = focusHScene.getXletContext();
            for (Iterator it = exclusiveAWTEventListener.iterator(); it.hasNext(); ) {
                UserEventItem item = (UserEventItem)it.next();
                if (item.context == context) {
                    UserEvent[] evts = item.userEvents.getUserEvent();
                    for (int i = 0; i < evts.length; i++) {
                        UserEvent evt = evts[i];
                        if ((evt.getFamily() == UserEvent.UEF_KEY_EVENT) &&
                            (evt.getFamily() == UserEvent.UEF_KEY_EVENT) &&
                            (evt.getCode() == keyCode) &&
                            (evt.getType() == type)) {

                            GUIManager.getInstance().postKeyEvent(type, modifiers, keyCode);
                            return;
                        }
                    }
                }
            }
        }

        for (Iterator it = exclusiveUserEventListener.iterator(); it.hasNext(); ) {
            UserEventItem item = (UserEventItem)it.next();
            UserEvent[] evts = item.userEvents.getUserEvent();
            for (int i = 0; i < evts.length; i++) {
                UserEvent evt = evts[i];
                if ((evt.getFamily() == UserEvent.UEF_KEY_EVENT) &&
                    (evt.getCode() == keyCode) &&
                    (evt.getType() == type)) {

                    BDJActionManager.getInstance().putCallback(new UserEventAction(item, i));
                    return;
                }
            }
        }

        GUIManager.getInstance().postKeyEvent(type, modifiers, keyCode);

        for (Iterator it = sharedUserEventListener.iterator(); it.hasNext(); ) {
            UserEventItem item = (UserEventItem)it.next();
            UserEvent[] evts = item.userEvents.getUserEvent();
            for (int i = 0; i < evts.length; i++) {
                UserEvent evt = evts[i];
                if ((evt.getFamily() == UserEvent.UEF_KEY_EVENT) &&
                    (evt.getCode() == keyCode) &&
                    (evt.getType() == type)) {
                    BDJActionManager.getInstance().putCallback(new UserEventAction(item, i));
                }
            }
        }
    }

    private boolean cleanupReservedEvents(UserEventRepository userEvents) {
        XletContext context = BDJXletContext.getCurrentContext();
        for (Iterator it = exclusiveUserEventListener.iterator(); it.hasNext(); ) {
            UserEventItem item = (UserEventItem)it.next();
                if (item.context == context)
                    continue;
                if (hasOverlap(userEvents, item.userEvents)) {
                    if (!item.client.requestRelease(item.userEvents, null))
                        return false;
                    sendResourceStatusEvent(new UserEventAvailableEvent(item.userEvents));
                    it.remove();
                }
        }
        for (Iterator it = exclusiveAWTEventListener.iterator(); it.hasNext(); ) {
            UserEventItem item = (UserEventItem)it.next();
            if (item.context == context)
                continue;
            if (hasOverlap(userEvents, item.userEvents)) {
                if (!item.client.requestRelease(item.userEvents, null))
                    return false;
                sendResourceStatusEvent(new UserEventAvailableEvent(item.userEvents));
                it.remove();
            }
        }
        return true;
    }

    private boolean hasOverlap(UserEventRepository userEvents1, UserEventRepository userEvents2) {
        UserEvent[] evts1 = userEvents1.getUserEvent();
        UserEvent[] evts2 = userEvents2.getUserEvent();
        for (int i = 0; i < evts1.length; i++) {
            UserEvent evt1 = evts1[i];
            for (int j = 0; j < evts2.length; j++) {
                UserEvent evt2 = evts2[j];
                if ((evt1.getFamily() == evt2.getFamily()) && (evt1.getCode() != evt2.getCode()))
                    return true;
            }
        }
        return false;
    }

    private class UserEventItem {
        public UserEventItem(XletContext context, UserEventListener listener,
                             ResourceClient client, UserEventRepository userEvents) {
            this.context = context;
            this.listener = listener;
            this.client = client;
            this.userEvents = userEvents;
        }

        public XletContext context;
        public UserEventListener listener;
        public ResourceClient client;
        public UserEventRepository userEvents;
    }

    private class UserEventAction extends BDJAction {
        public UserEventAction(UserEventItem item, int event) {
            super((BDJXletContext)item.context);
            this.item = item;
            this.event = event;
        }

        protected void doAction() {
            item.listener.userEventReceived(item.userEvents.getUserEvent()[event]);
        }

        private UserEventItem item;
        private int event;
    }

    private LinkedList exclusiveUserEventListener = new LinkedList();
    private LinkedList sharedUserEventListener = new LinkedList();
    private LinkedList exclusiveAWTEventListener = new LinkedList();
    private LinkedList resourceStatusEventListeners = new LinkedList();

    private static EventManager instance = null;
}
