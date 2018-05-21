/*
 * This file is part of libbluray
 * Copyright (C) 2015  Petri Hintukainen <phintuka@users.sourceforge.net>
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

import java.io.FilePermission;
import java.io.File;
import java.util.PropertyPermission;
import java.security.AccessController;
import java.security.Permission;
import java.security.PrivilegedAction;

final class BDJSecurityManager extends SecurityManager {

    private String discRoot;
    private String cacheRoot;
    private String budaRoot;
    private String persistentRoot;
    private boolean usingUdf = false;

    private static Class urlPermission = null;
    static {
        try {
            /* Java 8 */
            urlPermission = Class.forName("java.net.URLPermission");
        } catch (Exception e) {
        }
    }

    BDJSecurityManager(String discRoot, String persistentRoot, String budaRoot) {
        this.discRoot  = discRoot;
        this.cacheRoot = null;
        this.budaRoot  = budaRoot;
        this.persistentRoot = persistentRoot;
        if (discRoot == null) {
            usingUdf = true;
        }
    }

    protected void setCacheRoot(String root) {
        if (cacheRoot != null) {
            // limit only
            if (!root.startsWith(cacheRoot)) {
                logger.error("setCacheRoot(" + root + ") denied\n" + Logger.dumpStack());
                throw new SecurityException("cache root already set");
            }
        }
        cacheRoot = root;
    }

    /*
     *
     */

    private void deny(Permission perm) {
        logger.error("denied " + perm + "\n" + Logger.dumpStack());
        throw new SecurityException("denied " + perm);
    }

    public void checkPermission(Permission perm) {
        if (perm instanceof RuntimePermission) {
            if (perm.implies(new RuntimePermission("createSecurityManager"))) {

                // allow initializing of javax.crypto.JceSecurityManager
                if (classDepth("javax.crypto.JceSecurityManager") < 3) {
                    return;
                }

                deny(perm);
            }
            if (perm.implies(new RuntimePermission("setSecurityManager"))) {
                if (classDepth("org.videolan.Libbluray") == 3) {
                    return;
                }
                deny(perm);
            }

            // work around bug in openjdk 7 / 8
            // sun.awt.AWTAutoShutdown.notifyThreadBusy is missing doPrivileged()
            // (fixed in jdk9 / http://hg.openjdk.java.net/jdk9/client/jdk/rev/5b613a3c04be )
            if (classDepth("sun.awt.AWTAutoShutdown") > 0) {
                return;
            }

            if (perm.implies(new RuntimePermission("modifyThreadGroup"))) {
                /* do check here (no need to log failures) */
                super.checkPermission(perm);
            }
        }

        else if (perm instanceof PropertyPermission) {
            // allow read
            if (perm.getActions().equals("read")) {
                String prop = perm.getName();
                if (prop.startsWith("bluray.") || prop.startsWith("dvb.") || prop.startsWith("mhp.") || prop.startsWith("aacs.")) {
                    //logger.info(perm + " granted");
                    return;
                }
                if (prop.startsWith("user.dir")) {
                    //logger.info(perm + " granted\n" + Logger.dumpStack());
                    return;
                }
            }
            try {
                super.checkPermission(perm);
            } catch (Exception e) {
                logger.info(perm + " denied by system");
                throw new SecurityException("denied " + perm);
            }
        }

        else if (perm instanceof FilePermission) {
            /* grant delete for writable files */
            if (perm.getActions().equals("delete")) {
                checkWrite(perm.getName());
                return;
            }
            /* grant read access to BD files */
            if (perm.getActions().equals("read")) {
                String file = getCanonPath(perm.getName());
                if (canRead(file)) {
                    if (usingUdf) {
                        BDJLoader.accessFile(file);
                    }
                    return;
                }
            }
            if (perm.getActions().contains("write")) {
                /* write permissions are handled in checkWrite() */
                deny(perm);
            }
        }

        /* Networking */
        else if (perm instanceof java.net.SocketPermission) {
            if (new java.net.SocketPermission("*", "connect,resolve").implies(perm)) {
                return;
            }
        }
        else if (urlPermission != null &&
                 urlPermission.isInstance(perm)) {
            logger.info("grant " + perm);
            return;
        }

        /* Java TV */
        else if (perm instanceof javax.tv.service.ReadPermission) {
            return;
        }
        else if (perm instanceof javax.tv.service.selection.ServiceContextPermission) {
            return;
        }
        else if (perm instanceof javax.tv.service.selection.SelectPermission) {
            return;
        }
        else if (perm instanceof javax.tv.media.MediaSelectPermission) {
            return;
        }

        /* DVB */
        else if (perm instanceof org.dvb.application.AppsControlPermission) {
            return;
        }
        else if (perm instanceof org.dvb.media.DripFeedPermission) {
            return;
        }
        else if (perm instanceof org.dvb.user.UserPreferencePermission) {
            return;
        }

        /* bluray */
        else if (perm instanceof org.bluray.vfs.VFSPermission) {
            return;
        }

        try {
            java.security.AccessController.checkPermission(perm);
        } catch (java.security.AccessControlException ex) {
            System.err.println(" *** caught " + ex + " at\n" + Logger.dumpStack());
            throw ex;
        }
    }

    /*
     *
     */

    public void checkExec(String cmd) {
        logger.error("Exec(" + cmd + ") denied\n" + Logger.dumpStack());
        throw new SecurityException("exec denied");
    }

    public void checkExit(int status) {
        logger.error("Exit(" + status + ") denied\n" + Logger.dumpStack());
        throw new SecurityException("exit denied");
    }

    public void checkSystemClipboardAccess() {
        throw new SecurityException("clipboard access denied");
    }

    /*
     * file read access
     */

    private boolean canRead(String file) {

        if (cacheRoot != null && file.startsWith(cacheRoot)) {
            return true;
        }
        if (discRoot != null && file.startsWith(discRoot)) {
            return true;
        }
        if (budaRoot != null && file.startsWith(budaRoot)) {
            return true;
        }
        if (persistentRoot != null && file.startsWith(persistentRoot)) {
            return true;
        }

        return false;
    }

    /*
     * File write access
     */

    private boolean canWrite(String file) {

        // Xlet can write to persistent storage and binding unit

        if (budaRoot != null && file.startsWith(budaRoot)) {
            return true;
        }
        if (persistentRoot != null && file.startsWith(persistentRoot)) {
            return true;
        }

        BDJXletContext ctx = BDJXletContext.getCurrentContext();
        if (ctx != null) {
            logger.error("Xlet write " + file + " denied at\n" + Logger.dumpStack());
            return false;
        }

        // BD-J core can write to cache
        if (cacheRoot != null && file.startsWith(cacheRoot)) {
            return true;
        }

        logger.error("BD-J write " + file + " denied at\n" + Logger.dumpStack());
        return false;
    }

    public void checkWrite(String file) {
        file = getCanonPath(file);
        if (canWrite(file)) {
            return;
        }

        throw new SecurityException("write access denied");
    }

    private String getCanonPath(String origPath)
    {
        String suffix = "";

        if (!java.io.BDFileSystem.isAbsolutePath(origPath)) {
            String home = BDJXletContext.getCurrentXletHome();
            if (home == null) {
                logger.error("Relative path " + origPath + " outside Xlet context\n" + Logger.dumpStack());
                return origPath;
            }
            origPath = home + origPath;
        }

        if (origPath.endsWith(File.separator + "*")) {
            suffix = File.separator + "*";
            origPath = origPath.substring(0, origPath.length() - 2);
        }

        final String path = origPath;
        String cpath = (String)AccessController.doPrivileged(new PrivilegedAction() {
            public Object run() {
                try {
                    return new File(path).getCanonicalPath();
                } catch (Exception ioe) {
                    logger.error("error canonicalizing " + path + ": " + ioe);
                    return null;
                }
            }
            });
        if (cpath == null) {
            throw new SecurityException("cant canonicalize " + path);
        }
        return cpath + suffix;
    }

    /*
     *
     */

    private static final Logger logger = Logger.getLogger(BDJSecurityManager.class.getName());
}
