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

public class PlayListTable {
    public PlayListTable(boolean accessToAll, boolean autostartFirst, String[] playLists)
    {
        this.accessToAll = accessToAll;
        this.autostartFirst = autostartFirst;
        this.playLists = playLists;
    }

    public boolean isAccessToAll()
    {
        return accessToAll;
    }

    public boolean isAutostartFirst()
    {
        return autostartFirst;
    }

    public String[] getPlayLists()
    {
        return playLists;
    }

    public String toString()
    {
        return "PlayListTable [accessToAll=" + accessToAll + ", playLists="
                + Arrays.toString(playLists) + "]";
    }

    private boolean accessToAll;
    private boolean autostartFirst;
    private String[] playLists;
}
