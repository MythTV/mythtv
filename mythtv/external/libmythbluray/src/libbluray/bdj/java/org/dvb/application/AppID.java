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

package org.dvb.application;

import java.io.Serializable;

public class AppID implements Serializable {
    public AppID(int oid, int aid) {
        this.oid = oid;
        this.aid = aid;
    }

    public int getOID() {
        return oid;
    }

    public int getAID() {
        return aid;
    }

    public String toString() {
        return Long.toString(((long)oid << 16) + aid, 16);
    }

    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + aid;
        result = prime * result + oid;
        return result;
    }

    public boolean equals(Object obj) {
        if (!(obj instanceof AppID))
            return false;
        AppID other = (AppID) obj;
        if (aid != other.aid)
            return false;
        if (oid != other.oid)
            return false;
        return true;
    }

    private int oid;
    private int aid;
}
