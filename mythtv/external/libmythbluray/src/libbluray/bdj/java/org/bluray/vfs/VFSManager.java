/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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

package org.bluray.vfs;

import org.videolan.BUMFAsset;
import org.videolan.BUMFParser;
import org.videolan.Logger;

public class VFSManager {

    private static VFSManager instance = null;
    private static final Object instanceLock = new Object();

    public static VFSManager getInstance() throws SecurityException,
            UnsupportedOperationException {

        SecurityManager sm = System.getSecurityManager();
        if (sm != null)
            sm.checkPermission(new VFSPermission("*"));

        synchronized (instanceLock) {
            if (instance == null) {
                instance = new VFSManager();
            }
            return instance;
        }
    }

    protected VFSManager() {
        state = STABLE;
    }

    public boolean disableClip(String streamfile) {
        logger.unimplemented("disableClip");
        return true;
    }

    public boolean enableClip(String streamfile) {
        logger.unimplemented("enableClip");
        return true;
    }

    public String[] getDisabledClipIDs() {
        logger.unimplemented("getDisabledClipIDs");
        return new String[]{};
    }

    public int getState() {
        return state;
    }

    public boolean isEnabledClip(String clipID) {
        logger.unimplemented("isEnabledClip");
        return true;
    }

    public void requestUpdating(String manifestfile, String signaturefile,
            boolean initBackupRegs) throws PreparingFailedException {
        state = PREPARING;

        BUMFAsset[] assets = BUMFParser.parse(manifestfile);
        if (assets == null) {
            logger.error("manifest parsing failed");
            state = STABLE;
            throw new PreparingFailedException();
        }

        logger.unimplemented("requestUpdating(" + manifestfile + ")");
        state = STABLE;
        throw new PreparingFailedException();
    }

    private int state;

    public static final int STABLE = 1;
    public static final int PREPARING = 2;
    public static final int PREPARED = 3;
    public static final int UPDATING = 4;

    private static final Logger logger = Logger.getLogger(VFSManager.class.getName());
}
