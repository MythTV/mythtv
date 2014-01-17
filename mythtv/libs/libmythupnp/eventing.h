//////////////////////////////////////////////////////////////////////////////
// Program Name: Eventing.h
// Created     : Dec. 22, 2006
//
// Purpose     : uPnp Eventing Base Class Definition
//
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details                    
//
//////////////////////////////////////////////////////////////////////////////

#ifndef EVENTING_H_
#define EVENTING_H_

#include <QUrl>
#include <QUuid>
#include <QMap>

#include "upnpserviceimpl.h"
#include "upnputil.h"
#include "httpserver.h"

class QTextStream;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC SubscriberInfo
{
    public:
        SubscriberInfo()
            : nKey( 0 ), nDuration( 0 )
        {
            memset( &ttExpires, 0, sizeof( ttExpires ) );
            memset( &ttLastNotified, 0, sizeof( ttLastNotified ) );
            sUUID          = QUuid::createUuid().toString();
            sUUID          = sUUID.mid( 1, sUUID.length() - 2);
        }

        SubscriberInfo( const QString &url, unsigned long duration )
            : nKey( 0 ), nDuration( duration )
        {
            memset( &ttExpires, 0, sizeof( ttExpires ) );
            memset( &ttLastNotified, 0, sizeof( ttLastNotified ) );
            sUUID          = QUuid::createUuid().toString();
            sUUID          = sUUID.mid( 1, sUUID.length() - 2);
            qURL           = url;

            SetExpireTime( nDuration );
        }

        unsigned long IncrementKey()
        {
            // When key wraps around to zero again... must make it a 1. (upnp spec)
            if ((++nKey) == 0)
                nKey = 1;

            return nKey;
        }

        TaskTime            ttExpires;
        TaskTime            ttLastNotified;

        QString             sUUID;
        QUrl                qURL;
        unsigned short      nKey;
        unsigned long       nDuration;       // Seconds

    protected:

        void SetExpireTime( unsigned long nSecs )
        {
            TaskTime tt;
            gettimeofday( (&tt), NULL );

            AddMicroSecToTaskTime( tt, (nSecs * 1000000) );

            ttExpires = tt;
        }


};

//////////////////////////////////////////////////////////////////////////////

typedef QMap<QString,SubscriberInfo*> Subscribers;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC  StateVariableBase
{
    public:

        bool        m_bNotify;
        QString     m_sName;
        TaskTime    m_ttLastChanged;

    public:

        explicit StateVariableBase( const QString &sName, bool bNotify = false )
        {
            m_bNotify = bNotify;
            m_sName   = sName;
            gettimeofday( (&m_ttLastChanged), NULL );
        }
        virtual ~StateVariableBase() {};

        virtual QString ToString() = 0;
};

//////////////////////////////////////////////////////////////////////////////

template< class T >
class UPNP_PUBLIC  StateVariable : public StateVariableBase
{
    private:

        T     m_value;

    public:

        // ------------------------------------------------------------------

        StateVariable( const QString &sName, bool bNotify = false ) : StateVariableBase( sName, bNotify ), m_value( T( ) )
        {
        }

        // ------------------------------------------------------------------

        StateVariable( const QString &sName, T value, bool bNotify = false ) : StateVariableBase( sName, bNotify ), m_value(value)
        {
        }

        // ------------------------------------------------------------------

        virtual QString ToString()
        {
            return QString( "%1" ).arg( m_value );
        }

        // ------------------------------------------------------------------

        T GetValue()
        {
            return m_value;
        }

        // ------------------------------------------------------------------

        void SetValue( T value )
        {
            if ( m_value != value )
            {
                m_value = value;
                gettimeofday( (&m_ttLastChanged), NULL );
            }
        }
};

//////////////////////////////////////////////////////////////////////////////

template<typename T>
inline T state_var_init(const T*) { return (T)(0); }
template<>
inline QString state_var_init(const QString*) { return QString(); }

class UPNP_PUBLIC StateVariables
{
    protected:

        virtual void Notify() = 0;
        typedef QMap<QString, StateVariableBase*> SVMap;
        SVMap m_map;
    public:

        // ------------------------------------------------------------------

        StateVariables() { }
        virtual ~StateVariables()
        {
            SVMap::iterator it = m_map.begin();
            for (; it != m_map.end(); ++it)
                delete *it;
            m_map.clear();
        }

        // ------------------------------------------------------------------

        void AddVariable( StateVariableBase *pBase )
        {
            if (pBase != NULL)
                m_map.insert(pBase->m_sName, pBase);
        }

        // ------------------------------------------------------------------
        template < class T >
        bool SetValue( const QString &sName, T value )
        {
            SVMap::iterator it = m_map.find(sName);
            if (it == m_map.end())
                return false;

            StateVariable< T > *pVariable =
                dynamic_cast< StateVariable< T > *>( *it );

            if (pVariable == NULL)
                return false;           // It's not the expected type.

            if ( pVariable->GetValue() != value)
            {
                pVariable->SetValue( value );

                if (pVariable->m_bNotify)
                    Notify();
            }

            return true;
        }

        // ------------------------------------------------------------------

        template < class T >
        T GetValue( const QString &sName )
        {
            T *dummy = NULL;
            SVMap::iterator it = m_map.find(sName);
            if (it == m_map.end())
                return state_var_init(dummy);

            StateVariable< T > *pVariable =
                dynamic_cast< StateVariable< T > *>( *it );

            if (pVariable != NULL)
                return pVariable->GetValue();

            return state_var_init(dummy);
        }

        uint BuildNotifyBody(QTextStream &ts, TaskTime ttLastNotified) const;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// Eventing Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC  Eventing : public HttpServerExtension,
                              public StateVariables,
                              public IPostProcess,
                              public UPnpServiceImpl
{

    protected:

        QMutex              m_mutex;

        QString             m_sEventMethodName;
        Subscribers         m_Subscribers;

        int                 m_nSubscriptionDuration;

        short               m_nHoldCount;

        SubscriberInfo     *m_pInitializeSubscriber;

    protected:

        virtual void Notify           ( );
        void         NotifySubscriber ( SubscriberInfo *pInfo );
        void         HandleSubscribe  ( HTTPRequest *pRequest );
        void         HandleUnsubscribe( HTTPRequest *pRequest );

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceEventURL  () { return m_sEventMethodName; }

    public:
                 Eventing      ( const QString &sExtensionName,
                                 const QString &sEventMethodName,
                                 const QString &sSharePath );
        virtual ~Eventing      ( );

        virtual QStringList GetBasePaths();

        virtual bool ProcessRequest( HTTPRequest *pRequest );

        short    HoldEvents    ( );
        short    ReleaseEvents ( );

        void     ExecutePostProcess( );


};

#endif
