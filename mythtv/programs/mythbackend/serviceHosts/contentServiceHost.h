//////////////////////////////////////////////////////////////////////////////
// Program Name: contentServiceHost.h
// Created     : Mar. 7, 2011
//
// Purpose - Content Service Host 
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

#ifndef CONTENTSERVICEHOST_H_
#define CONTENTSERVICEHOST_H_

#include "servicehost.h"
#include "services/content.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class ContentServiceHost : public ServiceHost
{
    public:

        ContentServiceHost( const QString &sSharePath )
             : ServiceHost( Content::staticMetaObject, 
                            "Content", 
                            "/Content",
                            sSharePath )
        {
        }

        virtual ~ContentServiceHost()
        {
        }
};

#endif
