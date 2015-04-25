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

public abstract class BDJAction {
    public BDJAction() {
    }

    public int getState() {
        synchronized (this) {
            return state;
        }
    }

    public void waitBegin() {
        synchronized (this) {
            while (state == NOT_PROCESSED) {
                try {
                    this.wait();
                } catch (InterruptedException e) {
                }
            }
        }
    }

    public void waitEnd() {
        synchronized (this) {
            while (state != PROCESSED) {
                try {
                    this.wait();
                } catch (InterruptedException e) {
                }
            }
        }
    }

    public void process() {
        synchronized (this) {
            state = PROCESSING;
            this.notifyAll();
        }
        try {
            doAction();
        } catch (Exception e) {
            System.err.println("action failed: " + e + "\n" + Logger.dumpStack(e));
        }
        synchronized (this) {
            state = PROCESSED;
            this.notifyAll();
        }
    }

    public void abort() {
        synchronized (this) {
            state = PROCESSED;
            this.notifyAll();
        }
    }

    protected abstract void doAction();

    private int state = NOT_PROCESSED;

    public static final int NOT_PROCESSED = 0;
    public static final int PROCESSING = 1;
    public static final int PROCESSED = 2;
}
