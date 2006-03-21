/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from dsmcc by Richard Palmer 
 */
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>

#include "dsmccobjcarousel.h"
#include "dsmccreceiver.h"
#include "dsmcccache.h"
#include "dsmcc.h"

#include "mythcontext.h"

// Construct the data for a new module from a DII message.
DSMCCCacheModuleData::DSMCCCacheModuleData(DsmccDii *dii,
                                           DsmccModuleInfo *info,
                                           unsigned short streamTag)
    : m_carousel_id(dii->download_id), m_module_id(info->module_id),
      m_stream_id(streamTag),          m_version(info->module_version),
      m_moduleSize(info->module_size), m_receivedData(0),
      m_completed(false)
{
    m_blocks.setAutoDelete(true);

    // The number of blocks needed to hold this module.
    int num_blocks = (m_moduleSize + dii->block_size - 1) / dii->block_size;
    m_blocks.fill(NULL, num_blocks); // Set it to all zeros

    // Copy the descriptor information.
    m_descriptorData = info->modinfo.descriptorData;
}

/** \fn DSMCCCacheModuleData::AddModuleData(DsmccDb*,const unsigned char*)
 *  \brief Add block to the module and create the module if it's now complete.
 *  \return data for the module if it is complete, NULL otherwise.
 */
unsigned char *DSMCCCacheModuleData::AddModuleData(DsmccDb *ddb,
                                                   const unsigned char *data)
{
    if (m_version != ddb->module_version)
        return NULL; // Wrong version

    if (m_completed)
        return NULL; // Already got it.

    // Check if we have this block already or not. If not append to list
    VERBOSE(VB_DSMCC, QString("[dsmcc] Module %1 block number %2 length %3")
            .arg(ddb->module_id).arg(ddb->block_number).arg(ddb->len));

    if (ddb->block_number >= m_blocks.size())
    {
        VERBOSE(VB_DSMCC, QString("[dsmcc] Module %1 block number %2 "
                                  "is larger than %3")
                .arg(ddb->module_id).arg(ddb->block_number)
                .arg(m_blocks.size()));

        return NULL;
    }

    if (m_blocks[ddb->block_number] == NULL)
    {   // We haven't seen this block before.
        QByteArray *block = new QByteArray;
        block->duplicate((char*) data, ddb->len);
        // Add this to our set of blocks.
        m_blocks.insert(ddb->block_number, block);
        m_receivedData += ddb->len;
    }

    VERBOSE(VB_DSMCC, QString("[dsmcc] Module %1 Current Size %2 "
                              "Total Size %3")
            .arg(m_module_id).arg(m_receivedData).arg(m_moduleSize));

    if (m_receivedData < m_moduleSize)
        return NULL; // Not yet complete

    VERBOSE(VB_DSMCC, QString("[dsmcc] Reconstructing module %1 from blocks")
            .arg(m_module_id));

    // Re-assemble the blocks into the complete module.
    unsigned char *tmp_data = (unsigned char*) malloc(m_receivedData);
    if (tmp_data == NULL)
        return NULL;

    uint curp = 0;
    for (uint i = 0; i < m_blocks.size(); i++)
    {
        QByteArray *block = m_blocks[i];
        uint size = block->size();
        memcpy(tmp_data + curp, block->data(), size);
        curp += size;
    }
    m_blocks.clear(); // No longer required: free the space.

    /* Uncompress....  */
    if (m_descriptorData.isCompressed)
    {
        unsigned long dataLen = m_descriptorData.originalSize + 1;
        VERBOSE(VB_DSMCC, QString("[dsmcc] uncompressing: "
                                  "compressed size %1, final size %2")
                .arg(m_moduleSize).arg(dataLen));

        unsigned char *uncompressed = (unsigned char*) malloc(dataLen + 1);
        int ret = uncompress(uncompressed, &dataLen, tmp_data, m_moduleSize);
        if (ret != Z_OK)
        {
            VERBOSE(VB_DSMCC,"[dsmcc] compression error, skipping");
            free(tmp_data);
            free(uncompressed);
            return NULL;
        }

        free(tmp_data);
        m_completed = true;
        return uncompressed;
    }
    else
    {
        m_completed = true;
        return tmp_data;
    }
}


