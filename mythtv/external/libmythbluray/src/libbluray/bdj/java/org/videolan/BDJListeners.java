/*
 * This file is part of libbluray
 * Copyright (C) 2013  VideoLAN
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
package org.videolan;

import java.util.EventObject;
import java.util.Iterator;
import java.util.LinkedList;

import javax.media.ControllerEvent;
import javax.media.ControllerListener;
import javax.media.GainChangeEvent;
import javax.media.GainChangeListener;
import javax.tv.media.MediaSelectEvent;
import javax.tv.media.MediaSelectListener;
import javax.tv.service.selection.ServiceContextEvent;
import javax.tv.service.selection.ServiceContextListener;

import org.bluray.bdplus.StatusListener;

import org.bluray.media.AngleChangeEvent;
import org.bluray.media.AngleChangeListener;
import org.bluray.media.PanningChangeListener;
import org.bluray.media.PanningChangeEvent;
import org.bluray.media.PiPStatusEvent;
import org.bluray.media.PiPStatusListener;
import org.bluray.media.PlaybackListener;
import org.bluray.media.PlaybackMarkEvent;
import org.bluray.media.PlaybackPlayItemEvent;
import org.bluray.media.UOMaskTableListener;
import org.bluray.media.UOMaskTableChangedEvent;
import org.bluray.media.UOMaskedEvent;

import org.davic.resources.ResourceStatusEvent;
import org.davic.resources.ResourceStatusListener;

import org.dvb.application.AppsDatabaseEvent;
import org.dvb.application.AppsDatabaseEventListener;
import org.dvb.media.SubtitleAvailableEvent;
import org.dvb.media.SubtitleListener;
import org.dvb.media.SubtitleNotAvailableEvent;
import org.dvb.media.SubtitleNotSelectedEvent;
import org.dvb.media.SubtitleSelectedEvent;
import org.dvb.media.VideoFormatListener;
import org.dvb.media.VideoFormatEvent;

public class BDJListeners {
    private LinkedList listeners = new LinkedList();

    public void add(Object listener) {
        if (listener != null) {
            BDJXletContext ctx = BDJXletContext.getCurrentContext();
            if (ctx == null) {
                logger.error("Listener added from wrong thread: " + Logger.dumpStack());
                return;
            }
            synchronized (listeners) {
                remove(listener);
                listeners.add(new BDJListener(ctx, listener));
            }
        }
    }

    public void remove(Object listener) {
        synchronized (listeners) {
            for (Iterator it = listeners.iterator(); it.hasNext(); ) {
                BDJListener item = (BDJListener)it.next();
                if (item.listener == listener)
                    it.remove();
            }
        }
    }

    public void clear() {
        if (null != BDJXletContext.getCurrentContext()) {
            logger.error("clear() from wrong thread: " + Logger.dumpStack());
            return;
        }
        synchronized (listeners) {
            listeners.clear();
        }
    }

    public void putCallback(Object event) {
        boolean mediaQueue = true;
        /*
        if (event instanceof PlaybackMarkEvent) {
        } else if (event instanceof PlaybackPlayItemEvent) {
        } else if (event instanceof UOMaskTableChangedEvent) {
        } else if (event instanceof UOMaskedEvent) {
        } else if (event instanceof PiPStatusEvent) {
        } else if (event instanceof PanningChangeEvent) {
        } else if (event instanceof AngleChangeEvent) {
        } else if (event instanceof MediaSelectEvent) {
        } else if (event instanceof GainChangeEvent) {
        } else if (event instanceof ControllerEvent) {
        }
        */
        if (event instanceof ServiceContextEvent) {
            mediaQueue = false;
        } else if (event instanceof ResourceStatusEvent) {
            mediaQueue = false;
        } else if (event instanceof AppsDatabaseEvent) {
            mediaQueue = false;
        } else if (event instanceof PSR102Status) {
            mediaQueue = false;
        }
        putCallback(event, mediaQueue);
    }

    public void putCallback(Object event, boolean mediaQueue) {
        synchronized (listeners) {
            for (Iterator it = listeners.iterator(); it.hasNext(); ) {
                BDJListener item = (BDJListener)it.next();
                if (item.ctx == null || item.ctx.isReleased()) {
                    logger.info("Listener terminated: " + item.ctx);
                    it.remove();
                } else {
                    if (mediaQueue) {
                        item.ctx.putMediaCallback(new Callback(event, item.listener));
                    } else {
                        item.ctx.putCallback(new Callback(event, item.listener));
                    }
                }
            }
        }
    }

    private class PSR102Status {
        private PSR102Status(int value) {
            this.value = value;
        }
        public int value;
    }

    public void putPSR102Callback(int value) {
        putCallback(new PSR102Status(value));
    }

    private static class BDJListener {
        public BDJXletContext ctx;
        public Object listener;

        BDJListener(BDJXletContext ctx, Object listener) {
            this.ctx = ctx;
            this.listener = listener;
        }
    }

    private static class Callback extends BDJAction {
        private Callback(Object event, Object listener) {
            this.event = event;
            this.listener = listener;
        }

        public String toString() {
            return this.getClass().getName() + "[event=" + event + ", listener=" + listener;
        }

        protected void doAction() {
            if (event instanceof PlaybackMarkEvent) {
                ((PlaybackListener)listener).markReached((PlaybackMarkEvent)event);
            } else if (event instanceof PlaybackPlayItemEvent) {
                ((PlaybackListener)listener).playItemReached((PlaybackPlayItemEvent)event);
            } else if (event instanceof ServiceContextEvent) {
                ((ServiceContextListener)listener).receiveServiceContextEvent((ServiceContextEvent)event);
            } else if (event instanceof UOMaskTableChangedEvent) {
                ((UOMaskTableListener)listener).receiveUOMaskTableChangedEvent((UOMaskTableChangedEvent)event);
            } else if (event instanceof UOMaskedEvent) {
                ((UOMaskTableListener)listener).receiveUOMaskedEvent((UOMaskedEvent)event);
            } else if (event instanceof PiPStatusEvent) {
                ((PiPStatusListener)listener).piPStatusChange((PiPStatusEvent)event);
            } else if (event instanceof PanningChangeEvent) {
                ((PanningChangeListener)listener).panningChange((PanningChangeEvent)event);
            } else if (event instanceof AngleChangeEvent) {
                ((AngleChangeListener)listener).angleChange((AngleChangeEvent)event);
            } else if (event instanceof MediaSelectEvent) {
                ((MediaSelectListener)listener).selectionComplete((MediaSelectEvent)event);

            } else if (event instanceof GainChangeEvent) {
                ((GainChangeListener)listener).gainChange((GainChangeEvent)event);
            } else if (event instanceof ControllerEvent) {
                ((ControllerListener)listener).controllerUpdate((ControllerEvent)event);

            } else if (event instanceof ResourceStatusEvent) {
                ((ResourceStatusListener)listener).statusChanged((ResourceStatusEvent)event);

            } else if (event instanceof AppsDatabaseEvent) {
                AppsDatabaseEvent dbevent = (AppsDatabaseEvent)event;
                AppsDatabaseEventListener dblistener = (AppsDatabaseEventListener)listener;
                switch (dbevent.getEventId()) {
                case AppsDatabaseEvent.APP_ADDED:
                    dblistener.entryAdded(dbevent);
                    break;
                case AppsDatabaseEvent.APP_CHANGED:
                    dblistener.entryChanged(dbevent);
                    break;
                case AppsDatabaseEvent.APP_DELETED:
                    dblistener.entryRemoved(dbevent);
                    break;
                case AppsDatabaseEvent.NEW_DATABASE:
                    dblistener.newDatabase(dbevent);
                    break;
                }

            } else if (event instanceof SubtitleAvailableEvent || event instanceof SubtitleNotAvailableEvent ||
                       event instanceof SubtitleNotSelectedEvent || event instanceof SubtitleSelectedEvent) {
                ((SubtitleListener)listener).subtitleStatusChanged((EventObject)event);

            } else if (event instanceof VideoFormatEvent) {
                ((VideoFormatListener)listener).receiveVideoFormatEvent((VideoFormatEvent)event);

            } else if (event instanceof PSR102Status) {
                ((StatusListener)listener).receive(((PSR102Status)event).value);

            } else {
                System.err.println("Unknown event type: " + event.getClass().getName());
            }
        }

        private Object listener;
        private Object event;
    }

    private static final Logger logger = Logger.getLogger(BDJListeners.class.getName());
}
