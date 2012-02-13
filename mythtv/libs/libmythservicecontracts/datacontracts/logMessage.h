//////////////////////////////////////////////////////////////////////////////
// Program Name: logMessage.h
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef LOGMESSAGE_H_
#define LOGMESSAGE_H_

#include <QString>
#include <QDateTime>

#include "serviceexp.h"
#include "datacontracthelper.h"

namespace DTC
{

class SERVICE_PUBLIC LogMessage : public QObject
{
    Q_OBJECT
    Q_CLASSINFO( "version"    , "1.0" );

    Q_PROPERTY( QString    HostName        READ HostName
                                           WRITE setHostName    )
    Q_PROPERTY( QString    Application     READ Application
                                           WRITE setApplication )
    Q_PROPERTY( int        PID             READ PID
                                           WRITE setPID         )
    Q_PROPERTY( int        TID             READ TID
                                           WRITE setTID         )
    Q_PROPERTY( QString    Thread          READ Thread
                                           WRITE setThread      )
    Q_PROPERTY( QString    Filename        READ Filename
                                           WRITE setFilename    )
    Q_PROPERTY( int        Line            READ Line
                                           WRITE setLine        )
    Q_PROPERTY( QString    Function        READ Function
                                           WRITE setFunction    )
    Q_PROPERTY( QDateTime  Time            READ Time
                                           WRITE setTime        )
    Q_PROPERTY( QString    Level           READ Level
                                           WRITE setLevel       )
    Q_PROPERTY( QString    Message         READ Message
                                           WRITE setMessage     )

    PROPERTYIMP( QString  , HostName    )
    PROPERTYIMP( QString  , Application )
    PROPERTYIMP( int      , PID         )
    PROPERTYIMP( int      , TID         )
    PROPERTYIMP( QString  , Thread      )
    PROPERTYIMP( QString  , Filename    )
    PROPERTYIMP( int      , Line        )
    PROPERTYIMP( QString  , Function    )
    PROPERTYIMP( QDateTime, Time        )
    PROPERTYIMP( QString  , Level       )
    PROPERTYIMP( QString  , Message     )

    public:

        static void InitializeCustomTypes()
        {
            qRegisterMetaType< LogMessage  >();
            qRegisterMetaType< LogMessage* >();
        }

    public:

        LogMessage(QObject *parent = 0)
            : QObject       ( parent ),
              m_HostName    (        ),
              m_Application (        ),
              m_PID         ( 0      ),
              m_TID         ( 0      ),
              m_Thread      (        ),
              m_Filename    (        ),
              m_Line        ( 0      ),
              m_Function    (        ),
              m_Time        (        ),
              m_Level       (        ),
              m_Message     (        )
        {
        }

        LogMessage( const LogMessage &src )
        {
            Copy( src );
        }

        void Copy( const LogMessage &src )
        {
            m_HostName        = src.m_HostName       ;
            m_Application     = src.m_Application    ;
            m_PID             = src.m_PID            ;
            m_TID             = src.m_TID            ;
            m_Thread          = src.m_Thread         ;
            m_Filename        = src.m_Filename       ;
            m_Line            = src.m_Line           ;
            m_Function        = src.m_Function       ;
            m_Time            = src.m_Time           ;
            m_Level           = src.m_Level          ;
            m_Message         = src.m_Message        ;
        }
};

} // namespace DTC

Q_DECLARE_METATYPE( DTC::LogMessage )
Q_DECLARE_METATYPE( DTC::LogMessage* )

#endif
