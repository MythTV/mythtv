//////////////////////////////////////////////////////////////////////////////
// Program Name: captureServiceHost.h
// Created     : Sep. 21, 2011
//
// Purpose - Capture Device Service Host
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef CAPTURESERVICEHOST_H_
#define CAPTURESERVICEHOST_H_

#include "servicehost.h"
#include "services/capture.h"

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
//
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class CaptureServiceHost : public ServiceHost
{
    public:

        CaptureServiceHost( const QString &sSharePath )
                : ServiceHost( Capture::staticMetaObject,
                               "Capture",
                               "/Capture",
                               sSharePath )
        {
        }

        virtual ~CaptureServiceHost()
        {
        }
};

#endif
