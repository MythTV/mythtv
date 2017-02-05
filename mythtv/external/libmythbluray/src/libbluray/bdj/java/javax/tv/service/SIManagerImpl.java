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

import org.bluray.net.BDLocator;
import org.bluray.ti.TitleImpl;
import org.videolan.Libbluray;

public class SIManagerImpl extends SIManager {

    private static final Object instanceLock = new Object();

    public static SIManager createInstance() {
        synchronized (instanceLock) {
            if (instance == null)
                instance = new SIManagerImpl();
            return instance;
        }
    }

    public static void shutdown() {
        synchronized (instanceLock) {
            instance = null;
        }
    }

    protected SIManagerImpl() {
        int ntitles = Libbluray.numTitles();
        LinkedList list = new LinkedList();
        for (int i = 0; i <= ntitles; i++) {
            try {
                list.add(new TitleImpl(i));
            } catch (Exception t) {
                org.videolan.Logger.getLogger("SIManagerImpl").error("Failed initializing title " + i + ": " + t);
            }
        }

        try {
            list.add(new TitleImpl(65535));
        } catch (Exception t) {
            org.videolan.Logger.getLogger("SIManagerImpl").error("Failed initializing title FirstPlay: " + t);
        }

        titles = new ServiceListImpl(list);
    }

    public ServiceList filterServices(ServiceFilter filter) {
        return titles.filterServices(filter);
    }

    public String getPreferredLanguage() {
        return language;
    }

    public RatingDimension getRatingDimension(String name) throws SIException {
        if (!name.equals(RatingDimensionImpl.dimensionName))
            throw new SIException();
        return new RatingDimensionImpl();
    }

    public Service getService(Locator locator) throws InvalidLocatorException, SecurityException {

        BDLocator bdLocator = null;
        try {
            bdLocator = new BDLocator(locator.toExternalForm());
        } catch (org.davic.net.InvalidLocatorException e) {
            System.err.println("invalid locator: " + locator.toExternalForm() + "\n" + org.videolan.Logger.dumpStack(e));
            throw new javax.tv.locator.InvalidLocatorException(locator);
        }

        return titles.findService(locator);
    }

    public String[] getSupportedDimensions() {
         String[] dimensions = new String[1];
         dimensions[0] = RatingDimensionImpl.dimensionName;
         return dimensions;
    }

    public Transport[] getTransports() {
        Transport[] transports = new Transport[1];
        transports[0] = new TransportImpl();
        return transports;
    }

    public void registerInterest(Locator locator, boolean active)
            throws InvalidLocatorException, SecurityException {
        org.videolan.Logger.unimplemented(SIManagerImpl.class.getName(), "registerInterest");
    }

    public SIRequest retrieveProgramEvent(Locator locator, SIRequestor requestor)
            throws InvalidLocatorException, SecurityException {
        org.videolan.Logger.unimplemented(SIManagerImpl.class.getName(), "retrieveProgramEvent");
        throw new Error("Not implemented");
    }

    public SIRequest retrieveSIElement(Locator locator, SIRequestor requestor)
            throws InvalidLocatorException, SecurityException {
        org.videolan.Logger.unimplemented(SIManagerImpl.class.getName(), "retrieveSIElement");
        throw new Error("Not implemented");
    }

    public SIRequest retrieveServiceDetails(Locator locator, SIRequestor requestor)
                throws InvalidLocatorException, SecurityException {
        org.videolan.Logger.unimplemented(SIManagerImpl.class.getName(), "retrieveServiceDetails");
        throw new Error("Not implemented");
    }

    public void setPreferredLanguage(String language) {
        this.language = language;
    }

    private ServiceListImpl titles;
    private String language = null;

    private static SIManagerImpl instance = null;
}
