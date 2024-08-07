/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer
 */
#include <cstdlib>
#include <cstring>

#include "libmythbase/mythlogging.h"

#include "dsmccbiop.h"
#include "dsmccreceiver.h"
#include "dsmcccache.h"
#include "dsmccobjcarousel.h"
#include "dsmcc.h"

BiopNameComp::~BiopNameComp()
{
    if (m_id)
        free(m_id);
    if (m_kind)
        free(m_kind);
}

int BiopNameComp::Process(const unsigned char *data)
{
    int off = 0;

    m_idLen     = data[off++];
    m_id        = (char*) malloc(m_idLen);
    memcpy(m_id, data + off, m_idLen);

    off        += m_idLen;
    m_kindLen   = data[off++];
    m_kind      = (char*) malloc(m_kindLen);
    memcpy(m_kind, data + off, m_kindLen);

    off        += m_kindLen;

    return off;
}

BiopName::~BiopName()
{
    delete[] m_comps;
}

int BiopName::Process(const unsigned char *data)
{
    int off = 0;
    m_compCount = data[0];

    if (m_compCount != 1)
        LOG(VB_DSMCC, LOG_WARNING, "[biop] Expected one name");

    off++;
    m_comps = new BiopNameComp[m_compCount];

    for (int i = 0; i < m_compCount; i++)
    {
        int ret = m_comps[i].Process(data + off);
        if (ret <= 0)
            return ret;
        off += ret;
    }

    return off;
}

int BiopBinding::Process(const unsigned char *data)
{
    int off = 0;
    int ret = m_name.Process(data);

    if (ret > 0)
        off += ret;
    else
        return ret; // Error

    m_bindingType = data[off++];
    ret = m_ior.Process(data + off);

    if (ret > 0)
        off += ret;
    else
        return ret; // Error

    m_objInfoLen = (data[off] << 8) | data[off + 1];
    off += 2;

    if (m_objInfoLen > 0)
    {
        m_objInfo = (char*) malloc(m_objInfoLen);
        memcpy(m_objInfo, data + off, m_objInfoLen);
    }
    else
    {
        m_objInfo = nullptr;
    }

    off += m_objInfoLen;

    return off;
}

BiopBinding::~BiopBinding()
{
    free(m_objInfo);
}

bool BiopMessage::Process(DSMCCCacheModuleData *cachep, DSMCCCache *filecache,
                          unsigned char *data, unsigned long *curp)
{
    // Parse header
    if (! ProcessMsgHdr(data, curp))
    {
        LOG(VB_DSMCC, LOG_ERR,
            "[biop] Invalid biop header, dropping rest of module");

        /* not valid, skip rest of data */
        return false;
    }

    // Handle each message type
    if (strcmp(m_objKind, "fil") == 0)
    {
        LOG(VB_DSMCC, LOG_DEBUG, "[biop] Processing file");
        return ProcessFile(cachep, filecache, data, curp);
    }
    if (strcmp(m_objKind, "dir") == 0)
    {
        LOG(VB_DSMCC, LOG_DEBUG, "[biop] Processing directory");
        return ProcessDir(false, cachep, filecache, data, curp);
    }
    if (strcmp(m_objKind, "srg") == 0)
    {
        LOG(VB_DSMCC, LOG_DEBUG, "[biop] Processing gateway");
        return ProcessDir(true, cachep, filecache, data, curp);
    }

    /* Error */
    LOG(VB_DSMCC, LOG_WARNING, QString("Unknown or unsupported format %1%2%3%4")
        .arg(m_objKind[0]).arg(m_objKind[1])
        .arg(m_objKind[2]).arg(m_objKind[3]));
    return false;
}

BiopMessage::~BiopMessage()
{
    free(m_objInfo);
    free(m_objKind);
}

