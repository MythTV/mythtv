//////////////////////////////////////////////////////////////////////////////
// Program Name: Eventing.h
// Created     : Dec. 22, 2006
//
// Purpose     : uPnp Eventing Base Class Definition
//
// Copyright (c) 2006 David Blain <dblain@mythtv.org>
//
// Licensed under the GPL v2 or later, see LICENSE for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef EVENTING_H_
#define EVENTING_H_

#include <chrono>
#include <utility>

#include <QMap>
#include <QUrl>
#include <QUuid>

#include "upnpserviceimpl.h"
#include "httpserver.h"

class QTextStream;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC SubscriberInfo
{
    public:
        SubscriberInfo()
        {
            m_ttExpires = 0us;
            m_ttLastNotified = 0us;
            m_sUUID = QUuid::createUuid().toString();
            m_sUUID = m_sUUID.mid( 1, m_sUUID.length() - 2);
        }

        SubscriberInfo( const QString &url, std::chrono::seconds duration )
            : m_qURL(url), m_nDuration( duration )
        {
            m_ttExpires = 0us;
            m_ttLastNotified = 0us;
            m_sUUID = QUuid::createUuid().toString();
            m_sUUID = m_sUUID.mid( 1, m_sUUID.length() - 2);

            SetExpireTime( m_nDuration );
        }

        unsigned long IncrementKey()
        {
            // When key wraps around to zero again... must make it a 1. (upnp spec)
            if ((++m_nKey) == 0)
                m_nKey = 1;

            return m_nKey;
        }

        std::chrono::microseconds m_ttExpires      {};
        std::chrono::microseconds m_ttLastNotified {};

        QString             m_sUUID;
        QUrl                m_qURL;
        unsigned short      m_nKey      {0};
        std::chrono::seconds m_nDuration {0s};

    protected:

        void SetExpireTime( std::chrono::seconds secs )
        {
            m_ttExpires = nowAsDuration<std::chrono::microseconds>() + secs;
        }


};

//////////////////////////////////////////////////////////////////////////////

using Subscribers = QMap<QString,SubscriberInfo*>;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class UPNP_PUBLIC  StateVariableBase
{
    public:

        bool        m_bNotify;
        QString     m_sName;
        std::chrono::microseconds m_ttLastChanged {};

    public:

        explicit StateVariableBase( QString sName, bool bNotify = false )
          : m_bNotify(bNotify), m_sName(std::move(sName))
        {
            m_ttLastChanged = nowAsDuration<std::chrono::microseconds>();
        }
        virtual ~StateVariableBase() = default;

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

        explicit StateVariable( const QString &sName, bool bNotify = false ) : StateVariableBase( sName, bNotify ), m_value( T( ) )
        {
        }

        // ------------------------------------------------------------------

        StateVariable( const QString &sName, T value, bool bNotify = false ) : StateVariableBase( sName, bNotify ), m_value(value)
        {
        }

        // ------------------------------------------------------------------

        QString ToString() override // StateVariableBase
        {
            return QString( "%1" ).arg( m_value );
        }

        // ------------------------------------------------------------------

        T GetValue()
        {
            return m_value;
        }

        // ------------------------------------------------------------------

        void SetValue( const T& value )
        {
            if ( m_value != value )
            {
                m_value = value;
                m_ttLastChanged = nowAsDuration<std::chrono::microseconds>();
            }
        }
};

//////////////////////////////////////////////////////////////////////////////

template<typename T>
inline T state_var_init(const T */*unused*/) { return (T)(0); }
template<>
inline QString state_var_init(const QString */*unused*/) { return {}; }

class UPNP_PUBLIC StateVariables
{
    protected:

        virtual void Notify() = 0;
        using SVMap = QMap<QString, StateVariableBase*>;
        SVMap m_map;
    public:

        // ------------------------------------------------------------------

        StateVariables() = default;
        virtual ~StateVariables()
        {
            for (const auto *it : std::as_const(m_map))
                delete it;
            m_map.clear();
        }

        // ------------------------------------------------------------------

        void AddVariable( StateVariableBase *pBase )
        {
            if (pBase != nullptr)
                m_map.insert(pBase->m_sName, pBase);
        }

        // ------------------------------------------------------------------
        template < class T >
        bool SetValue( const QString &sName, const T& value )
        {
            SVMap::iterator it = m_map.find(sName);
            if (it == m_map.end())
                return false;

            auto *pVariable = dynamic_cast< StateVariable< T > *>( *it );

            if (pVariable == nullptr)
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
            T *dummy = nullptr;
            SVMap::iterator it = m_map.find(sName);
            if (it == m_map.end())
                return state_var_init(dummy);

            auto *pVariable = dynamic_cast< StateVariable< T > *>( *it );

            if (pVariable != nullptr)
                return pVariable->GetValue();

            return state_var_init(dummy);
        }

        uint BuildNotifyBody(QTextStream &ts, std::chrono::microseconds ttLastNotified) const;
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
        Subscribers         m_subscribers;

        std::chrono::seconds m_nSubscriptionDuration {30min};

        short               m_nHoldCount            {0};

        SubscriberInfo     *m_pInitializeSubscriber {nullptr};

    protected:

        void         Notify           ( ) override; // StateVariables
        void         NotifySubscriber ( SubscriberInfo *pInfo );
        void         HandleSubscribe  ( HTTPRequest *pRequest );
        void         HandleUnsubscribe( HTTPRequest *pRequest );

        // Implement UPnpServiceImpl methods that we can

        QString GetServiceEventURL() override // UPnpServiceImpl
            { return m_sEventMethodName; }

    public:
                 Eventing      ( const QString &sExtensionName,
                                 QString sEventMethodName,
                                 const QString &sSharePath );
        ~Eventing ( ) override;

        QStringList GetBasePaths() override; // HttpServerExtension

        bool ProcessRequest( HTTPRequest *pRequest ) override; // HttpServerExtension

        short    HoldEvents    ( );
        short    ReleaseEvents ( );

        void     ExecutePostProcess( ) override; // IPostProcess


};

#endif
