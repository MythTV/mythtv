/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer
 */
//#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsmccbiop.h"
#include "dsmccreceiver.h"
#include "dsmcccache.h"
#include "dsmccobjcarousel.h"
#include "dsmcc.h"

#include "mythlogging.h"

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

    m_id_len    = data[off++];
    m_id        = (char*) malloc(m_id_len);
    memcpy(m_id, data + off, m_id_len);

    off        += m_id_len;
    m_kind_len  = data[off++];
    m_kind      = (char*) malloc(m_kind_len);
    memcpy(m_kind, data + off, m_kind_len);

    off        += m_kind_len;

    return off;
}

BiopName::BiopName()
{
    m_comp_count = 0;
    m_comps = NULL;
}

BiopName::~BiopName()
{
    delete[] m_comps;
}

int BiopName::Process(const unsigned char *data)
{
    int off = 0;
    m_comp_count = data[0];

    if (m_comp_count != 1)
        LOG(VB_DSMCC, LOG_WARNING, "[biop] Expected one name");

    off++;
    m_comps = new BiopNameComp[m_comp_count];

    for (int i = 0; i < m_comp_count; i++)
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
    int off = 0, ret;
    ret = m_name.Process(data);

    if (ret > 0)
        off += ret;
    else
        return ret; // Error

    m_binding_type = data[off++];
    ret = m_ior.Process(data + off);

    if (ret > 0)
        off += ret;
    else
        return ret; // Error

    m_objinfo_len = (data[off] << 8) | data[off + 1];
    off += 2;

    if (m_objinfo_len > 0)
    {
        m_objinfo = (char*) malloc(m_objinfo_len);
        memcpy(m_objinfo, data + off, m_objinfo_len);
    }
    else
        m_objinfo = NULL;

    off += m_objinfo_len;

    return off;
}

BiopBinding::~BiopBinding()
{
    free(m_objinfo);
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
    if (strcmp(m_objkind, "fil") == 0)
    {
        LOG(VB_DSMCC, LOG_DEBUG, "[biop] Processing file");
        return ProcessFile(cachep, filecache, data, curp);
    }
    else if (strcmp(m_objkind, "dir") == 0)
    {
        LOG(VB_DSMCC, LOG_DEBUG, "[biop] Processing directory");
        return ProcessDir(false, cachep, filecache, data, curp);
    }
    else if (strcmp(m_objkind, "srg") == 0)
    {
        LOG(VB_DSMCC, LOG_DEBUG, "[biop] Processing gateway");
        return ProcessDir(true, cachep, filecache, data, curp);
    }
    else
    {
        /* Error */
        LOG(VB_DSMCC, LOG_WARNING, QString("Unknown or unsupported format %1%2%3%4")
                .arg(m_objkind[0]).arg(m_objkind[1])
                .arg(m_objkind[2]).arg(m_objkind[3]));
        return false;
    }
}

BiopMessage::~BiopMessage()
{
    free(m_objinfo);
    free(m_objkind);
}

