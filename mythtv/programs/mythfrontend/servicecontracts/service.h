//////////////////////////////////////////////////////////////////////////////
// Program Name: service.h
// Created     : Jan. 19, 2010
//
// Purpose     : Base class for all Web Services 
//                                                                            
// Copyright (c) 2010 David Blain <dblain@mythtv.org>
//                                          
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef SERVICE_H_
#define SERVICE_H_

#include <QObject>
#include <QMetaType>
#include <QVariant>
#include <QFileInfo>
#include <QDateTime>
#include <QString>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Notes for derived classes -
//
//  * This implementation can't handle declared default parameters
//
//  * When called, any missing params are sent default values for its datatype
//
//  * Q_CLASSINFO( "<methodName>_Method", "BOTH" ) 
//    is used to determine HTTP method type.  
//    Defaults to "BOTH", available values:
//          "GET", "POST" or "BOTH"
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class Service : public QObject
{
    Q_OBJECT

    public:

        explicit inline Service( QObject *parent = nullptr );

    public:

        /////////////////////////////////////////////////////////////////////
        // This method should be overridden to handle non-QObject based custom types
        /////////////////////////////////////////////////////////////////////

        virtual QVariant ConvertToVariant  ( int nType, void *pValue );

        virtual void* ConvertToParameterPtr( int            nTypeId,
                                             const QString &sParamType, 
                                             void*          pParam,
                                             const QString &sValue );

        static bool ToBool( const QString &sVal );

    public:

        bool HAS_PARAM(const QString& p) const { return m_parsedParams.contains(p); }
        QList<QString> m_parsedParams; // lowercased
};

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

inline Service::Service(QObject *parent) : QObject(parent)
{
     qRegisterMetaType< QFileInfo >();
}

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SCRIPT_CATCH_EXCEPTION( default, code ) \
            try                      \
            {                        \
                code                 \
            }                        \
            catch( QString &msg )    \
            {                        \
                m_pEngine->currentContext()->throwError( QScriptContext::UnknownError, msg ); \
                return default;      \
            }                        \
            catch( const char *msg ) \
            {                        \
                m_pEngine->currentContext()->throwError( QScriptContext::UnknownError, msg ); \
                return default;      \
            }                        \
            catch( ... )             \
            {                        \
                m_pEngine->currentContext()->throwError( QScriptContext::UnknownError, "Unknown Exception" ); \
                return default;      \
            }

//////////////////////////////////////////////////////////////////////////////
// The following replaces the standard Q_SCRIPT_DECLARE_QMETAOBJECT macro
// This implementation passes the QScriptEngine to the constructor as the
// first parameter.
//////////////////////////////////////////////////////////////////////////////

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define Q_SCRIPT_DECLARE_QMETAOBJECT_MYTHTV(T, _Arg1) \
template<> inline QScriptValue qscriptQMetaObjectConstructor<T>(QScriptContext *ctx, QScriptEngine *eng, T *) /* NOLINT(bugprone-macro-parentheses) */ \
{ \
    _Arg1 arg1 = qscriptvalue_cast<_Arg1> (ctx->argument(0)); \
    T* t = new T(eng, arg1); /* NOLINT(bugprone-macro-parentheses) */ \
    if (ctx->isCalledAsConstructor()) \
        return eng->newQObject(ctx->thisObject(), t, QScriptEngine::AutoOwnership); \
    QScriptValue o = eng->newQObject(t, QScriptEngine::AutoOwnership); \
    o.setPrototype(ctx->callee().property(QString::fromLatin1("prototype"))); \
    return o; \
}


#endif
