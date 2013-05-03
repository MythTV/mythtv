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

public class TitleInfo {
    public TitleInfo(int title, int objType, int playbackType, String bdjoName, int hdmvOID) {
        this.title = title;
        this.objType = objType;
        this.playbackType = playbackType;
        this.bdjoName = bdjoName;
        this.hdmvOID = hdmvOID;
    }

    public int getTitleNum() {
        return title;
    }

    public int getObjType() {
        return objType;
    }

    public int getPlaybackType() {
        return playbackType;
    }

    public String getBdjoName() {
        return bdjoName;
    }

    public int getHdmvOID() {
        return hdmvOID;
    }

    public boolean isBdj() {
        return (objType == OBJ_TYPE_BDJ);
    }

    public boolean isHdmv() {
        return (objType == OBJ_TYPE_HDMV);
    }

    private int title;
    private int objType;
    private int playbackType;
    private String bdjoName;
    private int hdmvOID;

    public static final int OBJ_TYPE_HDMV = 1;
    public static final int OBJ_TYPE_BDJ = 2;
    public static final int HDMV_PLAYBACK_TYPE_MOVIE = 0;
    public static final int HDMV_PLAYBACK_TYPE_INTERACTIVE = 1;
    public static final int BDJ_PLAYBACK_TYPE_MOVIE = 2;
    public static final int BDJ_PLAYBACK_TYPE_INTERACTIVE = 3;
}
