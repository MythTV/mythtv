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

package org.dvb.io.ixc;

import java.rmi.RemoteException;
import java.rmi.NotBoundException;
import java.rmi.AlreadyBoundException;
import java.rmi.Remote;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.logging.Logger;

import javax.tv.xlet.XletContext;

import org.dvb.application.AppID;
import org.dvb.application.AppProxy;
import org.dvb.application.AppsDatabase;

import org.videolan.BDJXletContext;

public class IxcRegistry {
    public static Remote lookup(XletContext xc, String path) throws NotBoundException, RemoteException {
        String[] parts = path.split("/", 3);

        if (parts.length != 3)
            throw new IllegalArgumentException("Malformed path");

        int orgId = Integer.parseInt(parts[0], 16);
        short appId = Short.parseShort(parts[1], 16);
        String name = parts[2];

        for (IxcObject obj : ixcList) {
            if (obj.orgId == orgId && obj.appId == appId && obj.name.equals(name)) {
                logger.info("Looked up " + path);
                return obj.obj;
            }
        }

        logger.warning("Failed to look up " + path);
        throw new NotBoundException();
    }

    public static void bind(XletContext xc, String name, Remote obj) throws AlreadyBoundException {
        if (xc == null || name == null || obj == null)
            throw new NullPointerException();

        // make sure the xlet is not currently in the destroyed state
        String orgid = (String)xc.getXletProperty("dvb.org.id");
        String appid = (String)xc.getXletProperty("dvb.app.id");
        AppID id = new AppID(Integer.parseInt(orgid, 16), Integer.parseInt(appid, 16));
        if (AppsDatabase.getAppsDatabase().getAppProxy(id).getState() == AppProxy.DESTROYED)
            return;

        int orgId = id.getOID();
        int iappId = id.getAID();
        short appId = (short)iappId;

        IxcObject ixcObj = new IxcObject(orgId, appId, name, obj);

        if (ixcList.contains(ixcObj))
            throw new AlreadyBoundException();

        ixcList.add(ixcObj);

        logger.info("Bound /" + orgid + "/" + appid + "/" + name);
    }

    public static void unbind(XletContext xc, String name) throws NotBoundException {
        if (xc == null || name == null)
            throw new NullPointerException();

        int orgId = (Integer)xc.getXletProperty("dvb.org.id");
        int iappId = (Integer)xc.getXletProperty("dvb.app.id");
        short appId = (short)iappId;

        IxcObject ixcObj = new IxcObject(orgId, appId, name, null);

        if (!ixcList.contains(ixcObj))
            throw new NotBoundException();

        ixcList.remove(ixcObj);

        logger.info("Unbound /" + Integer.toString(orgId, 16) + "/" + Integer.toString(appId, 16) + "/" + name);
    }

    public static void rebind(XletContext xc, String name, Remote obj) {

        try {
            unbind(xc, name);
        } catch (NotBoundException e) {
            // ignore
        }

        try {
            bind(xc, name, obj);
        } catch (AlreadyBoundException e) {
            logger.warning("rebind should never encounter an AlreadyBoundException, something is wrong here.");
            e.printStackTrace();
        }
    }

    public static String[] list(XletContext xc) {
        String[] out = new String[ixcList.size()];

        for (int i = 0; i < ixcList.size(); i++) {
            IxcObject obj = ixcList.get(i);

            out[i] = "/" + Integer.toString(obj.orgId, 16) + "/" + Integer.toString(obj.appId, 16) + "/" + obj.name;
        }

        return out;
    }

    private static class IxcObject {
        public IxcObject(int orgId, short appId, String name, Remote obj) {
            this.orgId = orgId;
            this.appId = appId;
            this.name = name;
            this.obj = obj;
        }

        public boolean equals(Object obj) {
            if (this == obj)
                return true;
            if (obj == null)
                return false;
            if (getClass() != obj.getClass())
                return false;
            IxcObject other = (IxcObject) obj;
            if (appId != other.appId)
                return false;
            if (name == null) {
                if (other.name != null)
                    return false;
            } else if (!name.equals(other.name))
                return false;
            if (orgId != other.orgId)
                return false;
            return true;
        }

        public int orgId;
        public short appId;
        public String name;
        public Remote obj;
    }

    private static List<IxcObject> ixcList = Collections.synchronizedList(new ArrayList<IxcObject>());
    private static Logger logger = Logger.getLogger(IxcRegistry.class.getName());
}
