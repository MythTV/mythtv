/*
 * This file is part of libbluray
 * Copyright (C) 2010  VideoLAN
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

package org.videolan.bdjo;

import org.videolan.Arrays;

public class Bdjo {
    public Bdjo(TerminalInfo terminalInfo, AppCache[] appCaches,
            PlayListTable accessiblePlaylists, AppEntry[] appTable,
            int keyInterestTable, String fileAccessInfo)
    {
        this.terminalInfo = terminalInfo;
        this.appCaches = appCaches;
        this.accessiblePlaylists = accessiblePlaylists;
        this.appTable = appTable;
        this.keyInterestTable = keyInterestTable;
        this.fileAccessInfo = fileAccessInfo;
    }

    public TerminalInfo getTerminalInfo()
    {
        return terminalInfo;
    }

    public AppCache[] getAppCaches()
    {
        return appCaches;
    }

    public PlayListTable getAccessiblePlaylists()
    {
        return accessiblePlaylists;
    }

    public AppEntry[] getAppTable()
    {
        return appTable;
    }

    public int getKeyInterestTable()
    {
        return keyInterestTable;
    }

    public String getFileAccessInfo()
    {
        return fileAccessInfo;
    }

    public String toString()
    {
        return "Bdjo [accessiblePlaylists=" + accessiblePlaylists
                + ", appCaches=" + Arrays.toString(appCaches) + ", appTable="
                + Arrays.toString(appTable) + ", fileAccessInfo="
                + fileAccessInfo + ", keyInterestTable=" + keyInterestTable
                + ", terminalInfo=" + terminalInfo + "]";
    }

    private TerminalInfo terminalInfo;
    private AppCache[] appCaches;
    private PlayListTable accessiblePlaylists;
    private AppEntry[] appTable;
    private int keyInterestTable;
    private String fileAccessInfo;
}
