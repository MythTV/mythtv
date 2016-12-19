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

package org.videolan;

import java.io.Serializable;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

import java.rmi.RemoteException;
import java.rmi.NotBoundException;
import java.rmi.AlreadyBoundException;
import java.rmi.Remote;

import java.security.AccessController;
import java.security.PrivilegedAction;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import javax.tv.xlet.XletContext;

import org.videolan.BDJXletContext;


public class IxcRegistryImpl {

    private boolean isProxiedRemoteObj(Object obj) {
        return Proxy.isProxyClass(obj.getClass()) &&
            (Proxy.getInvocationHandler(obj) instanceof RemoteObjectInvocationHandler);
    }

    private Object wrapImportedObject(WrappedRemoteObj remoteObj, XletContext toContext)
        throws RemoteException {

        if (toContext == remoteObj.context) {
            return remoteObj.object;
        }
        if (toContext == null) {
            logger.error("toContext is null");
            return remoteObj.object;
        }
        if (remoteObj.context == null) {
            logger.error("remote context is null");
        }

        InvocationHandler handler = new RemoteObjectInvocationHandler(remoteObj);
        ClassLoader cl = ((BDJXletContext)toContext).getClassLoader();

        ArrayList list = new ArrayList();
        getAllInterfaces(remoteObj.object.getClass(), list);

        ArrayList FilteredList = new ArrayList();
        for (int i = 0; i < list.size(); i++) {
            try {
                Class cls = cl.loadClass(((Class)list.get(i)).getName());
                if (cls.isInterface() && Remote.class.isAssignableFrom(cls)) {
                    FilteredList.add(cls);
                }
            }
            catch (ClassNotFoundException e) {
                logger.error("" + e);
            }
        }

        Class[] ifs = (Class[])FilteredList.toArray(new Class[0]);

        if (ifs != null && ifs.length != 0) {
            return (Remote)Proxy.newProxyInstance(cl, ifs, handler);
        }
        return null;
    }

    private Object exportRemoteObj(Object proxiedRemoteObj, XletContext toContext) throws RemoteException
    {
        RemoteObjectInvocationHandler handler = (RemoteObjectInvocationHandler)Proxy.getInvocationHandler(proxiedRemoteObj);

        if (handler.remoteObj.context == toContext) {
            return handler.remoteObj.object;
        }

        return wrapImportedObject(handler.remoteObj, toContext);
    }

    private Object wrapOrCopy(Object obj, BDJXletContext fromContext, BDJXletContext toContext)
        throws RemoteException {

        Object result = null;
        if (null == obj) {
            return null;
        }
        if (isProxiedRemoteObj(obj)) {
            return exportRemoteObj(obj, toContext);
        }
        if (obj instanceof Remote) {
            return wrapImportedObject(new WrappedRemoteObj((Remote)obj, fromContext), toContext);
        }
        if (obj instanceof WrappedRemoteObj) {
            WrappedRemoteObj wrappedObj = (WrappedRemoteObj)obj;
            if (isProxiedRemoteObj(wrappedObj.object)) {
                return exportRemoteObj(wrappedObj.object, toContext);
            }
            return wrapImportedObject(wrappedObj, toContext);
        }
        if (obj.getClass().isPrimitive() || obj.getClass().equals(Void.TYPE)) {
            return obj;
        }
        if (obj instanceof Serializable) {
            try {
                return Copy.deepCopy(toContext.getClassLoader(), (Serializable)obj);
            } catch (Exception e) {
                Debug("wrapOrCopy: failed in deepCopy:\n" + Logger.dumpStack(e));
                throw new RemoteException("serialization/deserialization failed", e);
            }
        }

        Debug("wrapOrCopy: Object is neither Serializable nor Remote");

        throw new RemoteException("Object is neither Serializable nor Remote");
    }

    private void getAllInterfaces(Class objClass, ArrayList resultList)
    {
        if (objClass == null) {
            return;
        }
        Class[] ifs = objClass.getInterfaces();

        for (int i = 0; i < ifs.length; i++) {
            int j;
            for (j = 0; (j < resultList.size()) && (resultList.get(j) != ifs[i]); j++);
            if (j == resultList.size()) {
                resultList.add(ifs[i]);
            }
        }
        getAllInterfaces(objClass.getSuperclass(), resultList);
    }

