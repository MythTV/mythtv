//////////////////////////////////////////////////////////////////////////////
// Program Name: guide.cpp
// Created     : Mar. 7, 2011
//
// Copyright (c) 2011 David Blain <dblain@mythtv.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include <math.h>

#include "guide.h"

#include "compat.h"
#include "mythversion.h"
#include "mythcorecontext.h"
#include "scheduler.h"
#include "autoexpire.h"
#include "channelutil.h"

extern AutoExpire  *expirer;
extern Scheduler   *sched;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::ProgramGuide *Guide::GetProgramGuide( const QDateTime &rawStartTime ,
                                           const QDateTime &rawEndTime   ,
                                           int              nStartChanId,
                                           int              nNumChannels,
                                           bool             bDetails      )
{     
    if (!rawStartTime.isValid())
        throw( "StartTime is invalid" );

    if (!rawEndTime.isValid())
        throw( "EndTime is invalid" );

    QDateTime dtStartTime = rawStartTime.toUTC();
    QDateTime dtEndTime = rawEndTime.toUTC();

    if (dtEndTime < dtStartTime)
        throw( "EndTime is before StartTime");

    if (nNumChannels == 0)
        nNumChannels = SHRT_MAX;

    // ----------------------------------------------------------------------
    // Find the ending channel Id
    // ----------------------------------------------------------------------

    int nEndChanId = nStartChanId;

    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare( "SELECT chanid FROM channel WHERE (chanid >= :STARTCHANID )"
                   " ORDER BY chanid LIMIT :NUMCHAN" );

    query.bindValue(":STARTCHANID", nStartChanId );
    query.bindValue(":NUMCHAN"    , nNumChannels );

    if (!query.exec())
        MythDB::DBError("Select ChanId", query);

    query.first();  nStartChanId = query.value(0).toInt();
    query.last();   nEndChanId   = query.value(0).toInt();

    // ----------------------------------------------------------------------
    // Build SQL statement for Program Listing
    // ----------------------------------------------------------------------

    ProgramList  progList;
    ProgramList  schedList;
    MSqlBindings bindings;

    QString      sSQL = "WHERE program.chanid >= :StartChanId "
                         "AND program.chanid <= :EndChanId "
                         "AND program.endtime >= :StartDate "
                         "AND program.starttime <= :EndDate "
                        "GROUP BY program.starttime, channel.channum, "
                         "channel.callsign, program.title "
                        "ORDER BY program.chanid ";

    bindings[":StartChanId"] = nStartChanId;
    bindings[":EndChanId"  ] = nEndChanId;
    bindings[":StartDate"  ] = dtStartTime;
    bindings[":EndDate"    ] = dtEndTime;

    // ----------------------------------------------------------------------
    // Get all Pending Scheduled Programs
    // ----------------------------------------------------------------------

    bool hasConflicts;
    LoadFromScheduler(schedList, hasConflicts);

    // ----------------------------------------------------------------------

    LoadFromProgram( progList, sSQL, bindings, schedList );

    // ----------------------------------------------------------------------
    // Build Response
    // ----------------------------------------------------------------------

    DTC::ProgramGuide *pGuide = new DTC::ProgramGuide();

    int               nChanCount = 0;
    uint              nCurChanId = 0;
    DTC::ChannelInfo *pChannel   = NULL;

    for( uint n = 0; n < progList.size(); n++)
    {
        ProgramInfo *pInfo = progList[ n ];

        if ( nCurChanId != pInfo->GetChanID() )
        {
            nChanCount++;

            nCurChanId = pInfo->GetChanID();

            pChannel = pGuide->AddNewChannel();

            FillChannelInfo( pChannel, pInfo, bDetails );
        }

        
        DTC::Program *pProgram = pChannel->AddNewProgram();

        FillProgramInfo( pProgram, pInfo, false, bDetails );
    }

    // ----------------------------------------------------------------------

    pGuide->setStartTime    ( dtStartTime   );
    pGuide->setEndTime      ( dtEndTime     );
    pGuide->setStartChanId  ( nStartChanId  );
    pGuide->setEndChanId    ( nEndChanId    );
    pGuide->setNumOfChannels( nChanCount    );
    pGuide->setDetails      ( bDetails      );
    
    pGuide->setCount        ( progList.size());
    pGuide->setAsOf         ( MythDate::current() );
    
    pGuide->setVersion      ( MYTH_BINARY_VERSION );
    pGuide->setProtoVer     ( MYTH_PROTO_VERSION  );
    
    return pGuide;
}
 
/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

DTC::Program* Guide::GetProgramDetails( int              nChanId,
                                        const QDateTime &rawStartTime )
                                          
{
    if (!rawStartTime.isValid())
        throw( "StartTime is invalid" );

    QDateTime dtStartTime = rawStartTime.toUTC();

    // ----------------------------------------------------------------------
    // -=>TODO: Add support for getting Recorded Program Info
    // ----------------------------------------------------------------------

    // Build add'l SQL statement for Program Listing

    MSqlBindings bindings;
    QString      sSQL = "WHERE program.chanid = :ChanId "
                          "AND program.starttime = :StartTime ";

    bindings[":ChanId"   ] = nChanId;
    bindings[":StartTime"] = dtStartTime;

    // Get all Pending Scheduled Programs

    ProgramList  schedList;
    bool hasConflicts;
    LoadFromScheduler(schedList, hasConflicts);

    // ----------------------------------------------------------------------

    ProgramList progList;

    LoadFromProgram( progList, sSQL, bindings, schedList );

    if ( progList.size() == 0)
        throw( "Error Reading Program Info" );

    // Build Response

    DTC::Program *pProgram = new DTC::Program();
    ProgramInfo  *pInfo    = progList[ 0 ];

    FillProgramInfo( pProgram, pInfo, true );

    return pProgram;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

QFileInfo Guide::GetChannelIcon( int nChanId,
                                 int nWidth  /* = 0 */,
                                 int nHeight /* = 0 */ )
{
    // Get Icon file path

    QString sFileName = ChannelUtil::GetIcon( nChanId );

    if (sFileName.isEmpty())
        return QFileInfo();

    if ((nWidth <= 0) && (nHeight <= 0))
    {
        // Use default pixmap
        return QFileInfo( sFileName );  
    }


    QString sNewFileName = QString( "%1.%2x%3.png" )
                              .arg( sFileName )
                              .arg( nWidth    )
                              .arg( nHeight   );

    // ----------------------------------------------------------------------
    // check to see if image is already created.
    // ----------------------------------------------------------------------

    if (QFile::exists( sNewFileName ))
        return QFileInfo( sNewFileName );

    // ----------------------------------------------------------------------
    // We need to create it...
    // ----------------------------------------------------------------------

    float fAspect = 0.0;

    QImage *pImage = new QImage( sFileName );

    if (!pImage)
        return QFileInfo();

    if (fAspect <= 0)
           fAspect = (float)(pImage->width()) / pImage->height();

    if (fAspect == 0)
    {
        delete pImage;
        return QFileInfo();
    }

    if ( nWidth == 0 )
        nWidth = (int)rint(nHeight * fAspect);

    if ( nHeight == 0 )
        nHeight = (int)rint(nWidth / fAspect);

    QImage img = pImage->scaled( nWidth, nHeight, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation);

    img.save( sNewFileName, "PNG" );

    delete pImage;

    return QFileInfo( sNewFileName );
}

