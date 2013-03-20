//////////////////////////////////////////////////////////////////////////////
// Program Name: contentServiceHost.h
// Created     : Jul. 27, 2012
//
// Purpose - Content Service Host
//
// Copyright (c) 2012 Robert Siebert <trebor_s@web.de>
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

#ifndef IMAGESERVICEHOST_H_
#define IMAGESERVICEHOST_H_

#include "servicehost.h"
#include "services/image.h"



class ImageServiceHost : public ServiceHost
{
    public:

        ImageServiceHost( const QString &sSharePath )
             : ServiceHost( Image::staticMetaObject,
                            "Image",
                            "/Image",
                            sSharePath )
        {
        }

        virtual ~ImageServiceHost()
        {
        }
};

#endif
