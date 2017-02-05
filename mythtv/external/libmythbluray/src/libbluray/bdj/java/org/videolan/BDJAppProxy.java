/*
 * This file is part of libbluray
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

import org.dvb.application.AppID;
import org.dvb.application.AppStateChangeEvent;
import org.dvb.application.AppStateChangeEventListener;
import org.dvb.application.DVBJProxy;

import java.awt.EventQueue;

import java.io.File;
import java.util.LinkedList;
import javax.tv.xlet.Xlet;

class BDJAppProxy implements DVBJProxy, Runnable {
    protected static BDJAppProxy newInstance(BDJXletContext context) {
        BDJAppProxy proxy = new BDJAppProxy(context);
        /* do not create and start thread in constructor.
           if constructor fails (exception), thread is left running without BDJAppProxy ... */
        proxy.startThread();
        return proxy;
    }

    private void startThread() {
        thread = new Thread(context.getThreadGroup(), this, "BDJAppProxy");
        thread.setDaemon(true);
        thread.start();

        /* wait until thread has been started and event queue is initialized.
         * We want event dispatcher thread to be inside xlet thread group
         * -> event queue must be created from thread running inside applet thread group.
         */
        while (context.getEventQueue() == null) {
            Thread.yield();
        }
    }

    private BDJAppProxy(BDJXletContext context) {
        this.context = context;
        state = NOT_LOADED;
    }

    public int getState() {
        return state;
    }

    public void load() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_LOAD, null);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    public void init() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_INIT, null);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    public void start() {
        start(null);
    }

    public void start(String[] args) {
        AppCommand cmd = new AppCommand(AppCommand.CMD_START, args);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    public void stop(boolean force, int timeout) {
        AppCommand cmd = new AppCommand(AppCommand.CMD_STOP, new Boolean(force));
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
        if (timeout > 0) {
            if (!cmd.waitDone(timeout)) {
                logger.error("stop() timeout: Xlet " + context.getThreadGroup().getName());
            }
        }
    }

    public void stop(boolean force) {
        stop(force, -1);
    }

    public void pause() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_PAUSE, null);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    public void resume() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_RESUME, null);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    protected void notifyDestroyed() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_NOTIFY_DESTROYED, null);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    protected void notifyPaused() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_NOTIFY_PAUSED, null);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    protected void release() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_STOP, new Boolean(true));
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.addLast(null);
            cmds.notifyAll();
        }

        if (!cmd.waitDone(5000)) {
            logger.error("release(): STOP timeout, killing Xlet " + context.getThreadGroup().getName());
        }

        final String persistentOrg = System.getProperty("dvb.persistent.root") + File.separator +
            (String)context.getXletProperty("dvb.org.id") + File.separator;
        final String persistentApp = persistentOrg + (String)context.getXletProperty("dvb.app.id");

        context.release();

        if (new File(persistentApp).delete()) {
            new File(persistentOrg).delete();
        }
    }

    public void addAppStateChangeEventListener(AppStateChangeEventListener listener) {
        synchronized(listeners) {
            listeners.add(listener);
        }
    }

    public void removeAppStateChangeEventListener(AppStateChangeEventListener listener) {
        synchronized(listeners) {
            listeners.remove(listener);
        }
    }

    private void notifyListeners(int fromState, int toState, boolean hasFailed) {
        LinkedList list;
        synchronized(listeners) {
            list = (LinkedList)listeners.clone();
        }

        AppStateChangeEvent event = new AppStateChangeEvent(
                (AppID)context.getXletProperty("org.dvb.application.appid"),
                fromState, toState, this, hasFailed);
        for (int i = 0; i < list.size(); i++)
            ((AppStateChangeEventListener)list.get(i)).stateChange(event);
    }

    protected BDJXletContext getXletContext() {
        return context;
    }

    private boolean doLoad() {
        if (state == NOT_LOADED) {
            try {
                xlet = ((BDJClassLoader)context.getClassLoader()).loadXlet();
                state = LOADED;
                return true;
            } catch (Throwable e) {
                logger.error("doLoad() failed: " + e + "\n" + Logger.dumpStack(e));
                state = INVALID;
            }
        }
        return false;
    }

    private boolean doInit() {
        if ((state == NOT_LOADED) && !doLoad())
            return false;
        if (state == LOADED) {
            try {
                String persistent = System.getProperty("dvb.persistent.root") + File.separator +
                    (String)context.getXletProperty("dvb.org.id") + File.separator +
                    (String)context.getXletProperty("dvb.app.id");
                new File(persistent).mkdirs();
                xlet.initXlet(context);
                state = PAUSED;
                return true;
            } catch (Throwable e) {
                logger.error("doInit() failed: " + e + "\n" + Logger.dumpStack(e));
                state = INVALID;
            }
        }
        return false;
    }

    private boolean doStart(String[] args) {
        if (((state == NOT_LOADED) || (state == LOADED)) && !doInit())
            return false;
        if (state == PAUSED) {
            try {
                if (args != null)
                    context.setArgs(args);
                xlet.startXlet();
                state = STARTED;
                return true;
            } catch (Throwable e) {
                logger.error("doStart() failed: " + e + "\n" + Logger.dumpStack(e));
                state = INVALID;
            }
        }
        return false;
    }

    private boolean doStop(boolean force) {
        if (state == INVALID)
            return false;
        if ((state != NOT_LOADED) && (state != LOADED)) {
            try {
                xlet.destroyXlet(force);

                context.closeSockets();
                context.getThreadGroup().waitForShutdown(1000, 1 + context.numEventQueueThreads());

                context.exitXlet();

            } catch (Throwable e) {
                logger.error("doStop() failed: " + e + "\n" + Logger.dumpStack(e));
                state = INVALID;
                return false;
            }
        }
        xlet = null;
        state = DESTROYED;
        return true;
    }

    private boolean doPause() {
        if (state == STARTED) {
            try {
                xlet.pauseXlet();
                state = PAUSED;
                return true;
            } catch (Throwable e) {
                logger.error("doPause() failed: " + e + "\n" + Logger.dumpStack(e));
                state = INVALID;
            }
        }
        return false;
    }

    private boolean doResume() {
        if (state == PAUSED) {
            try {
                xlet.startXlet();
                state = STARTED;
                return true;
            } catch (Throwable e) {
                logger.error("doResume() failed: " + e + "\n" + Logger.dumpStack(e));
                state = INVALID;
            }
        }
        return false;
    }

    public void run() {
        if (context.getEventQueue() == null)
            context.setEventQueue(new EventQueue());

        for (;;) {
            AppCommand cmd;
            synchronized(cmds) {
                while (cmds.isEmpty()) {
                    try {
                        cmds.wait();
                    } catch (InterruptedException e) {

                    }
                }
                cmd = (AppCommand)cmds.removeFirst();
            }
            if (cmd == null)
                return;
            int fromState = state;
            int toState;
            boolean ret;
            switch (cmd.getCommand()) {
            case AppCommand.CMD_LOAD:
                toState = LOADED;
                ret = doLoad();
                break;
            case AppCommand.CMD_INIT:
                toState = PAUSED;
                ret = doInit();
                break;
            case AppCommand.CMD_START:
                toState = STARTED;
                Object args = cmd.getArgument();
                ret = doStart(args == null ? null : (String[])args);
                break;
            case AppCommand.CMD_STOP:
                toState = DESTROYED;
                ret = doStop(((Boolean)cmd.getArgument()).booleanValue());
                break;
            case AppCommand.CMD_PAUSE:
                toState = PAUSED;
                ret = doPause();
                break;
            case AppCommand.CMD_RESUME:
                toState = STARTED;
                ret = doResume();
                break;
            case AppCommand.CMD_NOTIFY_DESTROYED:
                toState = DESTROYED;
                state = DESTROYED;
                ret = true;
                break;
            case AppCommand.CMD_NOTIFY_PAUSED:
                toState = PAUSED;
                state = PAUSED;
                ret = true;
                break;
            default:
                return;
            }
            notifyListeners(fromState, toState, !ret);
            cmd.release();
            if (state == DESTROYED)
                state = NOT_LOADED;
        }
    }

    private BDJXletContext context;
    private Xlet xlet;
    private int state;
    private LinkedList listeners = new LinkedList();
    private LinkedList cmds = new LinkedList();
    private Thread thread;
    private static final Logger logger = Logger.getLogger(BDJAppProxy.class.getName());

    private class AppCommand {
        public AppCommand(int cmd, Object arg) {
            this.cmd = cmd;
            this.arg = arg;
        }

        public int getCommand() {
            return cmd;
        }

        public Object getArgument() {
            return arg;
        }

        public boolean waitDone(int timeoutMs) {
            synchronized(this) {
                while (!done) {
                    try {
                        if (timeoutMs < 1) {
                            this.wait();
                        } else {
                            this.wait(timeoutMs);
                            break;
                        }
                    } catch (InterruptedException e) {
                    }
                }
                return done;
            }
        }

        public void release() {
            synchronized(this) {
                done = true;
                this.notifyAll();
            }
        }

        public static final int CMD_LOAD = 0;
        public static final int CMD_INIT = 1;
        public static final int CMD_START = 2;
        public static final int CMD_STOP = 3;
        public static final int CMD_PAUSE = 4;
        public static final int CMD_RESUME = 5;
        public static final int CMD_NOTIFY_DESTROYED = 6;
        public static final int CMD_NOTIFY_PAUSED = 7;

        private int cmd;
        private Object arg;
        private boolean done = false;
    }
}
