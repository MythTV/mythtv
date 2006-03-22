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

#include "mythcontext.h"

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
    off++;
    m_comps = new BiopNameComp[m_comp_count];

    for (int i = 0; i < m_comp_count; i++)
    {
        int ret = m_comps[i].Process(data + off);
        if (ret > 0)
            off += ret;
        else
            return off; // Error
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
        return off; // Error

    m_binding_type = data[off++];
    ret = m_ior.Process(data + off);

    if (ret > 0)
        off += ret;
    else
        return off; // Error

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
        VERBOSE(VB_DSMCC,"[biop] Invalid biop header, "
                "dropping rest of module");

        /* not valid, skip rest of data */
        return false;
    }

    // Handle each message type
    if (strcmp(m_objkind, "fil") == 0)
    {
        VERBOSE(VB_DSMCC,"[biop] Processing file");
        return ProcessFile(cachep, filecache, data, curp);
    }
    else if (strcmp(m_objkind, "dir") == 0)
    {
        VERBOSE(VB_DSMCC,"[biop] Processing directory");
        return ProcessDir(false, cachep, filecache, data, curp);
    }
    else if (strcmp(m_objkind, "srg") == 0)
    {
        VERBOSE(VB_DSMCC,"[biop] Processing gateway");
        return ProcessDir(true, cachep, filecache, data, curp);
    }
    else
    {
        /* Error */
        VERBOSE(VB_DSMCC, QString("Unknown or unsupported format %1%2%3%4")
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

    if (buf[0] !='B' || buf[1] !='I' || buf[2] !='O' || buf[3] !='P')
    {
        VERBOSE(VB_DSMCC, "BiopMessage - invalid header");
        return false;
    }

    off += 4;/* skip magic */
    m_version_major = buf[off++];
    m_version_minor = buf[off++];
    off += 2; /* skip byte order & message type */
    m_message_size  = ((buf[off + 0] << 24) | (buf[off+1] << 16) |
                       (buf[off + 2] << 8)  | (buf[off + 3]));
    off += 4;
    uint nObjLen = buf[off++];
    m_objkey.duplicate((const char*)buf + off, nObjLen);
    off += nObjLen;
    m_objkind_len = ((buf[off + 0] << 24) | (buf[off + 1] << 16) |
                     (buf[off + 2] << 8)  | (buf[off + 3]));

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
    const unsigned char *buf = data + (*curp);
    off++; // skip service context count

    unsigned long msgbody_len = ((buf[off + 0] << 24) | (buf[off + 1] << 16) |
                                 (buf[off + 2] <<  8) | (buf[off + 3]));
    (void) msgbody_len;
    off += 4;

    unsigned int bindings_count = buf[off] << 8 | buf[off + 1];
    off += 2;
    
    DSMCCCacheReference ref(cachep->CarouselId(), cachep->ModuleId(),
                            cachep->StreamId(), m_objkey);
    DSMCCCacheDir *pDir;
    if (isSrg)
        pDir = filecache->Srg(ref);
    else
        pDir = filecache->Directory(ref);

    VERBOSE(VB_DSMCC, QString("[Biop] Processing %1 reference %2")
            .arg(isSrg ? "gateway" : "directory").arg(ref.toString()));

    for (uint i = 0; i < bindings_count; i++)
    {
        BiopBinding binding;
        int ret = binding.Process(buf + off);
        if (ret > 0)
            off += ret;
        else
            return false; // Error

        // Process any taps in this binding.
        binding.m_ior.AddTap(filecache->m_Dsmcc);

        if (pDir)
        {
            if (strcmp("dir", binding.m_name.m_comps[0].m_kind) == 0)
                filecache->AddDirInfo(pDir, &binding);
            else if (strcmp("fil", binding.m_name.m_comps[0].m_kind) == 0)
                filecache->AddFileInfo(pDir, &binding);
        }
    }

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

    /* skip service contect count */

    off++;
    msgbody_len = ((buf[off    ] << 24) | (buf[off + 1] << 16) |
                   (buf[off + 2] <<  8) | (buf[off + 3]));
    off += 4;
    content_len = ((buf[off    ] << 24) | (buf[off + 1] << 16) |
                   (buf[off + 2] <<  8) | (buf[off + 3]));
    off += 4;

    (*curp) += off;

    DSMCCCacheReference ref(cachep->CarouselId(), cachep->ModuleId(),
                            cachep->StreamId(), m_objkey);

    QByteArray filedata;
    filedata.duplicate((const char *)data+(*curp), content_len);
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
                originalSize = ((data[1] << 24) | (data[2] << 16) |
                                (data[3] <<  8) | (data[4]));
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
    mod_timeout   = ((data[0]  << 24) | (data[1] << 16) |
                     (data[2]  <<  8) | (data[3]));
    block_timeout = ((data[4]  << 24) | (data[5] << 16) |
                     (data[6]  <<  8) | (data[7]));
    min_blocktime = ((data[8]  << 24) | (data[9] << 16) |
                     (data[10] <<  8) | (data[11]));

    taps_count = data[12];
    off = 13;

    if (taps_count > 0)
    {
        /* only 1 allowed TODO - may not be first though ? */
        ret = tap.Process(data + off);
        if (ret > 0)
            off += ret;
        /* else TODO error */
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

    id = (data[0] << 8) | data[1];
    off += 2;
    use = (data[off] << 8) | data[off + 1];
    off += 2;
    assoc_tag = (data[off] << 8) | data[off + 1];
    off += 2;
    selector_len = data[off++];
    selector_data = (char*) malloc(selector_len);
    memcpy(selector_data, data + off, selector_len);
    off += selector_len;

    return off;
}

int BiopConnbinder::Process(const unsigned char *data)
{
    int off = 0, ret;

    component_tag = ((data[0] << 24) | (data[1] << 16) |
                     (data[2] << 8)  | (data[3]));

    off += 4;
    component_data_len = data[off++];
    taps_count = data[off++];
    if (taps_count > 0)
    {
        /* UKProfile - only first tap read */
        ret = tap.Process(data + off);
        //printf("Binder - assoc_tag %u\n", tap.assoc_tag);
        if (ret > 0)
            off += ret;
        /* else TODO error */ 
    }

    return off;
}

int BiopObjLocation::Process(const unsigned char *data)
{
    int off = 0;

    component_tag = ((data[0] << 24) | (data[1] << 16) |
                     (data[2] <<  8) | (data[3]));
    off += 4;
    component_data_len = data[off++];
    m_Reference.m_nCarouselId =
        ((data[off    ] << 24) | (data[off + 1] << 16) |
         (data[off + 2] <<  8) | (data[off + 3]));

    off += 4;
    m_Reference.m_nModuleId = (data[off] << 8) | data[off + 1];
    off += 2;
    version_major = data[off++];
    version_minor = data[off++];
    uint objKeyLen = data[off++]; /* <= 4 */
    m_Reference.m_Key.duplicate((char*)data + off, objKeyLen);
    off += objKeyLen;
    return off;
}

// A Lite profile body is used to refer to an object referenced through
// a different PMT, We don't support that, at least at the moment.
int ProfileBodyLite::Process(const unsigned char */*data*/)
{
    VERBOSE(VB_DSMCC, "Found LiteProfileBody - Not Implemented Yet");
    return 0;
}

int ProfileBodyFull::Process(const unsigned char *data)
{
    int off = 0, ret;

    data_len = ((data[off    ] << 24) | (data[off + 1] << 16) |
                (data[off + 2] <<  8) | (data[off + 3]));
    off += 4;
    /* skip bit order */
    off += 1;
    lite_components_count = data[off++];

    ret = obj_loc.Process(data + off);
    if (ret > 0)
        off += ret;
    /* else TODO error */

    ret = dsm_conn.Process(data + off);
    if (ret > 0)
        off += ret;
    /* else TODO error */

    obj_loc.m_Reference.m_nStreamTag = dsm_conn.tap.assoc_tag;

    /* UKProfile - ignore anything else */

    return off;
}

int BiopIor::Process(const unsigned char *data)
{
    int off = 0, ret;
    type_id_len = ((data[0] << 24) | (data[1] << 16) |
                   (data[2] << 8)  | (data[3]));
    type_id = (char*) malloc(type_id_len);
    off += 4;
    memcpy(type_id, data + off, type_id_len);
    off += type_id_len;
    tagged_profiles_count = ((data[off    ] << 24) | (data[off + 1] << 16) |
                             (data[off + 2] <<  8) | (data[off + 3]));
    off += 4;
    profile_id_tag = ((data[off    ] << 24) | (data[off + 1] << 16) |
                      (data[off + 2] <<  8) | (data[off + 3]));
    off += 4;

    if ((profile_id_tag & 0xFF) == 0x06) // profile_id_tag == 0x49534F06
    {
        m_profile_body = new ProfileBodyFull;
        ret = m_profile_body->Process(data + off);
        if (ret > 0)
            off += ret;
        /* else TODO error */
    }
    else if((profile_id_tag & 0xFF) == 0x05) // profile_id_tag == 0x49534F05
    {
        m_profile_body = new ProfileBodyLite;
        ret = m_profile_body->Process(data + off);
        if (ret > 0)
            off += ret;
        /* else TODO error */
    }

    /* UKProfile - receiver may ignore other profiles */

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
