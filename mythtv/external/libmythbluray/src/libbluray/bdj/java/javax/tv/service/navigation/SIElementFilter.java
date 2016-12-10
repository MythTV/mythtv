/*
 * This file is part of libbluray
 * Copyright (C) 2010  William Hahne
 * Copyright (C) 2015-2016  Petri Hintukainen
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

import javax.tv.service.Service;
import javax.tv.service.SIElement;

import org.bluray.net.BDLocator;

import org.bluray.ti.PlayItem;
import org.bluray.ti.PlayItemImpl;
import org.bluray.ti.PlayList;
import org.bluray.ti.PlayListImpl;
import org.bluray.ti.TitleImpl;

public final class SIElementFilter extends ServiceFilter
{
    public SIElementFilter(SIElement element) throws FilterNotSupportedException {
        if (element == null) {
            System.err.println("null element");
            throw new NullPointerException();
        }

        try {
            new BDLocator(element.getLocator().toExternalForm());
        } catch (Exception e) {
            System.err.println("Invalid SI element: " + e + " at " + org.videolan.Logger.dumpStack(e));
            throw new FilterNotSupportedException();
        }

        this.element = element;
    }

    public SIElement getFilterValue() {
        return element;
    }

    public boolean accept(Service service) {

        if (service == null) {
            System.err.println("null service");
            throw new NullPointerException();
        }

        if (!(service instanceof TitleImpl))
            return false;
        TitleImpl title = (TitleImpl)service;

        if (element instanceof PlayListImpl) {
            int id = ((PlayListImpl)element).getId();
            PlayList[] pls = title.getPlayLists();
            for (int i = 0; i < pls.length; i++) {
                if (id == pls[i].getId()) {
                    return true;
                }
            }
            return false;

        } else if (element instanceof PlayItemImpl) {

            int piId = ((PlayItemImpl)element).getPlayItemId();
            int plId = ((PlayItemImpl)element).getPlayListId();

            PlayList[] pls = title.getPlayLists();
            for (int i = 0; i < pls.length; i++) {
                if (plId == pls[i].getId()) {

                    PlayItem pis[] = pls[i].getPlayItems();
                    for (int j = 0; j < pis.length; j++) {
                        if (piId == ((PlayItemImpl)pis[j]).getPlayItemId()) {
                            return true;
                        }
                    }
                }
            }
            return false;

        } else if (element instanceof ServiceDetails) {
            return element.getLocator() == service.getLocator();
        }

        System.err.println("Unsupported SI element");
        return false;
        /*
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
            if (req != null)
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
        */
    }

    SIElement element;
}
