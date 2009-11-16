//////////////////////////////////////////////////////////////////////////////
// Program Name: refcounted.h
// Created     : Oct. 21, 2005
//
// Purpose     : Reference Counted Base Class
//                                                                            
// Copyright (c) 2005 David Blain <mythtv@theblains.net>
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

#ifndef __REFCOUNTED_H__
#define __REFCOUNTED_H__

#include <QMutex>

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// RefCounted Class Definition/Implementation
// 
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

class RefCounted
{
    protected:

        long        m_nRefCount;
        QMutex      m_mutex;

    protected:

        // Destructor protected to force use of release;

        virtual ~RefCounted() {};

    public:

        // ------------------------------------------------------------------

        RefCounted() : m_nRefCount( 0 ) 
        {
        }

        // ------------------------------------------------------------------
        
        long AddRef()
        { 
            m_mutex.lock(); 
            long nRef = (++m_nRefCount);
            m_mutex.unlock();
    
            return( nRef );
        }

        // ------------------------------------------------------------------

        long Release() 
        { 

            m_mutex.lock(); 
            long nRef = (--m_nRefCount);
            m_mutex.unlock();

            if (nRef <= 0)
                delete this;

            return( nRef );
        }
};

#endif
