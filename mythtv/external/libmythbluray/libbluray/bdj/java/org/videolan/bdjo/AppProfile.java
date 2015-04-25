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

public class AppProfile {
    public AppProfile(short profile, byte major, byte minor, byte micro)
    {
        this.profile = profile;
        this.major = major;
        this.minor = minor;
        this.micro = micro;
    }

    public short getProfile()
    {
        return profile;
    }

    public byte getMajor()
    {
        return major;
    }

    public byte getMinor()
    {
        return minor;
    }

    public byte getMicro()
    {
        return micro;
    }

    public String toString()
    {
        return "AppProfile [major=" + major + ", micro=" + micro + ", minor="
                + minor + ", profile=" + profile + "]";
    }

    private short profile;
    private byte major;
    private byte minor;
    private byte micro;
}