    private static final void verifyRemoteInterfaces(Class remoteClass) throws RemoteException {
        Class[] remoteInterfaces = remoteClass.getInterfaces();
        for (int i = 0; i < remoteInterfaces.length; i++) {
            if (Remote.class.isAssignableFrom(remoteInterfaces[i])) {
                Method[] remoteMethods = remoteInterfaces[i].getMethods();
                for (int j = 0; j < remoteMethods.length; j++) {
                    verifyRemoteMethod(remoteMethods[j]);
                }
            }
        }
    }

    private static final void verifyRemoteMethod(Method remoteMethod) throws RemoteException {
        Class[] expTypes = remoteMethod.getExceptionTypes();
        boolean hasRemoteException = false;
        for (int i = 0; i < expTypes.length; i++) {
            if (expTypes[i].isAssignableFrom(RemoteException.class)) {
                hasRemoteException = true;
                break;
            }
        }
        if (!hasRemoteException) {
            throw new RemoteException("no RemoteException found from remote method");
        }

        Class[] paramTypes = remoteMethod.getParameterTypes();
        for (int i = 0; i < paramTypes.length; i++) {
            verifyRemoteParameters(paramTypes[i]);
        }
        verifyRemoteParameters(remoteMethod.getReturnType());
    }

    private static final void verifyRemoteParameters(Class parameter) throws RemoteException {
        if (Remote.class.isAssignableFrom(parameter)) {
            if (!parameter.isInterface())
                throw new RemoteException("remote parameter is not an interface");
            Class[] superInterfaces = parameter.getInterfaces();
            for (int j = 0; j < superInterfaces.length; j++) {
                if (!Remote.class.isAssignableFrom(superInterfaces[j]))
                    throw new RemoteException("remote parameter not assignable");
            }
        }
        else if ((!parameter.isPrimitive()) && (!parameter.equals(Void.TYPE)) && (!Serializable.class.isAssignableFrom(parameter))) {
            throw new RemoteException("invalid parameter");
        }
    }

    private class RemoteObjectInvocationHandler implements InvocationHandler {
        public IxcRegistryImpl.WrappedRemoteObj remoteObj = null;

        public RemoteObjectInvocationHandler(IxcRegistryImpl.WrappedRemoteObj remoteObj) {
            TRACE("RemoteInvocationHandler created for " + remoteObj);
            this.remoteObj = remoteObj;
        }

        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            if (null == remoteObj.context || remoteObj.context.isReleased()) {
                Debug("invoke(): callee has been destroyed");
                throw new RemoteException("callee has been destroyed");
            }

            TRACE("RemoteInvocationHandler called for " + remoteObj);

            if (null != args) {
                /* verify interfaces */
                for (int i = 0; i < args.length; i++) {
                    if ((null != args[i]) && (Remote.class.isAssignableFrom(args[i].getClass()))) {
                        IxcRegistryImpl.verifyRemoteInterfaces(args[i].getClass());
                    }
                }
            }

            RemoteMethod remoteMethod = new RemoteMethod(method, remoteObj.context, args);

            if (remoteMethod.callerContext.isReleased()) {
                Debug("invoke(): caller has been destroyed");
                throw new RemoteException("caller has been destroyed");
            }

            Thread remoteThread = new Thread(remoteObj.context.getThreadGroup(), remoteMethod, "Ixc Remote thread " + method);

            remoteMethod.callerContext.addIxcThread(remoteThread);
            remoteThread.setDaemon(false);
            remoteThread.start();
            remoteThread.join();
            remoteMethod.callerContext.removeIxcThread(remoteThread);

            if (!remoteMethod.finished) {
                Debug("invoke(): calling xlet destroyed during remote execution");
                throw new RemoteException("calling xlet destroyed during remote execution");
            }
            if (remoteMethod.exception != null) {
                Debug("Exception in remote thread");
                if (remoteMethod.exception instanceof InvocationTargetException) {
                    throw ((InvocationTargetException)remoteMethod.exception).getTargetException();
                }
                Debug("Exception in remote thread is not InvocationTargetException");
                throw remoteMethod.exception;
            }

