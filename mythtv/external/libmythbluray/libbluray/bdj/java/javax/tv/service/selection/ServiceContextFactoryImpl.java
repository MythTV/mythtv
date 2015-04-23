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

package javax.tv.service.selection;

import javax.tv.xlet.XletContext;

import org.bluray.ti.selection.TitleContextImpl;

public class ServiceContextFactoryImpl extends ServiceContextFactory {
    protected ServiceContextFactoryImpl() {
        serviceContexts = new ServiceContext[1];
        serviceContexts[0] = new TitleContextImpl();
    }

    public static ServiceContextFactory getInstance() {
        synchronized (ServiceContextFactoryImpl.class) {
            if (instance == null)
                instance = new ServiceContextFactoryImpl();
        }
        return instance;
    }

    public ServiceContext createServiceContext()
            throws InsufficientResourcesException, SecurityException
    {
        SecurityManager sec = System.getSecurityManager();
        if (sec != null)
            sec.checkPermission(new ServiceContextPermission("create", "own"));
        throw new InsufficientResourcesException("Only one ServiceContext allowed");
    }

    public ServiceContext getServiceContext(XletContext context)
            throws SecurityException, ServiceContextException
    {
        SecurityManager sec = System.getSecurityManager();
        if (sec != null)
            sec.checkPermission(new ServiceContextPermission("access", "own"));
        return serviceContexts[0];
    }

    public ServiceContext[] getServiceContexts() {
        SecurityManager sec = System.getSecurityManager();
        if (sec != null)
            sec.checkPermission(new ServiceContextPermission("access", "own"));
        return serviceContexts;
    }

    private ServiceContext[] serviceContexts;

    private static ServiceContextFactoryImpl instance = null;
}