bool BiopMessage::ProcessMsgHdr(const unsigned char *data, unsigned long *curp)
{
    const unsigned char *buf = data + (*curp);
    int off = 0;

    if (buf[off] !='B' || buf[off +1] !='I' || buf[off +2] !='O' || buf[off +3] !='P')
    {
        LOG(VB_DSMCC, LOG_WARNING, "BiopMessage - invalid header");
        return false;
    }
    off += 4;

    m_versionMajor = buf[off++];
    m_versionMinor = buf[off++];
    if (m_versionMajor != 1 || m_versionMinor != 0)
    {
        LOG(VB_DSMCC, LOG_WARNING, "BiopMessage invalid version");
        return false;
    }

    if (buf[off++] != 0)
    {
        LOG(VB_DSMCC, LOG_WARNING, "BiopMessage invalid byte order");
        return false;
    }
    if (buf[off++] != 0)
    {
        LOG(VB_DSMCC, LOG_WARNING, "BiopMessage invalid message type");
        return false;
    }

    m_messageSize = COMBINE32(buf, off);
    off += 4;

    uint nObjLen = buf[off++];
    m_objKey = DSMCCCacheKey((const char*)buf + off, nObjLen);
    off += nObjLen;

    m_objKindLen = COMBINE32(buf, off);
    off += 4;
    m_objKind = (char*) malloc(m_objKindLen);
    memcpy(m_objKind, buf + off, m_objKindLen);
    off += m_objKindLen;

    m_objInfoLen = buf[off] << 8 | buf[off + 1];
    off += 2;
    m_objInfo = (char*) malloc(m_objInfoLen);
    memcpy(m_objInfo, buf + off, m_objInfoLen);
    off += m_objInfoLen;

    (*curp) += off;

    return true;
}


/** \fn BiopMessage::ProcessDir(bool,DSMCCCacheModuleData*,DSMCCCache*,unsigned char*,unsigned long*)
 * \brief Process a Directory message.
 *
 *  This gives the directories and files that form part of this directory.
 *  This is also used for service gateways which identify a root of the
 *  directory structure and is very similar to a Directory message.
 *  We only know which is THE root of the hierarchy when we have a DSI message.
 */
bool BiopMessage::ProcessDir(
    bool isSrg, DSMCCCacheModuleData *cachep, DSMCCCache *filecache,
    const unsigned char *data, unsigned long *curp)
{
    int off = 0;
    const unsigned char * const buf = data + (*curp);

    if (m_objInfoLen)
        LOG(VB_DSMCC, LOG_WARNING, "[biop] ProcessDir non-zero objectInfo_length");

    const unsigned serviceContextList_count = buf[off++];
    if (serviceContextList_count)
    {
        // TODO Handle serviceContextList for service gateway
        LOG(VB_DSMCC, LOG_WARNING, QString("[biop] ProcessDir serviceContextList count %1")
            .arg(serviceContextList_count));
        return false; // Error
    }

    unsigned long msgbody_len = COMBINE32(buf, off);
    off += 4;
    int const start = off;

    unsigned int bindings_count = buf[off] << 8 | buf[off + 1];
    off += 2;

    DSMCCCacheReference ref(cachep->CarouselId(), cachep->ModuleId(),
                            cachep->StreamId(), m_objKey);
    DSMCCCacheDir *pDir = isSrg ? filecache->Srg(ref) : filecache->Directory(ref);

    for (uint i = 0; i < bindings_count; i++)
    {
        BiopBinding binding;
        int ret = binding.Process(buf + off);
        if (ret > 0)
            off += ret;
        else
            return false; // Error

        if (binding.m_name.m_compCount != 1)
            LOG(VB_DSMCC, LOG_WARNING, "[biop] ProcessDir nameComponents != 1");

        if (binding.m_bindingType != 1 && binding.m_bindingType != 2)
            LOG(VB_DSMCC, LOG_WARNING, "[biop] ProcessDir invalid BindingType");

        // Process any taps in this binding.
        binding.m_ior.AddTap(filecache->m_dsmcc);

        if (pDir && binding.m_name.m_compCount >= 1)
        {
            if (strcmp("fil", binding.m_name.m_comps[0].m_kind) == 0)
                DSMCCCache::AddFileInfo(pDir, &binding);
            else if (strcmp("dir", binding.m_name.m_comps[0].m_kind) == 0)
                DSMCCCache::AddDirInfo(pDir, &binding);
            else
                LOG(VB_DSMCC, LOG_WARNING, QString("[biop] ProcessDir unknown kind %1")
                    .arg(binding.m_name.m_comps[0].m_kind));
        }
    }

    if ((unsigned)(off - start) != msgbody_len)
        LOG(VB_DSMCC, LOG_WARNING, "[biop] ProcessDir incorrect msgbody_len");

    (*curp) += off;

    return true;
}

