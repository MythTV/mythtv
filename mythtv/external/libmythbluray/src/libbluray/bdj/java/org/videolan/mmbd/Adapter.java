/*
 * This file is part of libbluray
 * Copyright (C) 2017  VideoLAN
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

/*
 * BD-J support classes for libmmbd
 */

package org.videolan.mmbd;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;
import java.security.AccessController;
import java.security.PrivilegedAction;

import org.videolan.BDJClassFileTransformer;
import org.videolan.BDJClassLoaderAdapter;
import org.videolan.BDJXletContext;
import org.videolan.Logger;

import javax.media.ClockStartedError;
import javax.media.Manager;
import javax.media.NoPlayerException;
import javax.media.Player;
import javax.tv.xlet.Xlet;
import javax.tv.xlet.XletContext;
import javax.tv.xlet.XletStateChangeException;
import org.bluray.bdplus.StatusListener;

public class Adapter extends LoaderAdapter implements BDJClassLoaderAdapter {

    public Map getHideClasses() {
        return null;
    }

    public Map getBootClasses() {
        return bootClasses;
    }

    public Map getXletClasses() {
        return xletClasses;
    }

    public Adapter() throws ClassNotFoundException {

        if (BDJXletContext.getCurrentContext() != null)
            throw new ClassNotFoundException();

        /* relocate classes to avoid runtime package collisions */

        final String s1 = "org/videolan/mmbd/Adapter$Xlet";
        final String s2 = "org/videolan/mmbd/Adapter$If";
        Map m = new HashMap();
        String d1, d2;

        try {
            m.put(s1, d1 = (new String(b0, "UTF-8") + new String(b1, "UTF-8")));
            m.put(s2, d2 = (new String(b0, "UTF-8") + new String(b2, "UTF-8")));
        } catch (java.io.UnsupportedEncodingException uee) {
            throw new ClassNotFoundException();
        }

        BDJClassFileTransformer t = new BDJClassFileTransformer();
        byte[] c1 = t.rename(loadBootClassCode(s1), m);
        byte[] c2 = t.rename(loadBootClassCode(s2), m);

        if (c1 != null) {
            bootClasses = new HashMap();
            bootClasses.put(d1.replace('/', '.'), c1);
        }
        if (c2 != null) {
            xletClasses = new HashMap();
            xletClasses.put(d2.replace('/', '.'), c2);
        }
    }

    public static abstract class If {
        protected If() {}
    }

    public static class Xlet implements StatusListener {
        private static Xlet instance = new Xlet();
        private javax.tv.xlet.Xlet xlet = null;
        private XletContext ctx = null;

        static private void log(String s) {
            System.out.println(s);
        }
        static private void error(String s) {
            System.err.println(s);
        }

        public static Xlet initXlet(javax.tv.xlet.Xlet xlet, XletContext ctx) throws XletStateChangeException {
            if ((xlet == null) || (ctx == null)) {
                error("initXlet: null argument");
                throw new XletStateChangeException();
            }
            if ((instance.xlet == xlet) && (instance.ctx == ctx)) {
                error("initXlet: invalid arguments");
                throw new XletStateChangeException();
            }
            log("initXlet");
            instance.xlet = xlet;
            instance.ctx = ctx;
            return instance;
        }

        public static Xlet lookup(XletContext ctx) {
            return instance;
        }

        public void startXlet() throws XletStateChangeException {
        }

        public void pauseXlet() {
        }

        public void destroyXlet(boolean a) {
            this.instance.xlet = null;
            this.instance.ctx = null;
        }

        public boolean isAuthRequired() {
            log("isAuthRequired");
            return false;
        }

        public void doAuth() throws InterruptedException {
            doAuth(null);
        }

        public synchronized void doAuth(If a) throws InterruptedException, RuntimeException {
            error("doAuth");
        }

        public void destroy() {
        }

        public void receive(int i) {
        }

        public byte[] getMMV() {
            byte[] b = new byte[8];
            new java.util.Random().nextBytes(b);
            log("getMMV");
            return b;
        }

        public Player createPlayer(javax.media.protocol.DataSource s) throws IOException, NoPlayerException {
            return Manager.createPlayer(s);
        }

        public Player createPlayer(javax.media.MediaLocator l) throws IOException, NoPlayerException {
            return Manager.createPlayer(l);
        }

        public Player createPlayer(java.net.URL u) throws IOException, NoPlayerException {
            return Manager.createPlayer(u);
        }

        public void destroyPlayer(Player p) {
            if (p == null)
                return;
            p.stop();
            try {
                p.deallocate();
            } catch (ClockStartedError localClockStartedError) {
            }
            p.close();
        }
    }

    /*
     * class loader
     */

    private final byte[] b0 = {99,111,109,47,109,97,99,114,111,118,105,115,105,111,110,47,98,100,112,108,117,115,47};
    private final byte[] b1 = {77,86,67,111,109,109};
    private final byte[] b2 = {83,116,114,101,101,116,76,111,99,107,71,101,116,116,101,114};

    private byte[] loadBootClassCode(String name) throws ClassNotFoundException {
        final String path = name.replace('.', '/').concat(".class");
        InputStream is = null;
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        byte[] buffer = new byte[0xffff];
        try {
            is = (InputStream)
            AccessController.doPrivileged(
                new PrivilegedAction() {
                    public Object run() {
                        return ClassLoader.getSystemResourceAsStream(path);
                    }
                });

            if (is == null) {
                logger.error("loadBootClassCode(" + name + "): not found");
                throw new ClassNotFoundException(name);
            }

            while (true) {
                int r = is.read(buffer);
                if (r == -1) {
                    break;
                }
                os.write(buffer, 0, r);
            }

            return os.toByteArray();

        } catch (Exception e) {
            logger.error("loadBootClassCode(" + name + ") failed: " + e);
            throw new ClassNotFoundException(name);

        } finally {
            try {
                if (is != null)
                    is.close();
            } catch (IOException ioe) {
            }
            try {
                if (os != null)
                    os.close();
            } catch (IOException ioe) {
            }
        }
    }

    private Map bootClasses = new HashMap();
    private Map xletClasses = new HashMap();

    private static final Logger logger = Logger.getLogger(Adapter.class.getName());
}
