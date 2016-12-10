/*
 * This file is part of libbluray
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

package javax.media;

import java.io.IOException;
import java.net.URL;
import java.util.Vector;

import javax.media.protocol.DataSource;
import javax.media.protocol.URLDataSource;

import org.videolan.Logger;

/**
 * This file is a stripped down version of the Manager class from FMJ (fmj-sf.net)
 * @author Ken Larson
 *
 */
public final class Manager {
    public static Player createPlayer(URL sourceURL) throws IOException,
            NoPlayerException {
        return createPlayer(new MediaLocator(sourceURL));
    }

    public static Player createPlayer(MediaLocator sourceLocator)
            throws IOException, NoPlayerException {
        final String protocol = sourceLocator.getProtocol();
        final Vector dataSourceList = getDataSourceList(protocol);
        for (int i = 0; i < dataSourceList.size(); ++i) {
            String dataSourceClassName = (String) dataSourceList.get(i);
            try {
                final Class dataSourceClass = Class.forName(dataSourceClassName);
                final DataSource dataSource = (DataSource) dataSourceClass.newInstance();
                dataSource.setLocator(sourceLocator);
                dataSource.connect();
                return createPlayer(dataSource);

                // TODO: JMF seems to disconnect data sources in this method, based on this stack trace:
//              java.lang.NullPointerException
//              at com.sun.media.protocol.rtp.DataSource.disconnect(DataSource.java:207)
//              at javax.media.Manager.createPlayer(Manager.java:425)
//              at net.sf.fmj.ui.application.ContainerPlayer.createNewPlayer(ContainerPlayer.java:357)
            }
            catch (NoPlayerException e) {
                // no need to log, will be logged by call to createPlayer.
                continue;
            }
            catch (ClassNotFoundException e) {
                logger.warning("createPlayer: " + e);    // no need for call stack
                continue;
            }
            catch (IOException e) {
                logger.warning(""  + e);
                continue;
            }
            catch (NoClassDefFoundError e) {
                logger.warning(""  + e);
                continue;
            }
            catch (Exception e) {
                logger.warning(""  + e);
                continue;
            }
        }

        // if none found, try URLDataSource:
        final URL url;
        try {
            url = sourceLocator.getURL();
        }
        catch (Exception e) {
            logger.warning("" + e);
            throw new NoPlayerException();
        }
        final URLDataSource dataSource = new URLDataSource(url);
        dataSource.connect();   // TODO: there is a problem because we connect to the datasource here, but
                                // the following call may try twice or more to use the datasource with a player, once
                                // for the right content type, and multiple times for unknown.  The first attempt (for example) may actually
                                // read data, in which case the second one will be missing data when it reads.
                                // really, the datasource needs to be recreated.
                                // The workaround for now is that URLDataSource (and others) allows repeated connect() calls.
        return createPlayer(dataSource);
    }

    public static Player createPlayer(DataSource source) throws IOException,
            NoPlayerException {
        return createPlayer(source, source.getContentType());
    }

    public static DataSource createDataSource(URL sourceURL)
            throws IOException, NoDataSourceException {
        return createDataSource(new MediaLocator(sourceURL));
    }

    public static DataSource createDataSource(MediaLocator sourceLocator)
            throws IOException, NoDataSourceException {
        final String protocol = sourceLocator.getProtocol();
        final Vector dataSourceList = getDataSourceList(protocol);
        for (int i = 0; i < dataSourceList.size(); ++i) {
            String dataSourceClassName = (String) dataSourceList.get(i);
            try {
                final Class dataSourceClass = Class.forName(dataSourceClassName);
                final DataSource dataSource = (DataSource) dataSourceClass.newInstance();
                dataSource.setLocator(sourceLocator);
                dataSource.connect();
                return dataSource;
            }
            catch (ClassNotFoundException e) {
                logger.warning("createDataSource: "  + e);    // no need for call stack
                continue;
            }
            catch (IOException e) {
                logger.warning("" + e);
                continue;
            }
            catch (NoClassDefFoundError e) {
                logger.warning("" + e);
                continue;
            }
            catch (Exception e) {
                logger.warning("" + e);
                continue;
            }
        }

        // if none found, try URLDataSource:
        final URL url;
        try {
            url = sourceLocator.getURL();
        }
        catch (Exception e) {
            logger.warning("" + e);
            throw new NoDataSourceException();
        }
        final URLDataSource dataSource = new URLDataSource(url);
        dataSource.connect();
        return dataSource;
    }