bool BiopMessage::ProcessFile(DSMCCCacheModuleData *cachep, DSMCCCache *filecache,
                              unsigned char *data, unsigned long *curp)
{
    int off = 0;
    const unsigned char *buf = data + (*curp);

    if (m_objInfoLen != 8)
        LOG(VB_DSMCC, LOG_WARNING, QString("[biop] ProcessFile objectInfo_length = %1")
            .arg(m_objInfoLen));

    const unsigned serviceContextList_count = buf[off++];
    if (serviceContextList_count)
    {
        LOG(VB_DSMCC, LOG_WARNING,
            QString("[biop] ProcessFile Unexpected serviceContextList_count %1")
            .arg(serviceContextList_count));
        return false; // Error
    }

    unsigned long msgbody_len = COMBINE32(buf, off);
    off += 4;
    unsigned long content_len = COMBINE32(buf, off);
    off += 4;
    if (content_len + 4 != msgbody_len)
        LOG(VB_DSMCC, LOG_WARNING, "[biop] ProcessFile incorrect msgbody_len");

    (*curp) += off;

    DSMCCCacheReference ref(cachep->CarouselId(), cachep->ModuleId(),
                            cachep->StreamId(), m_objKey);

    QByteArray filedata = QByteArray((const char *)data+(*curp), content_len);
    filecache->CacheFileData(ref, filedata);

    (*curp) += content_len;
    return true;
}

void ModuleDescriptorData::Process(const unsigned char *data, int length)
{
    while (length > 0)
    {
        unsigned char tag = *data++;
        unsigned char len = *data++;
        length -= 2;

        // Tags:
        //   case 0x01: // Type
        //   case 0x02: // Name
        //   case 0x03: // Info
        //   case 0x04: // Modlink
        //   case 0x05: // CRC
        //   case 0x06: // Location
        //   case 0x07: // DLtime
        //   case 0x08: // Grouplink
        //   case 0x09: // Compressed.
        if (tag == 0x09)
        {
                // Skip the method.
                m_isCompressed = true;
                m_originalSize = COMBINE32(data, 1);
        }

        length -= len;
        data += len;
    }
}

int BiopModuleInfo::Process(const unsigned char *data)
{
    m_modTimeout   = COMBINE32(data, 0);
    m_blockTimeout = COMBINE32(data, 4);
    m_minBlockTime = COMBINE32(data, 8);

    m_tapsCount = data[12];
    int off = 13;

    LOG(VB_DSMCC, LOG_DEBUG, QString("[Biop] "
        "ModuleTimeout %1 BlockTimeout %2 MinBlockTime %3 Taps %4")
        .arg(m_modTimeout).arg(m_blockTimeout).arg(m_minBlockTime)
        .arg(m_tapsCount));

    if (m_tapsCount > 0)
    {
        /* only 1 allowed TODO - may not be first though ? */
        int ret = m_tap.Process(data + off);
        if (ret <= 0)
            return ret;
        off += ret;
    }

    unsigned userinfo_len = data[off++];

    if (userinfo_len > 0)
    {
        m_descriptorData.Process(data + off, userinfo_len);
        off += userinfo_len;
    }
    return off;

}

int BiopTap::Process(const unsigned char *data)
{
    int off=0;

    m_id = (data[off] << 8) | data[off + 1]; // Ignored
    off += 2;
    m_use = (data[off] << 8) | data[off + 1];
    off += 2;
    m_assocTag = (data[off] << 8) | data[off + 1];
    off += 2;
    m_selectorLen = data[off++];
    m_selectorData = (char*) malloc(m_selectorLen);
    memcpy(m_selectorData, data + off, m_selectorLen);
    if (m_use == 0x0016) // BIOP_DELIVERY_PARA_USE
    {
        unsigned selector_type = (data[off] << 8) | data[off + 1];
        if (m_selectorLen >= 10 && selector_type == 0x0001)
        {
            off += 2;
            unsigned long transactionId = COMBINE32(data, off);
            off += 4;
            unsigned long timeout = COMBINE32(data, off);
            LOG(VB_DSMCC, LOG_DEBUG, QString("[biop] BIOP_DELIVERY_PARA_USE tag %1 id 0x%2 timeout %3uS")
                .arg(m_assocTag).arg(transactionId,0,16).arg(timeout));
            off += 4;
            m_selectorLen -= 10;
        }
    }

    off += m_selectorLen;
    return off;
}

