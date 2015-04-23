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

import javax.tv.service.SIRetrievable;
import javax.tv.service.Service;
import javax.tv.service.SIElement;
import javax.tv.service.SIRequest;
import javax.tv.service.SIRequestorImpl;

public final class SIElementFilter extends ServiceFilter
{
    public SIElementFilter(SIElement element) throws FilterNotSupportedException {
        this.element = element;
    }

    public SIElement getFilterValue() {
        return element;
    }

    public boolean accept(Service service) {
        SIRequestorImpl requestor = new SIRequestorImpl();
        
        SIRequest req = service.retrieveDetails(requestor);
        
        // TODO: This may be a bit excessive
        int timeout = 0;
        while (!requestor.getResponse() && timeout < 1000) {
            try {
                Thread.sleep(1);
            } catch (InterruptedException e) {
                // ignore
            }
            
            timeout++;
        }
        
        // if we still don't have a response just cancel the request
        if (!requestor.getResponse()) {
            req.cancel();
        }
        
        if (requestor.getResult() == null)
            return false;
        
        SIRetrievable[] rets = requestor.getResult();
        for (int i = 0; i < rets.length; i++) {
            if (rets[i].equals(element))
                return true;
        }
        
        return false;
    }
    
    SIElement element;
}
