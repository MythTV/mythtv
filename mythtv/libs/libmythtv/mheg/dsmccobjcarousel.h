/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer 
 */
#ifndef DSMCC_OBJCAROUSEL_H
#define DSMCC_OBJCAROUSEL_H

#include <list>
#include <vector>

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
    ~DSMCCCacheModuleData();

    unsigned char *AddModuleData(DsmccDb *ddb, const unsigned char *Data);

    unsigned long  CarouselId(void) const { return m_carouselId; }
    unsigned short ModuleId(void)   const { return m_moduleId;   }
    unsigned short StreamId(void)   const { return m_streamId;   }
    unsigned char  Version(void)    const { return m_version;     }
    unsigned long  ModuleSize(void) const { return m_moduleSize;  }

    /// Return the, possibly uncompressed, module size
    unsigned long  DataSize(void) const
    {
        return (m_descriptorData.m_isCompressed) ?
            m_descriptorData.m_originalSize : m_moduleSize;
    }


  private:
    unsigned long  m_carouselId;
    unsigned short m_moduleId;
    unsigned short m_streamId;

    unsigned char  m_version;
    unsigned long  m_moduleSize;   ///< Total size
    unsigned long  m_receivedData {0}; ///< Size received so far.

    /// Block table.  As blocks are received they are added to this table. 
    std::vector<QByteArray*> m_blocks;
    /// True if we have completed this module.
    bool                   m_completed {false};
    ModuleDescriptorData   m_descriptorData;
};

class ObjCarousel
{
  public:
    explicit ObjCarousel(Dsmcc*dsmcc) : m_fileCache(dsmcc) {}
    ~ObjCarousel();
    void AddModuleInfo(DsmccDii *dii, Dsmcc *status, unsigned short streamTag);
    void AddModuleData(DsmccDb *ddb, const unsigned char *data);

    DSMCCCache                         m_fileCache;
    std::list<DSMCCCacheModuleData*>   m_Cache;
    /// Component tags matched to this carousel.
    std::vector<unsigned short>        m_Tags;
    unsigned long                      m_id        {0};
};

#endif

