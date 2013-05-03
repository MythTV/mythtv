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

public class ServiceType
{
    protected ServiceType(String name) { 
        this.name = name;
    }

    public String toString() {
        return name;
    }
    
    public static final ServiceType DIGITAL_TV = new ServiceType("DIGITAL_TV");
    public static final ServiceType DIGITAL_RADIO = new ServiceType("DIGITAL_RADIO");
    public static final ServiceType NVOD_REFERENCE = new ServiceType("NVOD_REFERENCE");
    public static final ServiceType NVOD_TIME_SHIFTED = new ServiceType("NVOD_TIME_SHIFTED");
    public static final ServiceType ANALOG_TV = new ServiceType("ANALOG_TV");
    public static final ServiceType ANALOG_RADIO = new ServiceType("ANALOG_RADIO");
    public static final ServiceType DATA_BROADCAST = new ServiceType("DATA_BROADCAST");
    public static final ServiceType DATA_APPLICATION = new ServiceType("DATA_APPLICATION");
    public static final ServiceType UNKNOWN = new ServiceType("UNKNOWN");
    
    private String name;
}
