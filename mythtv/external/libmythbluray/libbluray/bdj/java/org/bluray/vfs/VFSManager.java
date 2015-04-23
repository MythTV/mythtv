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

package org.bluray.vfs;

public class VFSManager {
    public static VFSManager getInstance() throws SecurityException,
            UnsupportedOperationException
    {
        throw new Error("Not implemented");
    }

    protected VFSManager()
    {

    }

    public boolean disableClip(String streamfile)
    {
        throw new Error("Not implemented");
    }

    public boolean enableClip(String streamfile)
    {
        throw new Error("Not implemented");
    }

    public String[] getDisabledClipIDs()
    {
        throw new Error("Not implemented");
    }

    public int getState()
    {
        throw new Error("Not implemented");
    }

    public boolean isEnabledClip(String clipID)
    {
        throw new Error("Not implemented");
    }

    public void requestUpdating(String manifestfile, String signaturefile,
            boolean initBackupRegs) throws PreparingFailedException
    {
        throw new Error("Not implemented");
    }

    public static final int STABLE = 1;
    public static final int PREPARING = 2;
    public static final int PREPARED = 3;
    public static final int UPDATING = 4;
}
