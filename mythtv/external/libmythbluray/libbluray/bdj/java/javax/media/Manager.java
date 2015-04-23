package javax.media;

import java.io.IOException;
import java.net.URL;
import java.util.Vector;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.media.protocol.DataSource;
import javax.media.protocol.URLDataSource;

/**
 * This file is a stripped down version of the Manager class from FMJ (fmj-sf.net)
 * @author Ken Larson
 *
 */
public final class Manager {
    public static Player createPlayer(URL sourceURL) throws IOException,
            NoPlayerException
    {
        return createPlayer(new MediaLocator(sourceURL));
    }

    public static Player createPlayer(MediaLocator sourceLocator)
            throws IOException, NoPlayerException
    {
        final String protocol = sourceLocator.getProtocol();
        final Vector<?> dataSourceList = getDataSourceList(protocol);
        for (int i = 0; i < dataSourceList.size(); ++i)
        {
            String dataSourceClassName = (String) dataSourceList.get(i);
            try
            {
                final Class<?> dataSourceClass = Class.forName(dataSourceClassName);
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
            catch (NoPlayerException e)
            {   // no need to log, will be logged by call to createPlayer.
                continue;
            }
            catch (ClassNotFoundException e)
            {
                logger.warning("createPlayer: "  + e);    // no need for call stack
                continue;
            }           
            catch (IOException e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }
            catch (NoClassDefFoundError e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }
            catch (Exception e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }

                
        }
        
        // if none found, try URLDataSource:
        final URL url;
        try
        {
            url = sourceLocator.getURL();
        }
        catch (Exception e)
        {   logger.log(Level.WARNING, ""  + e, e);
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
            NoPlayerException
    {
        return createPlayer(source, source.getContentType());
    }

    public static DataSource createDataSource(URL sourceURL)
            throws IOException, NoDataSourceException
    {
        return createDataSource(new MediaLocator(sourceURL));
    }

    public static DataSource createDataSource(MediaLocator sourceLocator)
            throws IOException, NoDataSourceException
    {
        final String protocol = sourceLocator.getProtocol();
        final Vector<?> dataSourceList = getDataSourceList(protocol);
        for (int i = 0; i < dataSourceList.size(); ++i)
        {
            String dataSourceClassName = (String) dataSourceList.get(i);
            try
            {
                final Class<?> dataSourceClass = Class.forName(dataSourceClassName);
                final DataSource dataSource = (DataSource) dataSourceClass.newInstance();
                dataSource.setLocator(sourceLocator);
                dataSource.connect();
                return dataSource;
                
                
            }
            catch (ClassNotFoundException e)
            {
                logger.warning("createDataSource: "  + e);    // no need for call stack
                continue;
            }
            catch (IOException e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }
            catch (NoClassDefFoundError e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }
            catch (Exception e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }

                
        }
        
        // if none found, try URLDataSource:
        final URL url;
        try
        {
            url = sourceLocator.getURL();
        }
        catch (Exception e)
        {   logger.log(Level.WARNING, ""  + e, e);
            throw new NoDataSourceException();
        }
        final URLDataSource dataSource = new URLDataSource(url);
        dataSource.connect();
        return dataSource;
    }

    public static TimeBase getSystemTimeBase()
    {
        return systemTimeBase;
    }

    public static Vector<?> getDataSourceList(String protocolName)
    {
        return getClassList(protocolName, PackageManager.getProtocolPrefixList(), "protocol", "DataSource");
    }

    public static Vector<?> getHandlerClassList(String contentName)
    {
        return getClassList(toPackageFriendly(contentName), PackageManager.getContentPrefixList(), "content", "Handler");
    }
    
    private static Player createPlayer(DataSource source, String contentType)
        throws IOException, NoPlayerException
    {
        final Vector<?> handlerClassList = getHandlerClassList(contentType);
        for (int i = 0; i < handlerClassList.size(); ++i)
        {
            final String handlerClassName = (String) handlerClassList.get(i);
            
            try
            {
                System.out.println(handlerClassName);
                final Class<?> handlerClass = Class.forName(handlerClassName);
                if (!Player.class.isAssignableFrom(handlerClass) && 
                    !MediaProxy.class.isAssignableFrom(handlerClass))
                        continue;   // skip any classes that will not be matched below.
                final MediaHandler handler = (MediaHandler) handlerClass.newInstance();
                handler.setSource(source);
                if (handler instanceof Player)
                {   return (Player) handler;
                }
                else if (handler instanceof MediaProxy)
                {
                    final MediaProxy mediaProxy = (MediaProxy) handler;
                    return createPlayer(mediaProxy.getDataSource());
                }
            }
            catch (ClassNotFoundException e)
            {
                logger.warning("createPlayer: "  + e);    // no need for call stack
                continue;
            }           
            catch (IncompatibleSourceException e)
            {
                logger.warning("createPlayer(" + source + ", " + contentType + "): "  + e);   // no need for call stack
                continue;
            }
            catch (IOException e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }
            catch (NoPlayerException e)
            {   // no need to log, will be logged by call to createPlayer.
                continue;
            }
            catch (NoClassDefFoundError e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }
            catch (Exception e)
            {
                logger.log(Level.WARNING, ""  + e, e);
                continue;
            }
        }
        throw new NoPlayerException("No player found for " + source.getLocator());
    }
    
    private static char toPackageFriendly(char c)
    {   if (c >= 'a' && c <= 'z')
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
    
    private static String toPackageFriendly(String contentName)
    {
        final StringBuffer b = new StringBuffer();
        for (int i = 0; i < contentName.length(); ++i)
        {   
            final char c = contentName.charAt(i);
            b.append(toPackageFriendly(c));
        }
        return b.toString();
    }
    
    public static Vector<String> getClassList(String contentName, Vector<?> packages, String component2, String className)
    {
        
        final Vector<String> result = new Vector<String>();
        //result.add("media." + component2 + "." + contentName + "." + className);
        
        for (int i = 0; i < packages.size(); ++i)
        {
            result.add(((String) packages.get(i)) + ".media." + component2 + "." + contentName + "." + className);
        }
        
        return result;
    }

    public static final String UNKNOWN_CONTENT_NAME = "unknown";
    
    private static final TimeBase systemTimeBase = new SystemTimeBase();
    private static final Logger logger = Logger.getLogger(Manager.class.getName());
}
