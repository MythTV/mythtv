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

import javax.tv.service.SIElement;
import javax.tv.service.SIRequest;
import javax.tv.service.SIRequestor;
import javax.tv.service.ServiceType;
import javax.tv.service.Service;
import javax.tv.service.guide.ProgramSchedule;

public interface ServiceDetails extends SIElement, CAIdentification {
    public SIRequest retrieveServiceDescription(SIRequestor requestor);

    public ServiceType getServiceType();

    public SIRequest retrieveComponents(SIRequestor requestor);

    public ProgramSchedule getProgramSchedule();

    public String getLongName();

    public Service getService();

    public void addServiceComponentChangeListener(
            ServiceComponentChangeListener listener);

    public void removeServiceComponentChangeListener(
            ServiceComponentChangeListener listener);

    public DeliverySystemType getDeliverySystemType();
}
