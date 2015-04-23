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
import javax.tv.service.selection.PresentationTerminatedEvent;
import javax.tv.service.selection.ServiceContentHandler;
import javax.tv.service.selection.ServiceContextDestroyedEvent;
import javax.tv.service.selection.ServiceContextEvent;
import javax.tv.service.selection.ServiceContextListener;

import org.bluray.ti.Title;
import org.bluray.ti.TitleImpl;
import org.videolan.BDJLoader;
import org.videolan.BDJAction;
import org.videolan.BDJActionManager;
import org.videolan.BDJLoaderCallback;
import org.videolan.media.content.playlist.Handler;

public class TitleContextImpl implements TitleContext {
    public Service getService() {
        return title;
    }

    public ServiceContentHandler[] getServiceContentHandlers() throws SecurityException {
        if (state == STATE_DESTROYED)
            throw new IllegalStateException();
        if (state == STATE_STOPPED)
            return new ServiceContentHandler[0];
        ServiceContentHandler[] handler = new ServiceContentHandler[1];
        handler[0] = new Handler();
        return handler;
    }

    public void start(Title title, boolean restart) throws SecurityException {
        if (state == STATE_DESTROYED)
            throw new IllegalStateException();
        TitleStartAction action = new TitleStartAction(this, (TitleImpl)title);
        if (!BDJLoader.load((TitleImpl)title, restart, action))
            action.loaderDone(false);
    }

    public void select(Service service) throws SecurityException {
        start((Title)service, true);
    }

    public void select(Locator[] locators)
        throws InvalidLocatorException, InvalidServiceComponentException, SecurityException {
        select(SIManager.createInstance().getService(locators[0]));
    }

    public void stop() throws SecurityException {
        if (state == STATE_DESTROYED)
            throw new IllegalStateException();
        TitleStopAction action = new TitleStopAction(this);
        if (!BDJLoader.unload(action))
            action.loaderDone(false);
    }

    public void destroy() throws SecurityException {
        if (state != STATE_DESTROYED) {
            state = STATE_DESTROYED;
            TitleStopAction action = new TitleStopAction(this);
            if (!BDJLoader.unload(action))
                action.loaderDone(false);
        }
    }

    public void addListener(ServiceContextListener listener) {
        if (listener != null) {
            synchronized (listeners) {
                listeners.add(listener);
            }
        }
    }

    public void removeListener(ServiceContextListener listener) {
        if (listener != null) {
            synchronized (listeners) {
                listeners.remove(listener);
            }
        }
    }

    public void removeServiceContentHandler(ServiceContentHandler handler) {
        if (handler != null) {
            synchronized (handlers) {
                handlers.remove(handler);
            }
        }
    }

    private class TitleCallbackAction extends BDJAction {
        private TitleCallbackAction(TitleContextImpl context, ServiceContextEvent event) {
            this.context = context;
        }

        protected void doAction() {
            LinkedList list;
            synchronized (context.listeners) {
                list = (LinkedList)context.listeners.clone();
            }
            for (int i = 0; i < list.size(); i++)
                ((ServiceContextListener)list.get(i)).receiveServiceContextEvent(event);
        }

        private TitleContextImpl context;
        private ServiceContextEvent event;
    }

    private class TitleStartAction implements BDJLoaderCallback {
        private TitleStartAction(TitleContextImpl context, TitleImpl title) {
            this.context = context;
            this.title = title;
        }

        public void loaderDone(boolean succeed) {
            if (succeed) {
                context.title = title;
                context.state = STATE_STARTED;
                BDJActionManager.getInstance().putCallback(new TitleCallbackAction(context, new NormalContentEvent(context)));
            }
        }

        private TitleContextImpl context;
        private TitleImpl title;
    }

    private class TitleStopAction implements BDJLoaderCallback {
        private TitleStopAction(TitleContextImpl context) {
            this.context = context;
        }

        public void loaderDone(boolean succeed) {
            if (succeed) {
                BDJActionManager.getInstance().putCallback(
                        new TitleCallbackAction(context,
                                                new PresentationTerminatedEvent(context, PresentationTerminatedEvent.USER_STOP)));
                if (context.state == STATE_DESTROYED)
                    BDJActionManager.getInstance().putCallback(
                            new TitleCallbackAction(context,
                                                    new ServiceContextDestroyedEvent(context)));
                else
                    context.state = STATE_STOPPED;
            }
        }

        private TitleContextImpl context;
    }

    private static final int STATE_STOPPED = 0;
    private static final int STATE_STARTED = 1;
    private static final int STATE_DESTROYED = 2;
    private LinkedList<ServiceContextListener> listeners = new LinkedList<ServiceContextListener>();
    private LinkedList<ServiceContentHandler> handlers = new LinkedList<ServiceContentHandler>();
    private TitleImpl title = null;
    private int state = STATE_STOPPED;
}
