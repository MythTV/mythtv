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

package org.dvb.io.persistent;

import java.io.File;
import java.io.IOException;
import java.util.Date;

public class FileAttributes {

    public static final int PRIORITY_LOW = 1;
    public static final int PRIORITY_MEDIUM = 2;
    public static final int PRIORITY_HIGH = 3;

    protected FileAttributes(Date expiration_date,
            FileAccessPermissions permissions, int priority)
    {
        this.permissions = permissions;
        this.priority = priority;
    }

    public Date getExpirationDate()
    {
        return null;
    }

    public void setExpirationDate(Date d)
    {
        // expiration dates are for losers
    }

    public FileAccessPermissions getPermissions()
    {
        return permissions;
    }

    public void setPermissions(FileAccessPermissions permissions)
    {
        this.permissions = permissions;
    }

    public int getPriority()
    {
        return priority;
    }

    public void setPriority(int priority)
    {
        this.priority = priority;
    }

    public static void setFileAttributes(FileAttributes p, File f)
            throws IOException
    {
        // not implemented
    }

    public static FileAttributes getFileAttributes(File f) throws IOException
    {
        boolean r = f.canRead();
        boolean w = f.canWrite();
        
        FileAccessPermissions permissions = new FileAccessPermissions(r, w, r, w, r, w);
        
        return new FileAttributes(null, permissions, PRIORITY_LOW);
    }

    private FileAccessPermissions permissions;
    private int priority;
}
