/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2013  Petri Hintukainen <phintuka@users.sourceforge.net>
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

class BDJThreadGroup extends ThreadGroup {

    protected BDJThreadGroup(String name, BDJXletContext context) {
        super(name);
        this.context = context;
    }

    public void uncaughtException(Thread t, Throwable e) {

        String stack = "";
        if (e != null) {
            StackTraceElement elems[] = e.getStackTrace();
            if (elems != null) {
                for (int i = 0; i < elems.length; i++)
                    stack += "\n\t" + elems[i].toString();
            }
        }

        if (e instanceof ThreadDeath) {
            logger.error("Thread " + t + " killed" /*+ stack*/);
        } else {
            logger.error("Unhandled exception in thread " + t + ": " + e + stack);
        }
    }

    protected BDJXletContext getContext() {
        return context;
    }

    protected boolean waitForShutdown(int timeout, int maxThreads) {

        if (parentOf(Thread.currentThread().getThreadGroup()) && maxThreads < 1) {
            logger.error("Current Thread is contained within ThreadGroup to be disposed.");
            throw new IllegalThreadStateException("Current Thread is contained within ThreadGroup to be disposed.");
        }

        long startTime = System.currentTimeMillis();
        long endTime = startTime + timeout;
        while ((activeCount() > maxThreads) &&
               (System.currentTimeMillis() < endTime)) {
            try {
                Thread.sleep(10);
            } catch (InterruptedException e) { }
        }

        boolean result = (activeCount() <= maxThreads);
        if (!result) {
            logger.error("waitForShutdown timeout (have " + activeCount() + " threads, expected " + maxThreads + ")");
        }
        return result;
    }

    protected void stopAll(int timeout) {

        interrupt();
        waitForShutdown(timeout, 0);

        if (activeCount() > 0) {
            logger.error("stopAll(): killing threads");
            dumpThreads();

            PortingHelper.stopThreadGroup(this);
            waitForShutdown(500, 0);
        }

        if (destroyed) return;

        try {
            destroy();
            destroyed = true;
        } catch (IllegalThreadStateException e) {
            logger.error("ThreadGroup destroy failed: " + e);
        }
    }

    public void dumpThreads() {
        logger.info("Active threads in " + this + ":");
        Thread[] threads = new Thread[activeCount() + 1];
        while (enumerate( threads, true ) == threads.length) {
            threads = new Thread[threads.length * 2];
        }
        for (int i = 0; i < threads.length; i++) {
            if (threads[i] == null)
                continue;
            logger.info("    " + threads[i]);
            /* no getState() in J2ME
            logger.info("   state " + threads[i].getState().toString());
            */
            logger.info("    at " + PortingHelper.dumpStack(threads[i]));
        }
    }

    private boolean destroyed = false;
    private final BDJXletContext context;
    private static final Logger logger = Logger.getLogger(BDJThreadGroup.class.getName());
}
