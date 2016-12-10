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

package javax.tv.service.transport;

import javax.tv.service.SIChangeType;

public class NetworkChangeEvent extends TransportSIChangeEvent {
    public NetworkChangeEvent(NetworkCollection collection, SIChangeType type,
            Network network)
    {
        super(collection, type, network);
        this.collection = collection;
        this.network = network;
    }

    public NetworkCollection getNetworkCollection()
    {
        return collection;
    }

    public Network getNetwork()
    {
        return network;
    }

    private NetworkCollection collection;
    private Network network;
    private static final long serialVersionUID = -8098805348857216815L;
}
