//////////////////////////////////////////////////////////////////////////////
// Program Name: guide.h
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef GUIDE_H
#define GUIDE_H

#include <QScriptEngine>

#include "serviceUtil.h"

#include "services/guideServices.h"

#include "datacontracts/programAndChannel.h"
#include "programinfo.h"

class Guide : public GuideServices
{
    Q_OBJECT

    public:
    
        Q_INVOKABLE Guide( QObject *parent = 0 ) {}

    public:

        
        DTC::ProgramGuide*  GetProgramGuide     ( const QDateTime &StartTime  ,
                                                  const QDateTime &EndTime    ,
                                                  int              StartChanId,
                                                  int              NumChannels,
                                                  bool             Details     );

        DTC::Program*       GetProgramDetails   ( int              ChanId,
                                                  const QDateTime &StartTime );

        QFileInfo           GetChannelIcon      ( int              ChanId,
                                                  int              Width ,
                                                  int              Height );
};

// --------------------------------------------------------------------------
// The following class wrapper is due to a limitation in Qt Script Engine.  It
// requires all methods that return pointers to user classes that are derived from
// QObject actually return QObject* (not the user class *).  If the user class pointer
// is returned, the script engine treats it as a QVariant and doesn't create a
// javascript prototype wrapper for it.
// 
// This class allows us to keep the rich return types in the main API class while
// offering the script engine a class it can work with.
// 
// Only API Classes that return custom classes needs to implement these wrappers.
//
// We should continue to look for a cleaning solution to this problem.
// --------------------------------------------------------------------------

class ScriptableGuide : public QObject
{
    Q_OBJECT

    private:

        Guide    m_obj;

    public:
    
        Q_INVOKABLE ScriptableGuide( QObject *parent = 0 ) : QObject( parent ) {}

    public slots:

        QObject* GetProgramGuide( const QDateTime &StartTime  ,
                                  const QDateTime &EndTime    ,
                                  int              StartChanId,
                                  int              NumChannels,
                                  bool             Details     )
        {
            return m_obj.GetProgramGuide( StartTime, EndTime, StartChanId, NumChannels, Details );
        }

        QObject* GetProgramDetails( int ChanId, const QDateTime &StartTime )
        {
            return m_obj.GetProgramDetails( ChanId, StartTime );
        }

        QFileInfo GetChannelIcon( int ChanId, int Width, int Height )
        {
            return m_obj.GetChannelIcon( ChanId, Width, Height );
        }
};

Q_SCRIPT_DECLARE_QMETAOBJECT( ScriptableGuide, QObject*);

#endif 
