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

import java.util.LinkedList;

public class BDJActionQueue implements Runnable {
    public BDJActionQueue(String name) {
        this(null, name);
    }

    public BDJActionQueue(BDJThreadGroup threadGroup, String name) {
        if (threadGroup == null) {
            if (BDJXletContext.getCurrentContext() != null) {
                logger.error("BDJActionQueue created from wrong context: " + Logger.dumpStack());
            }
        }

        /* run all actions in given thread group / xlet context */
        thread = new Thread(threadGroup, this, name + ".BDJActionQueue");
        thread.setDaemon(true);
        thread.start();

        watchdog = new Watchdog(name);
    }

    public void shutdown() {

        synchronized (actions) {
            terminated = true;
            actions.addLast(null);
            actions.notifyAll();
        }
        watchdog.shutdown();
        try {
            thread.join();
        } catch (InterruptedException t) {
            logger.error("Error joining thread: " + t);
        }
    }

    public void run() {
        while (true) {
            Object action;
            synchronized (actions) {
                while (actions.isEmpty()) {
                    try {
                        actions.wait();
                    } catch (InterruptedException e) {
                    }
                }
                action = actions.removeFirst();
            }
            if (action == null)
                return;
            try {
                watchdog.startAction(action);

                ((BDJAction)action).process();

                watchdog.endAction();
            } catch (ThreadDeath d) {
                System.err.println("action failed: " + d + "\n");
                throw d;
            } catch (Throwable e) {
                System.err.println("action failed: " + e + "\n" + Logger.dumpStack(e));
            }
        }
    }

    public void put(BDJAction action) {
        if (action != null) {
            synchronized (actions) {
                if (!terminated) {
                    actions.addLast(action);
                    actions.notifyAll();
                } else {
                    logger.error("Action skipped (queue stopped): " + action);
                    action.abort();
                }
            }
        }
    }

    private boolean terminated = false;
    private Thread thread;
    private LinkedList actions = new LinkedList();

    private static final Logger logger = Logger.getLogger(BDJActionQueue.class.getName());

    /*
     *
     */

    private Watchdog watchdog = null;

    class Watchdog implements Runnable {

        private Object currentAction = null;
        private boolean terminate = false;

        Watchdog(String name) {
            Thread t = new Thread(null, this, name + ".BDJActionQueue.Monitor");
            t.setDaemon(true);
            t.start();
        }

        public synchronized void shutdown() {
            terminate = true;
            notifyAll();
        }

        public synchronized void startAction(Object action) {
            currentAction = action;
            notifyAll();
        }

        public synchronized void endAction() {
            currentAction = null;
            notifyAll();
        }

        public void run() {
            Object loggedAction = null;
            synchronized (this) {
                while (!terminate) {

                    if (loggedAction != null && currentAction != loggedAction) {
                        loggedAction = null;
                        logger.info("Callback returned (" + thread + ")");
                    }

                    try {
                        if (currentAction == null) {
                            wait();
                        } else {
                            Object cachedAction = currentAction;
                            wait(5000);
                            if (currentAction == cachedAction && loggedAction != cachedAction) {
                                logger.error("Callback timeout in " + thread + ", callback=" + currentAction + "\n" +
                                             PortingHelper.dumpStack(thread));
                                loggedAction = cachedAction;
                            }
                        }
                    } catch (InterruptedException e) {
                    }
                }
            }
        }
    }
}
