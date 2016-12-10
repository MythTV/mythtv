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

import java.awt.BDJHelper;
import java.util.Iterator;
import java.util.LinkedList;

import javax.tv.xlet.XletContext;

import org.davic.resources.ResourceClient;
import org.davic.resources.ResourceServer;
import org.davic.resources.ResourceStatusEvent;
import org.davic.resources.ResourceStatusListener;
import org.havi.ui.HScene;
import org.videolan.BDJAction;
import org.videolan.BDJXletContext;
import org.videolan.GUIManager;
import org.videolan.Logger;

public class EventManager implements ResourceServer {

    private static final Object instanceLock = new Object();

    public static EventManager getInstance() {
        synchronized (instanceLock) {
            if (instance == null)
                instance = new EventManager();
            return instance;
        }
    }

    public static void shutdown() {
        EventManager e;
        synchronized (instanceLock) {
            e = instance;
            instance = null;
        }
        if (e != null) {
            e.exclusiveUserEventListener.clear();
            e.sharedUserEventListener.clear();
            e.exclusiveAWTEventListener.clear();
            e.resourceStatusEventListeners.clear();
        }
    }

    public boolean addUserEventListener(UserEventListener listener, ResourceClient client, UserEventRepository userEvents)
        throws IllegalArgumentException {
        if (client == null)
            throw new IllegalArgumentException();
        BDJXletContext context = BDJXletContext.getCurrentContext();
        synchronized (this) {
            if (!cleanupReservedEvents(userEvents))
                return false;
            exclusiveUserEventListener.add(new UserEventItem(context, listener, client, userEvents));
            sendResourceStatusEvent(new UserEventUnavailableEvent(userEvents));
            return true;
        }
    }

    public void addUserEventListener(UserEventListener listener, UserEventRepository userEvents) {
        if (listener == null || userEvents == null)
            throw new NullPointerException();
        BDJXletContext context = BDJXletContext.getCurrentContext();
        synchronized (this) {
            sharedUserEventListener.add(new UserEventItem(context, listener, null, userEvents));
        }
    }

    public void removeUserEventListener(UserEventListener listener) {
        BDJXletContext context = BDJXletContext.getCurrentContext();
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
        BDJXletContext context = BDJXletContext.getCurrentContext();
        synchronized (this) {
            if (!cleanupReservedEvents(userEvents))
                return false;
            exclusiveAWTEventListener.add(new UserEventItem(context, null, client, userEvents));
            sendResourceStatusEvent(new UserEventUnavailableEvent(userEvents));
            return true;
        }
    }

    public void removeExclusiveAccessToAWTEvent(ResourceClient client) {
        BDJXletContext context = BDJXletContext.getCurrentContext();
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
        receiveKeyEventN(type, modifiers, keyCode);
    }

    public boolean receiveKeyEventN(int type, int modifiers, int keyCode) {
        UserEvent ue = new UserEvent(this, 1, type, keyCode, modifiers, System.currentTimeMillis());
        HScene focusHScene = GUIManager.getInstance().getFocusHScene();
        boolean result = false;

        if (focusHScene != null) {
            BDJXletContext context = focusHScene.getXletContext();
            for (Iterator it = exclusiveAWTEventListener.iterator(); it.hasNext(); ) {
                UserEventItem item = (UserEventItem)it.next();
                if (item.context == null || item.context.isReleased()) {
                    logger.error("Removing exclusive AWT event listener for " + item.context);
                    it.remove();
                    continue;
                }
                if (item.context == context) {
                    if (item.userEvents.contains(ue)) {
                        result = BDJHelper.postKeyEvent(type, modifiers, keyCode);
                        logger.info("Key posted to exclusive AWT event listener, r=" + result);
                        return true;
                    }
                }
            }
        } else {
            logger.info("No focused HScene found !");
        }

        for (Iterator it = exclusiveUserEventListener.iterator(); it.hasNext(); ) {
            UserEventItem item = (UserEventItem)it.next();
            if (item.context == null || item.context.isReleased()) {
                logger.error("Removing exclusive UserEvent listener for " + item.context);
                it.remove();
                continue;
            }
            if (item.userEvents.contains(ue)) {
                item.context.putUserEvent(new UserEventAction(item, ue));
                logger.info("Key posted to exclusive UE listener");
                return true;
            }
        }

        result = BDJHelper.postKeyEvent(type, modifiers, keyCode);

        for (Iterator it = sharedUserEventListener.iterator(); it.hasNext(); ) {
            UserEventItem item = (UserEventItem)it.next();
            if (item.context == null || item.context.isReleased()) {
                logger.error("Removing UserEvent listener for " + item.context);
                it.remove();
                continue;
            }
            if (item.userEvents.contains(ue)) {
                item.context.putUserEvent(new UserEventAction(item, ue));
                logger.info("Key posted to shared UE listener");
                result = true;
            }
        }

        return result;
    }

    private boolean cleanupReservedEvents(UserEventRepository userEvents) {
        BDJXletContext context = BDJXletContext.getCurrentContext();
        for (Iterator it = exclusiveUserEventListener.iterator(); it.hasNext(); ) {
            UserEventItem item = (UserEventItem)it.next();
                if (item.context == context)
                    continue;
                if (hasOverlap(userEvents, item.userEvents)) {
                    try {
                        if (!item.client.requestRelease(item.userEvents, null))
                            return false;
                    } catch (Exception e) {
                        logger.error("requestRelease() failed: " + e.getClass());
                        e.printStackTrace();
                        return false;
                    }
                    sendResourceStatusEvent(new UserEventAvailableEvent(item.userEvents));
                    it.remove();
                }
        }
        for (Iterator it = exclusiveAWTEventListener.iterator(); it.hasNext(); ) {
            UserEventItem item = (UserEventItem)it.next();
            if (item.context == context)
                continue;
            if (hasOverlap(userEvents, item.userEvents)) {
                try {
                    if (!item.client.requestRelease(item.userEvents, null))
                        return false;
                } catch (Exception e) {
                    logger.error("requestRelease() failed: " + e.getClass());
                    e.printStackTrace();
                    return false;
                }
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

    private static class UserEventItem {
        public UserEventItem(BDJXletContext context, UserEventListener listener,
                             ResourceClient client, UserEventRepository userEvents) {
            this.context = context;
            this.listener = listener;
            this.client = client;
            this.userEvents = userEvents.getNewInstance();
            if (context == null) {
                logger.error("Missing xlet context: " + Logger.dumpStack());
            }
        }

        public final BDJXletContext context;
        public final UserEventListener listener;
        public final ResourceClient client;
        public final UserEventRepository userEvents;
    }

    private static class UserEventAction extends BDJAction {
        public UserEventAction(UserEventItem item, UserEvent event) {
            this.listener = item.listener;
            this.event = event;
        }

        protected void doAction() {
            listener.userEventReceived(event);
        }

        private UserEventListener listener;
        private UserEvent event;
    }

    private LinkedList exclusiveUserEventListener = new LinkedList();
    private LinkedList sharedUserEventListener = new LinkedList();
    private LinkedList exclusiveAWTEventListener = new LinkedList();
    private LinkedList resourceStatusEventListeners = new LinkedList();

    private static EventManager instance = null;

    private static final Logger logger = Logger.getLogger(EventManager.class.getName());
}
