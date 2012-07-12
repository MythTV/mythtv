//////////////////////////////////////////////////////////////////////////////
// Program Name: videoServiceHost.h
// Created     : Apr. 21, 2011
//
// Purpose - Imported Video Service Host
//
// Copyright (c) 2011 Robert McNamara <rmcnamara@mythtv.org>
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

#ifndef VIDEOSERVICEHOST_H_
#define VIDEOSERVICEHOST_H_

#include "servicehost.h"
#include "services/video.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class VideoServiceHost : public ServiceHost
{
    public:

        VideoServiceHost( const QString &sSharePath )
                : ServiceHost( Video::staticMetaObject,
                               "Video",
                               "/Video",
                               sSharePath )
        {
        }

        virtual ~VideoServiceHost()
        {
        }
};

#endif
