/*
 * This file is part of libbluray
 * Copyright (C) 2014  Petri Hintukainen <phintuka@users.sourceforge.net>
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

package org.videolan;

import java.lang.reflect.Method;

import java.util.Iterator;
import java.util.LinkedList;

import java.security.AccessController;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;

import java.net.Socket;
import java.net.SocketImpl;

class BDJSockets {

    protected BDJSockets() {
    }

    protected synchronized void add(Object obj) {
        if (!(obj instanceof SocketImpl)) {
            logger.error("expected SocketImpl, got " + obj);
            throw new Error("expected SocketImpl");
        }

        if (closed) {
            logger.error("Terminated Xlet tried to create socket at " + Logger.dumpStack());
            throw new Error("Terminated Xlet can not create sockets");
        }

        /* drop closed sockets */
        for (Iterator it = sockets.iterator(); it.hasNext(); ) {
            SocketImpl socketImpl = (SocketImpl)it.next();
            Socket     socket = getSocket(socketImpl);
            if (socket != null && socket.isClosed()) {
                it.remove();
            }
        }

        sockets.addLast(obj);
    }

    protected synchronized void closeAll() {
        closed = true;

        while (!sockets.isEmpty()) {
            SocketImpl socketImpl = (SocketImpl)sockets.removeFirst();
            Socket socket = getSocket(socketImpl);
            if (socket != null && !socket.isClosed()) {
                logger.warning("Closing " + socket);
                try {
                    socket.close();
                } catch (Exception e) {
                    logger.error("Failed to close socket: " + e);
                }
            }
        }
    }

    private Socket getSocket(SocketImpl socketImpl) {
        try {
            final SocketImpl si = socketImpl;
            return (Socket)AccessController.doPrivileged(
                new PrivilegedExceptionAction() {
                    public Object run() throws Exception {
                        Method getSocket = SocketImpl.class.getDeclaredMethod("getSocket", new Class[0]);
                        getSocket.setAccessible(true);
                        return getSocket.invoke(si, new Object[0]);
                    }
                });
        } catch (PrivilegedActionException e) {
            logger.error("Failed to get Socket: " + e.getException() + " at " + Logger.dumpStack());
            return null;
        }
    }

    private LinkedList sockets = new LinkedList();
    private boolean    closed  = false;

    private static final Logger logger = Logger.getLogger(BDJSockets.class.getName());
}
