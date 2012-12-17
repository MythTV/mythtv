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

package org.videolan;

import java.io.InvalidObjectException;
import java.util.Enumeration;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.bluray.net.BDLocator;
import org.bluray.ti.TitleImpl;
import org.davic.media.MediaLocator;
import org.dvb.application.AppID;
import org.dvb.application.AppsDatabase;
import org.dvb.application.CurrentServiceFilter;

import javax.media.Manager;
import javax.tv.locator.Locator;
import javax.tv.service.SIManager;

import org.videolan.bdjo.AppEntry;
import org.videolan.bdjo.Bdjo;
import org.videolan.bdjo.GraphicsResolution;
import org.videolan.bdjo.PlayListTable;

public class BDJLoader {
    public static boolean load(int title) {
        return load(title, true, null);
    }

    public static boolean load(int title, boolean restart, BDJLoaderCallback callback) {
        try {
            BDLocator locator = new BDLocator(null, title, -1);
            return load((TitleImpl)(SIManager.createInstance().getService(locator)), restart, callback);
        } catch (Throwable e) {
            e.printStackTrace();
            return false;
        }
    }

    public static boolean load(Locator locator, boolean restart, BDJLoaderCallback callback) {
        try {
            return load((TitleImpl)(SIManager.createInstance().getService(locator)), restart, callback);
        } catch (Throwable e) {
            e.printStackTrace();
            return false;
        }
    }

    public static boolean load(TitleImpl title, boolean restart, BDJLoaderCallback callback) {
        if (title == null)
            return false;
        synchronized (BDJLoader.class) {
            if (queue == null)
                queue = new BDJActionQueue();
        }
        queue.put(new BDJLoaderAction(title, restart, callback));
        return true;
    }

    public static boolean unload() {
        return unload(null);
    }

    public static boolean unload(BDJLoaderCallback callback) {
        synchronized (BDJLoader.class) {
            if (queue == null)
                queue = new BDJActionQueue();
        }
        queue.put(new BDJLoaderAction(null, false, callback));
        return true;
    }

    public static void shutdown() {
        unload();
        try {
            queue.finalize();
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }

    private static boolean loadN(TitleImpl title, boolean restart) {
        TitleInfo ti = title.getTitleInfo();
        if (!ti.isBdj()) {
            logger.log(Level.INFO, "Not BD-J title - requesting HDMV title start");
            unloadN();
            return Libbluray.selectTitle(title);
        }

        try {
            // load bdjo
            Bdjo bdjo = Libbluray.getBdjo(ti.getBdjoName());
            if (bdjo == null)
                throw new InvalidObjectException("bdjo not loaded");
            AppEntry[] appTable = bdjo.getAppTable();

            // reuse appProxys
            BDJAppProxy[] proxys = new BDJAppProxy[appTable.length];
            AppsDatabase db = AppsDatabase.getAppsDatabase();
            Enumeration ids = db.getAppIDs(new CurrentServiceFilter());
            while (ids.hasMoreElements()) {
                AppID id = (AppID)ids.nextElement();
                BDJAppProxy proxy = (BDJAppProxy)db.getAppProxy(id);
                AppEntry entry = (AppEntry)db.getAppAttributes(id);
                for (int i = 0; i < appTable.length; i++) {
                    if (id.equals(appTable[i].getIdentifier()) &&
                        entry.getInitialClass().equals(appTable[i].getInitialClass())) {
                        if (restart && appTable[i].getIsServiceBound())
                            proxy.stop(true);
                        proxy.getXletContext().update(appTable[i], bdjo.getAppCaches());
                        proxys[i] = proxy;
                        proxy = null;
                        break;
                    }
                }
                if (proxy != null)
                    proxy.release();
            }

            // start bdj window
            GUIManager gui = GUIManager.getInstance();
            GraphicsResolution res = bdjo.getTerminalInfo().getResolution();
            gui.setSize(res.getWidth(), res.getHeight());
            gui.setVisible(true);

            // initialize appProxys
            for (int i = 0; i < appTable.length; i++) {
                if (proxys[i] == null) {
                    proxys[i] = new BDJAppProxy(
                                                new BDJXletContext(
                                                                   appTable[i],
                                                                   bdjo.getAppCaches(),
                                                                   gui));
                }
                logger.log(Level.INFO, "Loaded class: " + appTable[i].getInitialClass());
            }

            // change psr
            Libbluray.writePSR(Libbluray.PSR_TITLE_NUMBER, title.getTitleNum());

            // notify AppsDatabase
            ((BDJAppsDatabase)BDJAppsDatabase.getAppsDatabase()).newDatabase(bdjo, proxys);

            // now run all the xlets
            for (int i = 0; i < appTable.length; i++) {
                int code = appTable[i].getControlCode();
                if (code == AppEntry.AUTOSTART)
                    proxys[i].start();
                else if (code == AppEntry.PRESENT)
                    proxys[i].resume();
                else {
                    proxys[i].pause();
                }
            }

            logger.log(Level.INFO, "Finished initializing and starting xlets.");

            // auto start playlist
            PlayListTable plt = bdjo.getAccessiblePlaylists();
            if ((plt != null) && (plt.isAutostartFirst())) {
                logger.log(Level.INFO, "Auto-starting playlist");
                String[] pl = plt.getPlayLists();
                if (pl.length > 0)
                    Manager.createPlayer(new MediaLocator(new BDLocator("bd://PLAYLIST:" + pl[0]))).start();
            }

            return true;

        } catch (Throwable e) {
            e.printStackTrace();
            unloadN();
            return false;
        }
    }

    private static boolean unloadN() {
        try {
            AppsDatabase db = AppsDatabase.getAppsDatabase();
            Enumeration ids = db.getAppIDs(new CurrentServiceFilter());
            while (ids.hasMoreElements()) {
                AppID id = (AppID)ids.nextElement();
                BDJAppProxy proxy = (BDJAppProxy)db.getAppProxy(id);
                proxy.release();
            }
            ((BDJAppsDatabase)db).newDatabase(null, null);
            return true;
        } catch (Throwable e) {
            e.printStackTrace();
            return false;
        }
    }

    private static class BDJLoaderAction extends BDJAction {
        public BDJLoaderAction(TitleImpl title, boolean restart, BDJLoaderCallback callback) {
            this.title = title;
            this.restart = restart;
            this.callback = callback;
        }

        protected void doAction() {
            boolean succeed;
            if (title != null)
                succeed = loadN(title, restart);
            else
                succeed = unloadN();
            if (callback != null)
                callback.loaderDone(succeed);
        }

        private TitleImpl title;
        private boolean restart;
        private BDJLoaderCallback callback;
    }

    private static final Logger logger = Logger.getLogger(BDJLoader.class.getName());

    private static BDJActionQueue queue = null;
}