bool BiopMessage::ProcessMsgHdr(unsigned char *data, unsigned long *curp)
{
    const unsigned char *buf = data + (*curp);
    int off = 0;

    if (buf[off] !='B' || buf[off +1] !='I' || buf[off +2] !='O' || buf[off +3] !='P')
    {
        LOG(VB_DSMCC, LOG_WARNING, "BiopMessage - invalid header");
        return false;
    }
    off += 4;

    m_version_major = buf[off++];
    m_version_minor = buf[off++];
    if (m_version_major != 1 || m_version_minor != 0)
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

    m_message_size  = COMBINE32(buf, off);
    off += 4;

    uint nObjLen = buf[off++];
    m_objkey = DSMCCCacheKey((const char*)buf + off, nObjLen);
    off += nObjLen;

    m_objkind_len = COMBINE32(buf, off);
    off += 4;
    m_objkind = (char*) malloc(m_objkind_len);
    memcpy(m_objkind, buf + off, m_objkind_len);
    off += m_objkind_len;

    m_objinfo_len = buf[off] << 8 | buf[off + 1];
    off += 2;
    m_objinfo = (char*) malloc(m_objinfo_len);
    memcpy(m_objinfo, buf + off, m_objinfo_len);
    off += m_objinfo_len;

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
    unsigned char *data, unsigned long *curp)
{
    int off = 0;
    const unsigned char * const buf = data + (*curp);

    if (m_objinfo_len)
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
                            cachep->StreamId(), m_objkey);
    DSMCCCacheDir *pDir = isSrg ? filecache->Srg(ref) : filecache->Directory(ref);

    for (uint i = 0; i < bindings_count; i++)
    {
        BiopBinding binding;
        int ret = binding.Process(buf + off);
        if (ret > 0)
            off += ret;
        else
            return false; // Error

        if (binding.m_name.m_comp_count != 1)
            LOG(VB_DSMCC, LOG_WARNING, "[biop] ProcessDir nameComponents != 1");

        if (binding.m_binding_type != 1 && binding.m_binding_type != 2)
            LOG(VB_DSMCC, LOG_WARNING, "[biop] ProcessDir invalid BindingType");

        // Process any taps in this binding.
        binding.m_ior.AddTap(filecache->m_Dsmcc);

        if (pDir && binding.m_name.m_comp_count >= 1)
        {
            if (strcmp("fil", binding.m_name.m_comps[0].m_kind) == 0)
                filecache->AddFileInfo(pDir, &binding);
            else if (strcmp("dir", binding.m_name.m_comps[0].m_kind) == 0)
                filecache->AddDirInfo(pDir, &binding);
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
    unsigned long msgbody_len;
    unsigned long content_len;

    if (m_objinfo_len != 8)
        LOG(VB_DSMCC, LOG_WARNING, QString("[biop] ProcessFile objectInfo_length = %1")
            .arg(m_objinfo_len));

    const unsigned serviceContextList_count = buf[off++];
    if (serviceContextList_count)
    {
        LOG(VB_DSMCC, LOG_WARNING,
            QString("[biop] ProcessFile Unexpected serviceContextList_count %1")
            .arg(serviceContextList_count));
        return false; // Error
    }

    msgbody_len = COMBINE32(buf, off);
    off += 4;
    content_len = COMBINE32(buf, off);
    off += 4;
    if (content_len + 4 != msgbody_len)
        LOG(VB_DSMCC, LOG_WARNING, "[biop] ProcessFile incorrect msgbody_len");

    (*curp) += off;

    DSMCCCacheReference ref(cachep->CarouselId(), cachep->ModuleId(),
                            cachep->StreamId(), m_objkey);

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
        switch (tag)
        {
            case 0x01: // Type
                break;
            case 0x02: // Name
                break;
            case 0x03: // Info
                break;
            case 0x04: // Modlink
                break;
            case 0x05: // CRC
                break;
            case 0x06: // Location
                break;
            case 0x07: // DLtime
                break;
            case 0x08: // Grouplink
                break;
            case 0x09: // Compressed.
                // Skip the method.
                isCompressed = true;
                originalSize = COMBINE32(data, 1);
                break;
            default:
                break;
        }
        length -= len;
        data += len;
    }
}

int BiopModuleInfo::Process(const unsigned char *data)
{
    int off, ret;
    mod_timeout   = COMBINE32(data, 0);
    block_timeout = COMBINE32(data, 4);
    min_blocktime = COMBINE32(data, 8);

    taps_count = data[12];
    off = 13;

    LOG(VB_DSMCC, LOG_DEBUG, QString("[Biop] "
        "ModuleTimeout %1 BlockTimeout %2 MinBlockTime %3 Taps %4")
        .arg(mod_timeout).arg(block_timeout).arg(min_blocktime)
        .arg(taps_count));

    if (taps_count > 0)
    {
        /* only 1 allowed TODO - may not be first though ? */
        ret = tap.Process(data + off);
        if (ret <= 0)
            return ret;
        off += ret;
    }

    unsigned userinfo_len = data[off++];

    if (userinfo_len > 0)
    {
        descriptorData.Process(data + off, userinfo_len);
        off += userinfo_len;
    }
    return off;

}