            Object result = remoteMethod.retInCaller;
            TRACE("RPC return " + result);
            return result;
        }

        private class RemoteMethod implements Runnable
        {
            final BDJXletContext calleeContext;
            final BDJXletContext callerContext;
            final Method         methodInCallee;
            Object[]  argsInCallee = new Object[0];
            Exception exception = null;
            boolean   finished = false;
            Object    retInCaller = null;

            public RemoteMethod(final Method method, BDJXletContext context, Object[] args)
                throws RemoteException {

                callerContext  = BDJXletContext.getCurrentContext();
                if (callerContext == null) {
                    logger.error("caller context is null");
                    throw new RemoteException("no caller context");
                }
                if (context == null) {
                    logger.error("callee context is null");
                    throw new RemoteException("no callee context");
                }
                calleeContext  = context;

                //methodInCallee = findMethodInCallee(method);
                methodInCallee = (Method)AccessController.doPrivileged(
                    new PrivilegedAction() {
                        public Object run() {
                            return findMethodInCallee(method);
                        }
                    });

                if (null != args) {
                    argsInCallee = new Object[args.length];
                    for (int i = 0; i < args.length; i++) {
                        argsInCallee[i] = IxcRegistryImpl.this.wrapOrCopy(args[i], callerContext, calleeContext);
                    }
                }
            }

            public void run() {
                TRACE("start execution in remote thread");

                Object ret = null;
                try {
                    ret = methodInCallee.invoke(IxcRegistryImpl.RemoteObjectInvocationHandler.this.remoteObj.object, argsInCallee);
                    TRACE("result: " + ret);
                    if (ret != null) {
                        TRACE("  type: " + ret.getClass().getName());
                    }
                    retInCaller = IxcRegistryImpl.this.wrapOrCopy(ret, calleeContext, callerContext);
                }
                catch (IllegalArgumentException e) {
                    exception = e;
                } catch (IllegalAccessException e) {
                    exception = e;
                } catch (InvocationTargetException e) {
                    try {
                        exception = ((Exception)IxcRegistryImpl.this.wrapOrCopy(e, calleeContext, callerContext));
                    }
                    catch (RemoteException e1) {
                        exception = e1;
                    }
                } catch (RemoteException e) {
                    exception = e;
                } finally {
                    finished = true;
                }
            }

