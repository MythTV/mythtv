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

public class BDJActionManager {
    protected static void createInstance() {
        if (running) {
            System.err.println("BDJActionManager: manager already running! " + Logger.dumpStack());
            return;
        }
        instance = new BDJActionManager();
    }

    public static BDJActionManager getInstance() {
        return instance;
    }

    public BDJActionManager() {
        commandQueue = new BDJActionQueue("BDJActionManager");
    }

    protected static void shutdown() {
        try {
            instance.commandQueue.shutdown();
        } catch (Exception t) {
        } finally {
            running = false;
        }
    }

    public void putCommand(BDJAction action) {
        commandQueue.put(action);
    }

    private BDJActionQueue commandQueue;

    private static BDJActionManager instance = null;
    private static boolean running = false;
}
