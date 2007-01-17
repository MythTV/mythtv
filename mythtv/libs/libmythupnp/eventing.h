//////////////////////////////////////////////////////////////////////////////
// Program Name: Eventing.h
//                                                                            
// Purpose - 
//                                                                            
// Created By  : David Blain                    Created On : Dec 22, 2006
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef EVENTING_H_
#define EVENTING_H_

#include <sys/time.h>

#include <qdom.h>
#include <qurl.h>
#include <quuid.h>
#include <qdatetime.h> 
#include <qptrlist.h>
#include <qdict.h>

#include "upnpimpl.h"
#include "upnputil.h"
#include "httpserver.h"
#include "mythcontext.h"
        
//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
        
class SubscriberInfo
{
    public:

        TaskTime            ttExpires;
        TaskTime            ttLastNotified;

        QString             sUUID;
        QUrl                qURL;
        unsigned short      nKey;
        unsigned long       nDuration;       // Seconds

    public:

        // ------------------------------------------------------------------

        SubscriberInfo()
        {
            nKey           = 0;
            nDuration      = 0;
            ttLastNotified.tv_sec = 0;
            sUUID          = QUuid::createUuid().toString();
            sUUID          = sUUID.mid( 1, sUUID.length() - 2);


        }

        // ------------------------------------------------------------------

        SubscriberInfo( const QString &url, unsigned long duration )
        {
            nKey           = 0;
            sUUID          = QUuid::createUuid().toString();
            sUUID          = sUUID.mid( 1, sUUID.length() - 2);
            qURL           = url;
            nDuration      = duration;
            ttLastNotified.tv_sec = 0;

            SetExpireTime( nDuration );
        }

        // ------------------------------------------------------------------

        unsigned long IncrementKey()
        {
            // When key wraps around to zero again... must make it a 1. (upnp spec)

            if ((++nKey) == 0)
                nKey = 1;

            return nKey;
        }

    protected:

        void SetExpireTime( unsigned long nSecs )
        {
            TaskTime tt;
            gettimeofday( &tt, NULL );

            AddMicroSecToTaskTime( tt, (nSecs * 1000000) );

            ttExpires = tt;
        }


};

//////////////////////////////////////////////////////////////////////////////

class Subscribers : public QDict< SubscriberInfo > 
{
    public:

        Subscribers()
        {
            setAutoDelete( true );
        }       
};

typedef QDictIterator< SubscriberInfo >  SubscriberIterator;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class StateVariableBase
{
    public:

        bool        m_bNotify;
        QString     m_sName;
        TaskTime    m_ttLastChanged;

    public:

        StateVariableBase( const QString &sName, bool bNotify = FALSE ) 
        {
            m_bNotify = bNotify;
            m_sName   = sName;
            gettimeofday( &m_ttLastChanged, NULL );
        }

        virtual QString ToString() = 0;
};
  
//////////////////////////////////////////////////////////////////////////////

template< class T >
class StateVariable : public StateVariableBase
{
    private:
        
        T     m_value;

    public:

        // ------------------------------------------------------------------

        StateVariable( const QString &sName, bool bNotify = FALSE ) : StateVariableBase( sName, bNotify )
        {
        }

        // ------------------------------------------------------------------

        StateVariable( const QString &sName, T value, bool bNotify = FALSE ) : StateVariableBase( sName, bNotify )
        {
            m_value = value;
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
                gettimeofday( &m_ttLastChanged, NULL );
            }
        }
};

//////////////////////////////////////////////////////////////////////////////

class StateVariables : public QDict< StateVariableBase > 
{
    protected:

        virtual void Notify() = 0;

    public:

        // ------------------------------------------------------------------

        StateVariables()
        {
            setAutoDelete( true );
        }     

        // ------------------------------------------------------------------

        void AddVariable( StateVariableBase *pBase )
        {
            if (pBase != NULL)
                insert( pBase->m_sName, pBase );
        }

        // ------------------------------------------------------------------
        template < class T >
        bool SetValue( const QString &sName, T value )
        {
            StateVariable< T > *pVariable = dynamic_cast< StateVariable< T > *>( find( sName ) );

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
            StateVariable< T > *pVariable = dynamic_cast< StateVariable< T > *>( find( sName ) );

            if (pVariable != NULL)
                return pVariable->GetValue();

            return T(0);
        }

};

typedef QDictIterator< StateVariableBase >  StateVariableIterator;
        
              
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// Eventing Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class Eventing : public HttpServerExtension,
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
        int          BuildNotifyBody  ( QTextStream &ts, TaskTime ttLastNotified );

        void         HandleSubscribe  ( HTTPRequest *pRequest ); 
        void         HandleUnsubscribe( HTTPRequest *pRequest ); 

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceEventURL  () { return m_sEventMethodName; }

    public:
                 Eventing      ( const QString &sExtensionName, const QString &sEventMethodName ); 
        virtual ~Eventing      ( );

        virtual bool ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );

        short    HoldEvents    ( );
        short    ReleaseEvents ( );

        void     ExecutePostProcess( );


};

#endif
