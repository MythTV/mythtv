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

package javax.tv.service;

public class SIRequestFailureType {
    protected SIRequestFailureType(String name)
    {
        this.name = name;
    }

    public String toString()
    {
        return name;
    }

    public static final SIRequestFailureType CANCELED = new SIRequestFailureType(
            "CANCELED");
    public static final SIRequestFailureType INSUFFICIENT_RESOURCES = new SIRequestFailureType(
            "INSUFFICIENT_RESOURCES");
    public static final SIRequestFailureType DATA_UNAVAILABLE = new SIRequestFailureType(
            "DATA_UNAVAILABLE");
    public static final SIRequestFailureType UNKNOWN = new SIRequestFailureType(
            "UNKNOWN");
    private String name;
}