ObjCarousel::ObjCarousel(Dsmcc *dsmcc)
    : filecache(dsmcc), m_id(0)
{
    m_Cache.setAutoDelete(true);
}

void ObjCarousel::AddModuleInfo(DsmccDii *dii, Dsmcc *status,
                                unsigned short streamTag)
{
    for (int i = 0; i < dii->number_modules; i++)
    {
        DSMCCCacheModuleData *cachep;
        DsmccModuleInfo *info = &(dii->modules[i]);
        // Do we already know this module?
        // If so and it is the same version we don't need to do anything.
        // If the version has changed we have to replace it.
        for (cachep = m_Cache.first(); cachep; cachep = m_Cache.next())
        {
            if (cachep->CarouselId() == dii->download_id &&
                    cachep->ModuleId() == info->module_id)
            {
                /* already known */
                if (cachep->Version() == info->module_version)
                {
                    VERBOSE(VB_DSMCC, QString("[dsmcc] Already Know "
                                              "Module %1")
                            .arg(info->module_id));

                    return;
                }
                else
                {
                    // Version has change - Drop old data.
                    VERBOSE(VB_DSMCC, QString("[dsmcc] Updated "
                                              "Module %1")
                            .arg(info->module_id));

                    // Remove and delete the cache object.
                    m_Cache.remove();
                    break;
                }
            }
        }

        VERBOSE(VB_DSMCC, QString("[dsmcc] Saving info for module %1")
                .arg(dii->modules[i].module_id));

        // Create a new cache module data object.
        cachep = new DSMCCCacheModuleData(dii, info, streamTag);

        int tag = info->modinfo.tap.assoc_tag;
        VERBOSE(VB_DSMCC, QString("[dsmcc] Module info tap "
                                  "identifies tag %1 with carousel %2\n")
                .arg(tag).arg(cachep->CarouselId()));

        // If a carousel with this id does not exist create it.
        status->AddTap(tag, cachep->CarouselId());

        // Add this module to the cache.
        m_Cache.append(cachep);
    }
}

/** \fn ObjCarousel::AddModuleData(unsigned long,DsmccDb*,const unsigned char*)
 *  \brief We have received a block for a module.
 *
 *   Add it to the module and process the module if it's now complete.
 */
void ObjCarousel::AddModuleData(unsigned long carousel, DsmccDb *ddb,
                                const unsigned char *data)
{
    VERBOSE(VB_DSMCC, QString("[dsmcc] Data block on carousel %1").arg(m_id));

    // Search the saved module info for this module
    QPtrListIterator<DSMCCCacheModuleData> it(m_Cache);
    DSMCCCacheModuleData *cachep;
    for (; (cachep = it.current()) != 0; ++it)
    {
        if (cachep->CarouselId() == carousel &&
            (cachep->ModuleId() == ddb->module_id))
        {
            break;
        }
    }

    if (cachep == NULL)
        return; // Not found module info.

    // Add the block to the module
    unsigned char *tmp_data = cachep->AddModuleData(ddb, data);

    if (tmp_data)
    {
        // It is complete and we have the data
        unsigned int len   = cachep->DataSize();
        unsigned long curp = 0;
        VERBOSE(VB_DSMCC, QString("[biop] Module size (uncompressed) = %1")
                .arg(len));

        // Now process the BIOP tables in this module.
        // Tables may be file contents or the descriptions of
        // directories or service gateways (root directories).
        while (curp < len)
        {
            BiopMessage bm;
            if (!bm.Process(cachep, &filecache, tmp_data, &curp))
                break;
        }
        free(tmp_data);
    }
}
