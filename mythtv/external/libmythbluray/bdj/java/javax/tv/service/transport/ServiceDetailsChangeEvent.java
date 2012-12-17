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
import javax.tv.service.navigation.ServiceDetails;

public class ServiceDetailsChangeEvent extends TransportSIChangeEvent {
    public ServiceDetailsChangeEvent(Transport transport, SIChangeType type,
            ServiceDetails details)
    {
        super(transport, type, details);
        this.details = details;
    }

    public ServiceDetails getServiceDetails()
    {
        return details;
    }

    private ServiceDetails details;
    private static final long serialVersionUID = 1919091258257916100L;
}