int BiopConnbinder::Process(const unsigned char *data)
{
    int off = 0;

    m_componentTag = COMBINE32(data, 0);
    if (0x49534F40 != m_componentTag)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] Invalid Connbinder tag");
        return 0;
    }
    off += 4;
    m_componentDataLen = data[off++];
    m_tapsCount = data[off++];
    if (m_tapsCount > 0)
    {
        /* UKProfile - only first tap read */
        int ret = m_tap.Process(data + off);
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, QString("Binder - assoc_tag %1")
                                       .arg(m_tap.m_assocTag));
#endif
        if (ret > 0)
            off += ret;
        /* else TODO error */
    }

    return off;
}

int BiopObjLocation::Process(const unsigned char *data)
{
    int off = 0;

    m_componentTag = COMBINE32(data, 0);
    if (0x49534F50 != m_componentTag)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] Invalid ObjectLocation tag");
        return 0;
    }
    off += 4;

    m_componentDataLen = data[off++];
    m_reference.m_nCarouselId = COMBINE32(data, off);

    off += 4;

    m_reference.m_nModuleId = (data[off] << 8) | data[off + 1];
    off += 2;

    m_versionMajor = data[off++];
    m_versionMinor = data[off++];
    if (1 != m_versionMajor || 0 != m_versionMinor)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] Invalid ObjectLocation version");
        return 0;
    }

    uint objKeyLen = data[off++]; /* <= 4 */
    m_reference.m_key = DSMCCCacheKey((char*)data + off, objKeyLen);
    off += objKeyLen;
    return off;
}

// A Lite profile body is used to refer to an object referenced through
// a different PMT, We don't support that, at least at the moment.
int ProfileBodyLite::Process(const unsigned char * /*data*/)
{
    LOG(VB_DSMCC, LOG_WARNING, "Found LiteProfileBody - Not Implemented Yet");
    return 0;
}

int ProfileBodyFull::Process(const unsigned char *data)
{
    int off = 0;

    m_dataLen = COMBINE32(data, off);
    off += 4;

    /* bit order */
    if (data[off++] != 0)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] ProfileBody invalid byte order");
        return 0;
    }

    m_liteComponentsCount = data[off++];
    if (m_liteComponentsCount < 2)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] ProfileBody invalid components_count");
        return 0;
    }

    int ret = m_objLoc.Process(data + off);
    if (ret <= 0)
        return ret;
    off += ret;

    ret = m_dsmConn.Process(data + off);
    if (ret <= 0)
        return ret;
    off += ret;

    m_objLoc.m_reference.m_nStreamTag = m_dsmConn.m_tap.m_assocTag;

    /* UKProfile - ignore anything else */

    return off;
}

int BiopIor::Process(const unsigned char *data)
{
    int off = 0;
    m_typeIdLen = COMBINE32(data, 0);
    m_typeId = (char*) malloc(m_typeIdLen);
    off += 4;
    memcpy(m_typeId, data + off, m_typeIdLen);
    off += m_typeIdLen;

    m_taggedProfilesCount = COMBINE32(data, off);
    if (m_taggedProfilesCount < 1)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] IOR missing taggedProfile");
        return 0;
    }
    off += 4;

    m_profileIdTag = COMBINE32(data, off);
    off += 4;

    if (m_profileIdTag == 0x49534F06) // profile_id_tag == 0x49534F06
    {
        m_profileBody = new ProfileBodyFull;
        int ret = m_profileBody->Process(data + off);
        if (ret <= 0)
            return ret;
        off += ret;
    }
    else if(m_profileIdTag == 0x49534F05) // profile_id_tag == 0x49534F05
    {
        m_profileBody = new ProfileBodyLite;
        int ret = m_profileBody->Process(data + off);
        if (ret <= 0)
            return ret;
        off += ret;
    }
    else
    {
        /* UKProfile - receiver may ignore other profiles */
        LOG(VB_DSMCC, LOG_WARNING, QString("[biop] Unknown Ior profile 0x%1")
            .arg(m_profileIdTag, 0, 16));
        return 0;
    }

    return off;
}

// An IOR may refer to other streams.  We may have to add taps for them.
void BiopIor::AddTap(Dsmcc *pStatus) const
{
    DSMCCCacheReference *ref = m_profileBody->GetReference();
    if (ref != nullptr)
        pStatus->AddTap(ref->m_nStreamTag, ref->m_nCarouselId);
}

BiopTap::~BiopTap()
{
    free(m_selectorData);
}
