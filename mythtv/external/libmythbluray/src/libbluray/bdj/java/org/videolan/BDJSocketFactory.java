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

import java.io.IOException;

import java.lang.reflect.Constructor;

import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketImpl;
import java.net.SocketImplFactory;

import java.security.AccessController;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;

class BDJSocketFactory implements SocketImplFactory {

    protected static void init() {
        try {
            Socket.setSocketImplFactory(new BDJSocketFactory(false));
            ServerSocket.setSocketFactory(new BDJSocketFactory(true));
        } catch (IOException e) {
            logger.error("Failed to hook SocketFactory: " + e);
        }
    }

    private BDJSocketFactory(boolean server) {
        this.server = server;
    }

    private SocketImpl newSocket() {
        try {
            return (SocketImpl)AccessController.doPrivileged(
                new PrivilegedExceptionAction() {
                    public Object run() throws Exception {
                        Class defaultSocketImpl = Class.forName("java.net.SocksSocketImpl");
                        Constructor constructor = defaultSocketImpl.getDeclaredConstructor/*s*/(new Class[0])/*[0]*/;
                        constructor.setAccessible(true);
                        return constructor.newInstance(new Object[0]);
                    }
                });
        } catch (PrivilegedActionException e) {
            logger.error("Failed to create socket: " + e.getException() + " at " + Logger.dumpStack());
            throw new RuntimeException(e.getException());
        }
    }

    public SocketImpl createSocketImpl() {

        if (server) {
            logger.error("Xlet tried to create server socket");
            throw new RuntimeException("server sockets disabled");
        }

        SocketImpl socket = newSocket();
        BDJXletContext ctx = BDJXletContext.getCurrentContext();
        if (ctx != null) {
            logger.info("Xlet " + ctx + " created new socket");
            ctx.addSocket(socket);
        } else {
            logger.error("New socket created outside of Xlet context: " + Logger.dumpStack());
        }
        return socket;
    }

    private boolean server = false;

    private static final Logger logger = Logger.getLogger(BDJSocketFactory.class.getName());
}
