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

package org.dvb.dsmcc;

public class ServiceXFRException extends DSMCCException {
    public ServiceXFRException(org.davic.net.Locator aService, int carouselId,
            String pathName)
    {
        this.ref = new ServiceXFRReference(aService, carouselId, pathName);
    }

    public ServiceXFRException(byte[] NSAPAddress, String pathName)
    {
        this.ref = new ServiceXFRReference(NSAPAddress, pathName);
    }

    public ServiceXFRReference getServiceXFR()
    {
        return ref;
    }

    private ServiceXFRReference ref;
    private static final long serialVersionUID = 8552009237150558808L;
}