int BiopTap::Process(const unsigned char *data)
{
    int off=0;

    id = (data[off] << 8) | data[off + 1]; // Ignored
    off += 2;
    use = (data[off] << 8) | data[off + 1];
    off += 2;
    assoc_tag = (data[off] << 8) | data[off + 1];
    off += 2;
    selector_len = data[off++];
    selector_data = (char*) malloc(selector_len);
    memcpy(selector_data, data + off, selector_len);
    if (use == 0x0016) // BIOP_DELIVERY_PARA_USE
    {
        unsigned selector_type = (data[off] << 8) | data[off + 1];
        if (selector_len >= 10 && selector_type == 0x0001)
        {
            off += 2;
            unsigned long transactionId = COMBINE32(data, off);
            off += 4;
            unsigned long timeout = COMBINE32(data, off);
            LOG(VB_DSMCC, LOG_DEBUG, QString("[biop] BIOP_DELIVERY_PARA_USE tag %1 id 0x%2 timeout %3uS")
                .arg(assoc_tag).arg(transactionId,0,16).arg(timeout));
            off += 4;
            selector_len -= 10;
        }
    }

    off += selector_len;
    return off;
}

int BiopConnbinder::Process(const unsigned char *data)
{
    int off = 0, ret;

    component_tag = COMBINE32(data, 0);
    if (0x49534F40 != component_tag)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] Invalid Connbinder tag");
        return 0;
    }
    off += 4;
    component_data_len = data[off++];
    taps_count = data[off++];
    if (taps_count > 0)
    {
        /* UKProfile - only first tap read */
        ret = tap.Process(data + off);
#if 0
        LOG(VB_GENERAL, LOG_DEBUG, QString("Binder - assoc_tag %1")
                                       .arg(tap.assoc_tag));
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

    component_tag = COMBINE32(data, 0);
    if (0x49534F50 != component_tag)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] Invalid ObjectLocation tag");
        return 0;
    }
    off += 4;

    component_data_len = data[off++];
    m_Reference.m_nCarouselId = COMBINE32(data, off);

    off += 4;

    m_Reference.m_nModuleId = (data[off] << 8) | data[off + 1];
    off += 2;

    version_major = data[off++];
    version_minor = data[off++];
    if (1 != version_major || 0 != version_minor)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] Invalid ObjectLocation version");
        return 0;
    }

    uint objKeyLen = data[off++]; /* <= 4 */
    m_Reference.m_Key = DSMCCCacheKey((char*)data + off, objKeyLen);
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
    int off = 0, ret;

    data_len = COMBINE32(data, off);
    off += 4;

    /* bit order */
    if (data[off++] != 0)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] ProfileBody invalid byte order");
        return 0;
    }

    lite_components_count = data[off++];
    if (lite_components_count < 2)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] ProfileBody invalid components_count");
        return 0;
    }

    ret = obj_loc.Process(data + off);
    if (ret <= 0)
        return ret;
    off += ret;

    ret = dsm_conn.Process(data + off);
    if (ret <= 0)
        return ret;
    off += ret;

    obj_loc.m_Reference.m_nStreamTag = dsm_conn.tap.assoc_tag;

    /* UKProfile - ignore anything else */

    return off;
}

int BiopIor::Process(const unsigned char *data)
{
    int off = 0, ret;
    type_id_len = COMBINE32(data, 0);
    type_id = (char*) malloc(type_id_len);
    off += 4;
    memcpy(type_id, data + off, type_id_len);
    off += type_id_len;

    tagged_profiles_count = COMBINE32(data, off);
    if (tagged_profiles_count < 1)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[biop] IOR missing taggedProfile");
        return 0;
    }
    off += 4;

    profile_id_tag = COMBINE32(data, off);
    off += 4;

    if (profile_id_tag == 0x49534F06) // profile_id_tag == 0x49534F06
    {
        m_profile_body = new ProfileBodyFull;
        ret = m_profile_body->Process(data + off);
        if (ret <= 0)
            return ret;
        off += ret;
    }
    else if(profile_id_tag == 0x49534F05) // profile_id_tag == 0x49534F05
    {
        m_profile_body = new ProfileBodyLite;
        ret = m_profile_body->Process(data + off);
        if (ret <= 0)
            return ret;
        off += ret;
    }
    else
    {
        /* UKProfile - receiver may ignore other profiles */
        LOG(VB_DSMCC, LOG_WARNING, QString("[biop] Unknown Ior profile 0x%1")
            .arg(profile_id_tag, 0, 16));
        return 0;
    }

    return off;
}

// An IOR may refer to other streams.  We may have to add taps for them.
void BiopIor::AddTap(Dsmcc *pStatus)
{
    DSMCCCacheReference *ref = m_profile_body->GetReference();
    if (ref != NULL)
        pStatus->AddTap(ref->m_nStreamTag, ref->m_nCarouselId);
}

BiopTap::~BiopTap()
{
    free(selector_data);
}
