//////////////////////////////////////////////////////////////////////////////
// Program Name: backendStatus.h
// Created     : Oct. 18, 2021
//
// Copyright (c) 2021 Peter Bennett <pbennett@mythtv.org>
//
// Licensed under the GPL v2 or later, see COPYING for details
//
//////////////////////////////////////////////////////////////////////////////

#ifndef V2BACKENDSTATUS_H_
#define V2BACKENDSTATUS_H_

#include <QDateTime>
#include <QString>
#include <QVariantList>

#include "libmythbase/http/mythhttpservice.h"
#include "v2programAndChannel.h"
#include "v2encoder.h"
#include "v2frontend.h"

class V2Backend : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    SERVICE_PROPERTY2( QString    , Name            )
    SERVICE_PROPERTY2( QString    , IP              )
    SERVICE_PROPERTY2( QString    , Type            )

    public:

        Q_INVOKABLE V2Backend(QObject *parent = nullptr)
            : QObject         ( parent )
        {
        }

    private:
        Q_DISABLE_COPY(V2Backend);
};

Q_DECLARE_METATYPE(V2Backend*)


class V2StorageGroup : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    SERVICE_PROPERTY2(QString, Id)
    SERVICE_PROPERTY2(int, Used)
    SERVICE_PROPERTY2(int, Free)
    SERVICE_PROPERTY2(int, Deleted)
    SERVICE_PROPERTY2(int, Total)
    SERVICE_PROPERTY2(int, Expirable)
    SERVICE_PROPERTY2(int, LiveTV)
    SERVICE_PROPERTY2(QString, Directory)
    public:
        Q_INVOKABLE V2StorageGroup(QObject *parent = nullptr)
            : QObject( parent )
        {
        }
    private:
        Q_DISABLE_COPY(V2StorageGroup);
};
Q_DECLARE_METATYPE(V2StorageGroup*)

class V2MachineInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );
    Q_CLASSINFO( "StorageGroups", "type=V2StorageGroup");
    SERVICE_PROPERTY2( QVariantList, StorageGroups );
    SERVICE_PROPERTY2(float, LoadAvg1)
    SERVICE_PROPERTY2(float, LoadAvg2)
    SERVICE_PROPERTY2(float, LoadAvg3)
    SERVICE_PROPERTY2(QDateTime, GuideStart)
    SERVICE_PROPERTY2(QDateTime, GuideEnd)
    SERVICE_PROPERTY2(QDateTime, GuideThru)
    SERVICE_PROPERTY2(int, GuideDays)
    SERVICE_PROPERTY2(QString, GuideStatus)
    SERVICE_PROPERTY2(QString, GuideNext)

    public:
        Q_INVOKABLE V2MachineInfo(QObject *parent = nullptr)
            : QObject( parent )
        {
        }
        V2StorageGroup *AddNewStorageGroup()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'
            auto *pObject = new V2StorageGroup( this );
            m_StorageGroups.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }

    private:
        Q_DISABLE_COPY(V2MachineInfo);
};
Q_DECLARE_METATYPE(V2MachineInfo*)

class V2Job : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    SERVICE_PROPERTY2( int       , Id            )
    SERVICE_PROPERTY2( uint      , ChanId        )
    SERVICE_PROPERTY2( QDateTime , StartTime     )
    SERVICE_PROPERTY2( QDateTime , SchedRunTime  )
    SERVICE_PROPERTY2( QString   , StartTs       )
    SERVICE_PROPERTY2( QDateTime , InsertTime    )
    SERVICE_PROPERTY2( int       , Type          )
    SERVICE_PROPERTY2( QString   , LocalizedJobName )
    SERVICE_PROPERTY2( int       , Cmds          )
    SERVICE_PROPERTY2( int       , Flags         )
    SERVICE_PROPERTY2( int       , Status        )
    SERVICE_PROPERTY2( QString   , LocalizedStatus )
    SERVICE_PROPERTY2( QDateTime , StatusTime    )
    SERVICE_PROPERTY2( QString   , HostName      )
    SERVICE_PROPERTY2( QString   , Args          )
    SERVICE_PROPERTY2( QString   , Comment       )
    Q_PROPERTY( QObject*  Program    READ Program     USER true)
    SERVICE_PROPERTY_PTR(V2Program, Program )

    public:
        Q_INVOKABLE V2Job(QObject *parent = nullptr)
            : QObject( parent )
        {
        }
    private:
        Q_DISABLE_COPY(V2Job);

};
Q_DECLARE_METATYPE(V2Job*)


class V2BackendStatus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "Version", "1.0" );

    Q_CLASSINFO( "Encoders", "type=V2Encoder")
    Q_CLASSINFO( "Scheduled", "type=V2Program")
    Q_CLASSINFO( "Frontends", "type=V2Frontend")
    Q_CLASSINFO( "Backends", "type=V2Backend")
    Q_CLASSINFO( "JobQueue", "type=V2Job")
    Q_CLASSINFO( "AsOf"    , "transient=true"   )

    SERVICE_PROPERTY2( QDateTime   , AsOf            )
    SERVICE_PROPERTY2( QString     , Version         )
    SERVICE_PROPERTY2( QString     , ProtoVer        )
    SERVICE_PROPERTY2( QVariantList, Encoders )
    SERVICE_PROPERTY2( QVariantList, Scheduled )
    SERVICE_PROPERTY2( QVariantList, Frontends )
    SERVICE_PROPERTY2( QVariantList, Backends  )
    SERVICE_PROPERTY2( QVariantList, JobQueue      )
    Q_PROPERTY( QObject*  MachineInfo    READ MachineInfo     USER true)
    SERVICE_PROPERTY_PTR(V2MachineInfo, MachineInfo     )
    SERVICE_PROPERTY2( QString     , Miscellaneous        )

    public:
        Q_INVOKABLE V2BackendStatus(QObject *parent = nullptr)
            : QObject(parent)
        {
        }
        // These are here so that a routine in serviceutil
        // can fill the QVariantlist. Unfortunately the standard
        // call [e.g. Encoders()] generated by the macros returns a
        // const QVariantlist.
        QVariantList& GetEncoders() {return m_Encoders;}
        QVariantList& GetScheduled() {return m_Scheduled;}
        QVariantList& GetFrontends() {return m_Frontends;}

        V2Job *AddNewJob()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'
            auto *pObject = new V2Job( this );
            m_JobQueue.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }

        V2Backend *AddNewBackend()
        {
            // We must make sure the object added to the QVariantList has
            // a parent of 'this'
            auto *pObject = new V2Backend( this );
            m_Backends.append( QVariant::fromValue<QObject *>( pObject ));
            return pObject;
        }


    private:
        Q_DISABLE_COPY(V2BackendStatus);
};
Q_DECLARE_METATYPE(V2BackendStatus*)

#endif // V2BACKENDSTATUS_H_
