/*
 * This file is part of libbluray
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

import java.util.Arrays;
import java.util.Comparator;
import java.util.LinkedList;

import javax.tv.locator.Locator;
import javax.tv.locator.InvalidLocatorException;
import javax.tv.service.Service;

import org.bluray.net.BDLocator;
import org.bluray.ti.TitleImpl;

public class ServiceListImpl implements ServiceList {
    public ServiceListImpl(LinkedList services) {
        this.services = services;
    }

    public ServiceList sortByName() {
        Object[] array = services.toArray();
        Arrays.sort(array, new TitleComparator());
        LinkedList list = new LinkedList();
        for (int i = 0; i < array.length; i++)
            list.add(array[i]);
        return new ServiceListImpl(list);
    }

    public ServiceList sortByNumber() throws SortNotAvailableException {
        return sortByName();
    }

    public Service findService(Locator locator) throws InvalidLocatorException {
        BDLocator bdLocator;
        if (!(locator instanceof BDLocator)) {
            try {
                bdLocator = new BDLocator(locator.toExternalForm());
            } catch (org.davic.net.InvalidLocatorException e) {
                throw new InvalidLocatorException(locator);
            }
        } else {
            bdLocator = (BDLocator)locator;
        }
        int title = bdLocator.getTitleNumber();
        if (title < 0)
            throw new InvalidLocatorException(locator);
        for (int i = 0; i < size(); i++) {
            TitleImpl ti = (TitleImpl)services.get(i);
            if (((TitleImpl)services.get(i)).getTitleNum() == title)
                return ti;
        }
        return null;
    }

    public ServiceList filterServices(ServiceFilter filter) {
        LinkedList list = new LinkedList();
        for (int i = 0; i < size(); i++) {
            Service service = getService(i);
            if (filter.accept(service))
                list.add(service);
        }
        return new ServiceListImpl(list);
    }

    public ServiceIterator createServiceIterator() {
        return new ServiceIteratorImpl(services);
    }

    public boolean contains(Service service) {
        return services.contains(service);
    }

    public int indexOf(Service service) {
        return services.indexOf(service);
    }

    public int size() {
        return services.size();
    }

    public Service getService(int num) {
        return (Service)services.get(num);
    }

    public boolean equals(Object obj) {
        if (!(obj instanceof ServiceListImpl))
            return false;
        return services.equals(((ServiceListImpl)obj).services);
    }

    public int hashCode() {
        return services.hashCode();
    }

    public void addService(Service service) {
        services.add(service);
    }

    private class TitleComparator implements Comparator {
        public int compare(Object obj1, Object obj2) {
            return ((TitleImpl)obj1).getTitleNum() - ((TitleImpl)obj2).getTitleNum();
        }
    }

    private LinkedList services;
}