    public static TimeBase getSystemTimeBase() {
        return systemTimeBase;
    }

    public static Vector getDataSourceList(String protocolName) {
        return getClassList(protocolName, PackageManager.getProtocolPrefixList(), "protocol", "DataSource");
    }

    public static Vector getHandlerClassList(String contentName) {
        return getClassList(toPackageFriendly(contentName), PackageManager.getContentPrefixList(), "content", "Handler");
    }

    private static Player createPlayer(DataSource source, String contentType)
        throws IOException, NoPlayerException {
        final Vector handlerClassList = getHandlerClassList(contentType);
        for (int i = 0; i < handlerClassList.size(); ++i) {
            final String handlerClassName = (String) handlerClassList.get(i);

            try {
                System.out.println(handlerClassName);
                final Class handlerClass = Class.forName(handlerClassName);
                if (!Player.class.isAssignableFrom(handlerClass) &&
                    !MediaProxy.class.isAssignableFrom(handlerClass)) {
                    continue;   // skip any classes that will not be matched below.
                }
                final MediaHandler handler = (MediaHandler) handlerClass.newInstance();
                handler.setSource(source);

                if (handler instanceof Player) {
                    return (Player) handler;
                } else if (handler instanceof MediaProxy) {
                    final MediaProxy mediaProxy = (MediaProxy) handler;
                    return createPlayer(mediaProxy.getDataSource());
                }
            }
            catch (ClassNotFoundException e) {
                // no need for call stack
                logger.warning("createPlayer: "  + e);
                continue;
            }
            catch (IncompatibleSourceException e) {
                // no need for call stack
                logger.warning("createPlayer(" + source + ", " + contentType + "): "  + e);
                continue;
            }
            catch (IOException e) {
                logger.warning("" + e);
                continue;
            }
            catch (NoPlayerException e) {
                // no need to log, will be logged by call to createPlayer.
                continue;
            }
            catch (NoClassDefFoundError e) {
                logger.warning("" + e);
                continue;
            }
            catch (Exception e) {
                logger.warning("" + e);
                continue;
            }
        }
        logger.error("No player found for " + contentType + " / " + source.getLocator());
        throw new NoPlayerException("No player found for " + source.getLocator());
    }

    private static char toPackageFriendly(char c) {
        if (c >= 'a' && c <= 'z')
            return c;
        else if (c >= 'A' && c <= 'Z')
            return c;
        else if (c >= '0' && c <= '9')
            return c;
        else if (c == '.')
            return c;
        else if (c == '/')
            return '.';
        else
            return '_';
    }

    private static String toPackageFriendly(String contentName) {
        final StringBuffer b = new StringBuffer();
        for (int i = 0; i < contentName.length(); ++i) {
            final char c = contentName.charAt(i);
            b.append(toPackageFriendly(c));
        }
        return b.toString();
    }

    public static Vector getClassList(String contentName, Vector packages, String component2, String className) {
        final Vector result = new Vector();
        //result.add("media." + component2 + "." + contentName + "." + className);

        for (int i = 0; i < packages.size(); ++i) {
            result.add(((String) packages.get(i)) + ".media." + component2 + "." + contentName + "." + className);
        }

        return result;
    }

    public static final String UNKNOWN_CONTENT_NAME = "unknown";

    private static final TimeBase systemTimeBase = new SystemTimeBase();
    private static final Logger logger = Logger.getLogger(Manager.class.getName());
}
