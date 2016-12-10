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
            boolean readOrganization, boolean writeOrganization,
            boolean readApplication, boolean writeApplication) {
        this.readWorld = readWorld;
        this.writeWorld = writeWorld;
        this.readOrganization = readOrganization;
        this.writeOrganization = writeOrganization;
        this.readApplication = readApplication;
        this.writeApplication = writeApplication;
    }

    public boolean hasReadWorldAccessRight() {
        return readWorld;
    }

    public boolean hasWriteWorldAccessRight() {
        return writeWorld;
    }

    public boolean hasReadOrganizationAccessRight() {
        return readOrganization;
    }

    public boolean hasWriteOrganizationAccessRight() {
        return writeOrganization;
    }

    public boolean hasReadApplicationAccessRight() {
        return readApplication;
    }

    public boolean hasWriteApplicationAccessRight() {
        return writeApplication;
    }

    public void setPermissions(boolean ReadWorld, boolean WriteWorld,
            boolean ReadOrganization, boolean WriteOrganization,
            boolean ReadApplication, boolean WriteApplication) {
        this.readWorld = ReadWorld;
        this.writeWorld = WriteWorld;
        this.readOrganization = ReadOrganization;
        this.writeOrganization = WriteOrganization;
        this.readApplication = ReadApplication;
        this.writeApplication = WriteApplication;
    }

    public String toString() {
        return this.getClass().getName() +
            "[rApp=" + readApplication +
            ",wApp=" + writeApplication +
            ",rOrg=" + readOrganization +
            ",wOrg=" + writeOrganization +
            ",rWorld=" + readWorld +
            ",wWorld=" + writeWorld + "]";
    }

    private boolean readWorld;
    private boolean writeWorld;
    private boolean readOrganization;
    private boolean writeOrganization;
    private boolean readApplication;
    private boolean writeApplication;
}