            private Method findMethodInCallee(Method method) {
                try {
                    Class c = method.getDeclaringClass();
                    Method[] methodsInCaller = callerContext.getClassLoader().loadClass(c.getName()).getDeclaredMethods();
                    Method[] methodsInCallee = calleeContext.getClassLoader().loadClass(c.getName()).getDeclaredMethods();

                    for (int i = 0; i < methodsInCaller.length; i++) {
                        if (methodsInCaller[i].equals(method)) {
                            Method result = methodsInCallee[i];
                            TRACE("method in callee: " + result);
                            return result;
                        }
                    }
                    TRACE("can't find method in callee");
                } catch (SecurityException e) {
                    TRACE("can't find method in callee: " + e + "\n" + Logger.dumpStack(e));
                } catch (ClassNotFoundException e) {
                    TRACE("can't find method in callee: " + e + "\n" + Logger.dumpStack(e));
                }

                return null;
            }
        }
    }

    /*
     * store
     */

    private class WrappedRemoteObj
    {
        final Remote         object;
        final BDJXletContext context;

        public WrappedRemoteObj(Remote object, BDJXletContext context)
        {
            this.object  = object;
            this.context = context;
        }
    }

    private final HashMap remoteObjects = new HashMap();

    /*
     * debug
     */

    private static final boolean DEBUG = true;
    private static final boolean TRACE = true;
    private static final Logger logger = Logger.getLogger(IxcRegistryImpl.class.getName());

    private static void Debug(String s) {
        if (DEBUG) {
            logger.info(s);
        }
    }

    private static void TRACE(String s) {
        if (TRACE) {
            logger.info("Ixc TRACE: " + s);
        }
    }

    /*
     * interface
     */

    public void bind(XletContext xc, String path, Remote obj)
        throws AlreadyBoundException {

        Debug("IxcRegistry.bind(" + xc + ", " + path + ", " + obj + ")");

        if (!(xc instanceof BDJXletContext) || (BDJXletContext)xc != BDJXletContext.getCurrentContext()) {
            Debug("bind(): xc not current BDJXletContext");
            throw new IllegalArgumentException("xc not current BDJXletContext");
        }
        if (((BDJXletContext)xc).isReleased()) {
            Debug("bind(): xc is destroyed");
            return;
        }

        synchronized (remoteObjects) {
            if (remoteObjects.containsKey(path))
                throw new AlreadyBoundException();
            remoteObjects.put(path, new WrappedRemoteObj(obj, (BDJXletContext)xc));
        }
    }

    public Remote lookup(XletContext xc, String path)
        throws NotBoundException, RemoteException {

        Debug("IxcRegistry.lookup(" + xc + ", " + path + ")");

        if (!(xc instanceof BDJXletContext) || (BDJXletContext)xc != BDJXletContext.getCurrentContext()) {
            Debug("lookup(): xc not current BDJXletContext");
            throw new IllegalArgumentException("xc not current BDJXletContext");
        }

        if ("/7fff7669/4050/Messenger".equals(path)) {
            /* known discs:
               - Terminator Salvation
            */
            try {
                logger.error("Enabling Ixc delay hack for " + path);
                Thread.sleep(200L);
            } catch (InterruptedException ie) {
                ie.printStackTrace();
            }
        }

        WrappedRemoteObj wrappedObj = null;
        synchronized (remoteObjects) {
            if (!remoteObjects.containsKey(path)) {
                throw new NotBoundException();
            }
            wrappedObj = (WrappedRemoteObj)remoteObjects.get(path);
        }
        if (null == wrappedObj) {
            return null;
        }
        Object remoteObj = wrapOrCopy(wrappedObj, wrappedObj.context, (BDJXletContext)xc);

        Debug("IxcRegistry.lookup(" + path + ") => OK");

        return (Remote)remoteObj;
    }


    public void unbind(String path) throws NotBoundException
    {
        Debug("IxcRegistry.unbind(" + path + ")");

        synchronized (remoteObjects) {
            if (!remoteObjects.containsKey(path))
                throw new NotBoundException();

            WrappedRemoteObj wrappedObj = (WrappedRemoteObj)remoteObjects.get(path);
            if (wrappedObj != null && wrappedObj.context != BDJXletContext.getCurrentContext()) {
                throw new IllegalArgumentException("invalid context");
            }

            remoteObjects.remove(path);
        }

        Debug("IxcRegistry.unbind(" + path + ") OK");
    }

    public String[] list()
    {
        String[] result = null;
        int i = 0;
        synchronized (remoteObjects) {
            result = new String[remoteObjects.size()];
            Iterator it = remoteObjects.keySet().iterator();
            while (it.hasNext()) {
                result[(i++)] = ((String)it.next());
            }
        }

        if (DEBUG) {
            Debug("IxcRegistry.list():");
            for (i = 0; i < result.length; i++)
                Debug("\t" + i + " => " + result[i]);
        }

        return result;
    }

    public void unbindAll() {
        if (null != BDJXletContext.getCurrentContext()) {
            logger.error("unbindAll() from wrong thread: " + Logger.dumpStack());
            return;
        }

        synchronized (remoteObjects) {
            remoteObjects.clear();
        }
    }

    public void unbindAll(XletContext xc) {
        XletContext requestor = BDJXletContext.getCurrentContext();
        if (requestor != null && requestor != xc) {
            logger.error("unbindAll(ctx) from wrong thread: " + Logger.dumpStack());
            return;
        }

        Debug("IxcRegistry.removeBinding(" + xc + ")");
        String prefix = "/" + (String)xc.getXletProperty("dvb.org.id") + "/" + (String)xc.getXletProperty("dvb.app.id") + "/";

        synchronized (remoteObjects) {
            Iterator it = remoteObjects.keySet().iterator();
            while (it.hasNext()) {
                Object o = it.next();
                String path = (String)o;
                if (path.startsWith(prefix))
                    it.remove();
            }
        }
    }
}
