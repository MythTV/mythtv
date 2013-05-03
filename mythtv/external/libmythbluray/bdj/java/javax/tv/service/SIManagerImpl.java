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

import java.util.LinkedList;

import javax.tv.locator.InvalidLocatorException;
import javax.tv.locator.Locator;
import javax.tv.service.navigation.ServiceFilter;
import javax.tv.service.navigation.ServiceList;
import javax.tv.service.navigation.ServiceListImpl;
import javax.tv.service.transport.Transport;
import javax.tv.service.transport.TransportImpl;

import org.bluray.ti.TitleImpl;
import org.videolan.Libbluray;

public class SIManagerImpl extends SIManager {
    public static SIManager createInstance() {
        synchronized (SIManagerImpl.class) {
            if (instance == null)
                instance = new SIManagerImpl();
            return instance;
        }
    }

    protected SIManagerImpl() {
        int ntitles = Libbluray.getTitles();
        LinkedList list = new LinkedList();
        for (int i = 0; i <= ntitles; i++)
            list.add(new TitleImpl(i));
        list.add(new TitleImpl(65535));
        titles = new ServiceListImpl(list);
    }

    @Override
    public ServiceList filterServices(ServiceFilter filter) {
        return titles.filterServices(filter);
    }

    @Override
    public String getPreferredLanguage() {
        return language;
    }

    @Override
    public RatingDimension getRatingDimension(String name) throws SIException {
        if (!name.equals(RatingDimensionImpl.dimensionName))
            throw new SIException();
        return new RatingDimensionImpl();
    }

    @Override
    public Service getService(Locator locator) throws InvalidLocatorException, SecurityException {
        return titles.findService(locator);
    }

    @Override
    public String[] getSupportedDimensions() {
         String[] dimensions = new String[1];
         dimensions[0] = RatingDimensionImpl.dimensionName;
         return dimensions;
    }

    @Override
    public Transport[] getTransports() {
        Transport[] transports = new Transport[1];
        transports[0] = new TransportImpl();
        return transports;
    }

    @Override
    public void registerInterest(Locator locator, boolean active)
            throws InvalidLocatorException, SecurityException {
        throw new Error("Not implemented");
    }

    @Override
    public SIRequest retrieveProgramEvent(Locator locator, SIRequestor requestor)
            throws InvalidLocatorException, SecurityException {
        throw new Error("Not implemented");
    }

    @Override
    public SIRequest retrieveSIElement(Locator locator, SIRequestor requestor)
            throws InvalidLocatorException, SecurityException {
        throw new Error("Not implemented");
    }

    @Override
    public SIRequest retrieveServiceDetails(Locator locator, SIRequestor requestor)
                throws InvalidLocatorException, SecurityException {
        throw new Error("Not implemented");
    }

    @Override
    public void setPreferredLanguage(String language) {
        this.language = language;
    }

    private ServiceListImpl titles;
    private String language = null;

    private static SIManagerImpl instance = null;
}
