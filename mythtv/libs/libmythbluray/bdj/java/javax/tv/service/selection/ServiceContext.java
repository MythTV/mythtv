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

import javax.tv.locator.Locator;
import javax.tv.locator.InvalidLocatorException;
import javax.tv.service.Service;

public interface ServiceContext {
    public void stop() throws SecurityException;

    public void destroy() throws SecurityException;

    public void select(Service service) throws SecurityException;

    public void select(Locator[] locators) throws InvalidLocatorException,
            InvalidServiceComponentException, SecurityException;

    public ServiceContentHandler[] getServiceContentHandlers()
            throws SecurityException;

    public Service getService();

    public void addListener(ServiceContextListener listener);

    public void removeListener(ServiceContextListener listener);
}
