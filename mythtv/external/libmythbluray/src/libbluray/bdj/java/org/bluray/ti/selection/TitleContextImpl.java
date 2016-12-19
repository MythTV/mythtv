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
package org.bluray.ti.selection;

import java.util.LinkedList;

import javax.tv.locator.InvalidLocatorException;
import javax.tv.locator.Locator;
import javax.tv.service.SIManager;
import javax.tv.service.Service;
import javax.tv.service.selection.InvalidServiceComponentException;
import javax.tv.service.selection.NormalContentEvent;
import javax.tv.service.selection.PresentationChangedEvent;
import javax.tv.service.selection.PresentationTerminatedEvent;
import javax.tv.service.selection.SelectionFailedEvent;
import javax.tv.service.selection.SelectPermission;
import javax.tv.service.selection.ServiceContentHandler;
import javax.tv.service.selection.ServiceContextDestroyedEvent;
import javax.tv.service.selection.ServiceContextEvent;
import javax.tv.service.selection.ServiceContextListener;
import javax.tv.service.selection.ServiceContextPermission;

import org.bluray.ti.Title;
import org.bluray.ti.TitleImpl;

import org.videolan.BDJLoader;
import org.videolan.BDJLoaderCallback;
import org.videolan.BDJListeners;
import org.videolan.Logger;
import org.videolan.media.content.PlayerManager;

public class TitleContextImpl implements TitleContext {
    public Service getService() {
        return title;
    }

    public ServiceContentHandler[] getServiceContentHandlers() throws SecurityException {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new ServiceContextPermission("getServiceContentHandlers", "own"));
        }

        if (state == STATE_DESTROYED)
            throw new IllegalStateException();
        if (state == STATE_STOPPED)
            return new ServiceContentHandler[0];

        ServiceContentHandler player = PlayerManager.getInstance().getPlaylistPlayer();
        if (player != null) {
            ServiceContentHandler[] handler = new ServiceContentHandler[1];
            handler[0] = player;
            return handler;
        }

        System.err.println("getServiceContentHandlers(): none found");
        return new ServiceContentHandler[0];
    }

    public void start(Title title, boolean restart) throws SecurityException {
        logger.info("start(" + title.getName() + ", restart=" + restart + ")");

        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new SelectPermission(title.getLocator(), "own"));
        }
        if (state == STATE_DESTROYED) {
            logger.error("start() failed: Title Context already destroyed");
            throw new IllegalStateException();
        }

        if (!restart && (this.title == null || !title.equals(this.title))) {
            /* force restarting of service bound Xlets when title changes */
            logger.info("start(): title changed,  force restart");
            restart = true;
        }

        TitleStartAction action = new TitleStartAction(this, (TitleImpl)title);
        if (!BDJLoader.load((TitleImpl)title, restart, action))
            action.loaderDone(false);
    }

    public void select(Service service) throws SecurityException {
        logger.info("select(" + service.getName() + ")");
        start((Title)service, true);
    }

    public void select(Locator[] locators)
        throws InvalidLocatorException, InvalidServiceComponentException, SecurityException {

        Logger.unimplemented("TitleContextImpl", "select(Locator[])");

        for (int i = 0; i < locators.length; i++)
            logger.info("  [" + i + "]: " + locators[i]);

        select(SIManager.createInstance().getService(locators[0]));
    }

    public void stop() throws SecurityException {
        logger.info("stop()");

        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new ServiceContextPermission("stop", "own"));
        }

        if (state == STATE_DESTROYED)
            throw new IllegalStateException();
        TitleStopAction action = new TitleStopAction(this);
        if (!BDJLoader.unload(action))
            action.loaderDone(false);
    }

    public void destroy() throws SecurityException {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new ServiceContextPermission("stop", "own"));
        }

        if (state != STATE_DESTROYED) {
            state = STATE_DESTROYED;
            TitleStopAction action = new TitleStopAction(this);
            if (!BDJLoader.unload(action))
                action.loaderDone(false);
        }
    }

    public void addListener(ServiceContextListener listener) {
        listeners.add(listener);
    }

    public void removeListener(ServiceContextListener listener) {
        listeners.remove(listener);
    }

    public void presentationChanged() {
        if (state == STATE_STARTED) {
            postEvent(new PresentationChangedEvent(this));
        }
    }

    private void postEvent(ServiceContextEvent event)
    {
        listeners.putCallback(event);
    }

    private static class TitleStartAction implements BDJLoaderCallback {
        private TitleStartAction(TitleContextImpl context, TitleImpl title) {
            this.context = context;
            this.title = title;
        }

        public void loaderDone(boolean succeed) {
            if (succeed) {
                context.title = title;
                context.state = STATE_STARTED;
                context.postEvent(new NormalContentEvent(context));
            } else {
                context.postEvent(new SelectionFailedEvent(context, SelectionFailedEvent.OTHER));
            }
        }

        private TitleContextImpl context;
        private TitleImpl title;
    }

    private static class TitleStopAction implements BDJLoaderCallback {
        private TitleStopAction(TitleContextImpl context) {
            this.context = context;
        }

        public void loaderDone(boolean succeed) {
            if (succeed) {
                context.postEvent(new PresentationTerminatedEvent(context, PresentationTerminatedEvent.USER_STOP));
                if (context.state == STATE_DESTROYED)
                    context.postEvent(new ServiceContextDestroyedEvent(context));
                else
                    context.state = STATE_STOPPED;
            } else {
                context.postEvent(new SelectionFailedEvent(context, SelectionFailedEvent.OTHER));
            }
        }

        private TitleContextImpl context;
    }

    private static final int STATE_STOPPED = 0;
    private static final int STATE_STARTED = 1;
    private static final int STATE_DESTROYED = 2;
    private BDJListeners listeners = new BDJListeners();
    private TitleImpl title = null;
    private int state = STATE_STOPPED;

    private static final Logger logger = Logger.getLogger(TitleContextImpl.class.getName());
}
