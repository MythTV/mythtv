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

package javax.tv.service.navigation;

import javax.tv.locator.Locator;
import javax.tv.locator.InvalidLocatorException;
import javax.tv.service.Service;

public final class LocatorFilter extends ServiceFilter {
    public LocatorFilter(Locator[] locators) throws InvalidLocatorException {
        this.locators = locators;
    }

    public Locator[] getFilterValue() {
        return locators;
    }

    public boolean accept(Service service) {
        if (service == null)
            throw new NullPointerException();

        for (int i = 0; i < locators.length; i++) {
            if (locators[i].equals(service.getLocator()))
                return true;
        }

        return false;
    }

    Locator[] locators;
}
