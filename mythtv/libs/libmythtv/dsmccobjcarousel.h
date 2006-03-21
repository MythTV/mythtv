/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer 
 */
#ifndef DSMCC_OBJCAROUSEL_H
#define DSMCC_OBJCAROUSEL_H


#include <qptrlist.h>
#include <qvaluevector.h>
#include <qcstring.h>
#include <qptrvector.h>

class DsmccDii;
class Dsmcc;
class DsmccDb;
class DsmccModuleInfo;

#include "dsmcccache.h"
#include "dsmccbiop.h" // For ModuleDescriptorData

/** \class DSMCCCacheModuleData
 *  \brief DSMCCCacheModuleData contains information about a module
 *         and holds the blocks for a partly completed module.
 */
class DSMCCCacheModuleData
{
  public:
    DSMCCCacheModuleData(DsmccDii *dii, DsmccModuleInfo *info,
                    unsigned short streamTag);

    unsigned char *AddModuleData(DsmccDb *ddb, const unsigned char *Data);

    unsigned long  CarouselId(void) const { return m_carousel_id; }
    unsigned short ModuleId(void)   const { return m_module_id;   }
    unsigned short StreamId(void)   const { return m_stream_id;   }
    unsigned char  Version(void)    const { return m_version;     }

    /// Return the, possibly uncompressed, module size
    unsigned long  DataSize(void) const
    {
        return (m_descriptorData.isCompressed) ?
            m_descriptorData.originalSize : m_moduleSize;
    }


  private:
    unsigned long  m_carousel_id;
    unsigned short m_module_id;
    unsigned short m_stream_id;

    unsigned char  m_version;
    unsigned long  m_moduleSize;   ///< Total size
    unsigned long  m_receivedData; ///< Size received so far.

    /// Block table.  As blocks are received they are added to this table. 
    QPtrVector<QByteArray> m_blocks;
    /// True if we have completed this module.
    bool                   m_completed;
    ModuleDescriptorData   m_descriptorData;
};

class ObjCarousel
{
  public:
    ObjCarousel(Dsmcc*);
    void AddModuleInfo(DsmccDii *dii, Dsmcc *status, unsigned short streamTag);
    void AddModuleData(unsigned long carousel, DsmccDb *ddb,
                       const unsigned char *data);

    DSMCCCache                     filecache;
    QPtrList<DSMCCCacheModuleData> m_Cache;
    /// Component tags matched to this carousel.
    QValueVector<unsigned short>   m_Tags;
    unsigned long                  m_id;
};

#endif

