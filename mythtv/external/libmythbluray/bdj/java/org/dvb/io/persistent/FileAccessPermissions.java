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

public class FileAccessPermissions {
    public FileAccessPermissions(boolean readWorld, boolean writeWorld,
            boolean readOrganisation, boolean writeOrganisation,
            boolean readApplication, boolean writeApplication)
    {
        this.readWorld = readWorld;
        this.writeWorld = writeWorld;
        this.readOrganisation = readOrganisation;
        this.writeOrganisation = writeOrganisation;
        this.readApplication = readApplication;
        this.writeApplication = writeApplication;
    }

    public boolean hasReadWorldAccessRight()
    {
        return readWorld;
    }

    public boolean hasWriteWorldAccessRight()
    {
        return writeWorld;
    }

    public boolean hasReadOrganisationAccessRight()
    {
        return readOrganisation;
    }

    public boolean hasWriteOrganisationAccessRight()
    {
        return writeOrganisation;
    }

    public boolean hasReadApplicationAccessRight()
    {
        return readApplication;
    }

    public boolean hasWriteApplicationAccessRight()
    {
        return writeApplication;
    }

    public void setPermissions(boolean ReadWorld, boolean WriteWorld,
            boolean ReadOrganisation, boolean WriteOrganisation,
            boolean ReadApplication, boolean WriteApplication)
    {
        this.readWorld = ReadWorld;
        this.writeWorld = WriteWorld;
        this.readOrganisation = ReadOrganisation;
        this.writeOrganisation = WriteOrganisation;
        this.readApplication = ReadApplication;
        this.writeApplication = WriteApplication;
    }

    private boolean readWorld;
    private boolean writeWorld;
    private boolean readOrganisation;
    private boolean writeOrganisation;
    private boolean readApplication;
    private boolean writeApplication;
}
