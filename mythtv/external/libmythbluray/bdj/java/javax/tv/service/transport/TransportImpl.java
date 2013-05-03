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

package javax.tv.service.transport;

import java.util.LinkedList;

import javax.tv.service.navigation.DeliverySystemType;

public class TransportImpl implements Transport {
    public void addServiceDetailsChangeListener(ServiceDetailsChangeListener listener) {
        synchronized (listeners) {
            if (!listeners.contains(listener))
                listeners.add(listener);
        }
    }

    public void removeServiceDetailsChangeListener(ServiceDetailsChangeListener listener) {
        synchronized (listeners) {
            listeners.remove(listener);
        }
    }

    public DeliverySystemType getDeliverySystemType() {
        return org.bluray.ti.DeliverySystemType.BD_ROM;
    }

    public void notifyListeners() {
    }

    private LinkedList listeners = new LinkedList();
}
