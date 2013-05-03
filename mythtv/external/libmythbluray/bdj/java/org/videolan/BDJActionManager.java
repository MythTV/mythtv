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
    public static BDJActionManager getInstance() {
        synchronized (BDJActionManager.class) {
            if (instance == null)
                instance = new BDJActionManager();
        }
        return instance;
    }

    public BDJActionManager() {
        commandQueue = new BDJActionQueue();
        callbackQueue = new BDJActionQueue();
    }

    protected void finalize() throws Throwable {
        commandQueue.finalize();
        callbackQueue.finalize();
        synchronized (BDJActionManager.class) {
            instance = null;
        }
        super.finalize();
    }

    public void putCommand(BDJAction action) {
        commandQueue.put(action);
    }

    public void putCallback(BDJAction action) {
        callbackQueue.put(action);
    }

    private BDJActionQueue commandQueue;
    private BDJActionQueue callbackQueue;

    private static BDJActionManager instance = null;
}
