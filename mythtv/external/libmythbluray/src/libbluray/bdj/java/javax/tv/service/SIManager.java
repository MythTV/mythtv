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

import javax.tv.locator.*;
import javax.tv.service.navigation.*;

import javax.tv.service.transport.Transport;

public abstract class SIManager {
    protected SIManager() {
    }

    public static SIManager createInstance() {
        return SIManagerImpl.createInstance();
    }

    public abstract void setPreferredLanguage(String language);

    public abstract String getPreferredLanguage();

    public abstract void registerInterest(Locator locator, boolean active)
            throws InvalidLocatorException, SecurityException;

    public abstract String[] getSupportedDimensions();

    public abstract RatingDimension getRatingDimension(String name)
            throws SIException;

    public abstract Transport[] getTransports();

    public abstract SIRequest retrieveSIElement(Locator locator,
            SIRequestor requestor) throws InvalidLocatorException,
            SecurityException;

    public abstract Service getService(Locator locator)
            throws InvalidLocatorException, SecurityException;

    public abstract SIRequest retrieveServiceDetails(Locator locator,
            SIRequestor requestor) throws InvalidLocatorException,
            SecurityException;

    public abstract SIRequest retrieveProgramEvent(Locator locator,
            SIRequestor requestor) throws InvalidLocatorException,
            SecurityException;

    public abstract ServiceList filterServices(ServiceFilter filter);
}
