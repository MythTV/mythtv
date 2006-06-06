//////////////////////////////////////////////////////////////////////////////
// Program Name: refcounted.h
//                                                                            
// Purpose - Reference Counted Base Class
//                                                                            
// Created By  : David Blain	                Created On : Oct. 21, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#ifndef __REFCOUNTED_H__
#define __REFCOUNTED_H__

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
