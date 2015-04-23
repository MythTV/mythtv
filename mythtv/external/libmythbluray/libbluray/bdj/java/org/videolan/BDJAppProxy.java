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

import java.io.File;
import java.util.LinkedList;
import javax.tv.xlet.Xlet;

public class BDJAppProxy implements DVBJProxy, Runnable {
    public BDJAppProxy(BDJXletContext context) {
        this.context = context;
        state = NOT_LOADED;
        threadGroup = new BDJThreadGroup((String)context.getXletProperty("dvb.org.id") + "." +
                                         (String)context.getXletProperty("dvb.app.id"),
                                         context);
        thread = new Thread(threadGroup, this);
        thread.setDaemon(true);
        thread.start();
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
        AppCommand cmd = new AppCommand(AppCommand.CMD_START, null);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    public void start(String[] args) {
        AppCommand cmd = new AppCommand(AppCommand.CMD_START, args);
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
    }

    public void stop(boolean force) {
        AppCommand cmd = new AppCommand(AppCommand.CMD_STOP, new Boolean(force));
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
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

    protected void syncStop() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_STOP, new Boolean(true));
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.notifyAll();
        }
        cmd.waitDone();
    }

    protected void release() {
        AppCommand cmd = new AppCommand(AppCommand.CMD_STOP, new Boolean(true));
        synchronized(cmds) {
            cmds.addLast(cmd);
            cmds.addLast(null);
            cmds.notifyAll();
        }
        try {
            thread.join();
        } catch (InterruptedException e) {

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
            ((AppStateChangeEventListener)listeners.get(i)).stateChange(event);
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
                e.printStackTrace();
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
                e.printStackTrace();
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
                e.printStackTrace();
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
                for (int i = 0; (i < 50) && (threadGroup.activeCount() > 1); i++)
                    Thread.sleep(20L);
                String persistent = System.getProperty("dvb.persistent.root") + File.separator +
                    (String)context.getXletProperty("dvb.org.id") + File.separator +
                    (String)context.getXletProperty("dvb.app.id");
                if (new File(persistent).delete()) {
                    persistent = System.getProperty("dvb.persistent.root") + File.separator +
                        (String)context.getXletProperty("dvb.org.id");
                    new File(persistent).delete();
                }
            } catch (Throwable e) {
                e.printStackTrace();
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
                e.printStackTrace();
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
                e.printStackTrace();
                state = INVALID;
            }
        }
        return false;
    }

    public void run() {
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
    private BDJThreadGroup threadGroup;
    private Thread thread;

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

        public void waitDone() {
            synchronized(this) {
                while (!done) {
                    try {
                        this.wait();
                    } catch (InterruptedException e) {

                    }
                }
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
