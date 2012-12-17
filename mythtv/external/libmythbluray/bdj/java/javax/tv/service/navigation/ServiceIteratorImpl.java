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

import java.util.LinkedList;

import javax.tv.service.Service;

public class ServiceIteratorImpl implements ServiceIterator {
    public ServiceIteratorImpl(LinkedList services) {
        this.services = services;
    }

    public void toBeginning() {
        index = 0;
    }

    public void toEnd() {
        index = services.size();
    }

    public Service nextService() {
        if (index < services.size())
            return (Service)services.get(index++);
        return null;
    }

    public Service previousService() {
        if (index > 0)
            return (Service)services.get(--index);
        return null;
    }

    public boolean hasNext() {
        return index < services.size();
    }

    public boolean hasPrevious() {
        return index > 0;
    }

    private LinkedList services;
    private int index = 0;
}
