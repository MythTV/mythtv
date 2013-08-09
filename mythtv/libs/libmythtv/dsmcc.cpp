/*
 *  Copyright (C) David C.J. Matthews 2005, 2006
 *     Derived from libdsmcc by Richard Palmer
 */
#include <stdint.h>

#include "mythlogging.h"

#include "dsmccreceiver.h"
#include "dsmccbiop.h"
#include "dsmcccache.h"
#include "dsmcc.h"

#define DSMCC_SYNC_BYTE         0x47
#define DSMCC_TRANSPORT_ERROR   0x80
#define DSMCC_START_INDICATOR   0x40

#define DSMCC_MESSAGE_DSI       0x1006
#define DSMCC_MESSAGE_DII       0x1002
#define DSMCC_MESSAGE_DDB       0x1003

#define DSMCC_SECTION_INDICATION    0x3B
#define DSMCC_SECTION_DATA      0x3C
#define DSMCC_SECTION_DESCR     0x3D

#define DSMCC_SECTION_OFFSET    0
#define DSMCC_MSGHDR_OFFSET     8
#define DSMCC_DATAHDR_OFFSET    8
#define DSMCC_DSI_OFFSET        20
#define DSMCC_DII_OFFSET        20
#define DSMCC_DDB_OFFSET        20
#define DSMCC_BIOP_OFFSET       24

static uint32_t crc32(const unsigned char *data, int len);

Dsmcc::Dsmcc()
{
    m_startTag = 0;
}

Dsmcc::~Dsmcc()
{
    Reset();
}

/** \fn Dsmcc::GetCarouselById(unsigned int)
 *  \brief Returns a carousel with the given ID.
 */
ObjCarousel *Dsmcc::GetCarouselById(unsigned int carouselId)
{
    QLinkedList<ObjCarousel*>::iterator it = carousels.begin();

    for (; it != carousels.end(); ++it)
    {
        ObjCarousel *car = *it;
        if (car && car->m_id == carouselId)
            return car;
    }
    return NULL;
}

/** \fn Dsmcc::AddTap(unsigned short,unsigned)
 *  \brief Add a tap.
 *
 *   This indicates the component tag of the stream that is to
 *   be used to receive subsequent messages for this carousel.
 *
 *  \return carousel for this ID.
 */
ObjCarousel *Dsmcc::AddTap(unsigned short componentTag, unsigned carouselId)
{
    ObjCarousel *car = GetCarouselById(carouselId);
    // I think we will nearly always already have a carousel with this id
    // except for start-up.

    if (car == NULL)
    { // Need to make a new one.
        car = new ObjCarousel(this);
        carousels.append(car);
        car->m_id = carouselId;
    }

    // Add this only if it's not already there.
    vector<unsigned short>::iterator it;
    for (it = car->m_Tags.begin(); it != car->m_Tags.end(); ++it)
    {
        if (*it == componentTag)
            return car;
    }

    // Not there.
    car->m_Tags.push_back(componentTag);
    LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] Adding tap for stream "
                                    "tag %1 with carousel %2")
            .arg(componentTag).arg(carouselId));

    return car;
}

bool Dsmcc::ProcessSectionHeader(DsmccSectionHeader *header,
                                 const unsigned char *data, int length)
{
    int crc_offset = 0;

    header->table_id = data[0];
    header->flags[0] = data[1];
    header->flags[1] = data[2];

    /* Check CRC is set and private_indicator is set to its complement,
     * else skip packet */
    if (((header->flags[0] & 0x80) == 0) || (header->flags[0] & 0x40) != 0)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] Invalid section");
        return false;
    }

    /* data[3] - reserved */

    header->table_id_extension = (data[4] << 8) | data[5];

    header->flags2 = data[6];

    crc_offset = length - 4 - 1;    /* 4 bytes */

    /* skip to end, read last 4 bytes and store in crc */

    header->crc = COMBINE32(data, crc_offset);

    return true;
}

/** \fn Dsmcc::ProcessDownloadServerInitiate(const unsigned char*,int)
 *  \brief Process a DSI message.
 *
 *   This may contain service gateway info. In the UK it is often the
 *   case that an object carousel is shared between different services.
 *   One PID carries all the messages for these services and the only
 *   difference is that a service-specific PID carries DSI messages
 *   identifying the root to be used for that service.
 */
void Dsmcc::ProcessDownloadServerInitiate(const unsigned char *data,
                                          int length)
{
    /* 0-19 Server id = 20 * 0xFF */
    int off;
    for (off = 0; off < 20; ++off)
    {
        if (data[off] != 0xff)
        {
            LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] DSI invalid serverID"
                " index %1: 0x%2").arg(off).arg(data[off],0,16));
            return;
        }
    }

    /* 20,21 compatibilitydescriptorlength = 0x0000 */
    if (data[off++] != 0 || data[off++] != 0)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] DSI non zero compatibilityDescriptorLen");
        return;
    }

    // 22,23 privateData length
    int data_len = (data[off] << 8) | data[off+1];
    off += 2;
    if (data_len + off > length)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] DSI ServiceGatewayInfo too big");
        return;
    }

    // 24.. IOP::IOR
    BiopIor gatewayProfile;
    int ret = gatewayProfile.Process(data+DSMCC_BIOP_OFFSET);
    if (ret <= 0)
        return; /* error */

    if (strcmp(gatewayProfile.type_id, "srg"))
    {
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] IOR unexpected type_id: '%1'")
            .arg(gatewayProfile.type_id));
        return; /* error */
    }
    if (ret + 4 > data_len)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] DSI IOP:IOR too big");
        return; /* error */
    }

    off += ret;

    // Process any new taps
    gatewayProfile.AddTap(this);

    DSMCCCacheReference *ref = gatewayProfile.m_profile_body->GetReference();
    unsigned carouselId = ref->m_nCarouselId;
    ObjCarousel *car = GetCarouselById(carouselId);

    // This provides us with a map from component tag to carousel ID.
    ProfileBodyFull *full = dynamic_cast<ProfileBodyFull*>(gatewayProfile.m_profile_body);
    if (full)
    {
        LOG(VB_DSMCC, LOG_DEBUG, QString("[dsmcc] DSI ServiceGateway"
            " carousel %1 tag %2 module %3 key %4")
            .arg(carouselId).arg(full->dsm_conn.tap.assoc_tag)
            .arg(ref->m_nModuleId).arg(ref->m_Key.toString()));

        // Add the tap to the map and create a new carousel if necessary.
        car = AddTap(full->dsm_conn.tap.assoc_tag, carouselId);
    }
    else
    {
        LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] DSI ServiceGateway"
            " carousel %1 module %2 key %3")
            .arg(carouselId).arg(ref->m_nModuleId)
            .arg(ref->m_Key.toString()));
    }

    // Set the gateway (if it isn't already set).
    if (car)
        car->filecache.SetGateway(*ref);

    // The UK profile says that we can have the file to boot in
    // the serviceContextList but in practice this seems not to
    // be used and all these counts are zero.
    unsigned short downloadTapsCount = data[off];
    off++;
    if (downloadTapsCount)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] DSI unexpected downloadTap");
        // TODO off += downloadTapsCount * sizeof(DSM::Tap);
    }

    unsigned short serviceContextListCount = data[off];
    off++;
    if (serviceContextListCount)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] DSI unexpected serviceContextList");
        // TODO off += serviceContextListCount * sizeof serviceContextList;
    }

    unsigned short userInfoLength = (data[off] << 8) | data[off+1];
    off += 2;
    if (userInfoLength)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] DSI unexpected userInfo");
        off += userInfoLength;
    }
}

void Dsmcc::ProcessDownloadInfoIndication(const unsigned char *data,
                                          unsigned short streamTag)
{
    DsmccDii dii;
    int off = 0;

    dii.download_id = COMBINE32(data, 0);

    ObjCarousel *car = GetCarouselById(dii.download_id);

    if (car == NULL)
    {
        LOG(VB_DSMCC, LOG_ERR, QString("[dsmcc] Section Info for "
                                       "unknown carousel %1")
                .arg(dii.download_id));
        // No known carousels yet (possible?)
        return;
    }

    off += 4;
    dii.block_size = data[off] << 8 | data[off+1];
    off += 2;

    off += 6; /* not used fields */
    dii.tc_download_scenario = COMBINE32(data, off);
    off += 4;

    /* skip unused compatibility descriptor len */
    off += 2;
    dii.number_modules = (data[off] << 8) | data[off + 1];
    off += 2;
    dii.modules = new DsmccModuleInfo[dii.number_modules];

    for (uint i = 0; i < dii.number_modules; i++)
    {
        dii.modules[i].module_id = (data[off] << 8) | data[off + 1];
        off += 2;
        dii.modules[i].module_size = COMBINE32(data, off);
        off += 4;
        dii.modules[i].module_version  = data[off++];
        dii.modules[i].module_info_len = data[off++];

        LOG(VB_DSMCC, LOG_DEBUG, QString("[dsmcc] Module %1 -> "
                                        "Size = %2 Version = %3")
                .arg(dii.modules[i].module_id)
                .arg(dii.modules[i].module_size)
                .arg(dii.modules[i].module_version));

        int ret = dii.modules[i].modinfo.Process(data + off);

        if (ret > 0)
        {
            off += ret;
        }
        else
        {
            return; // Error
        }
    }

    dii.private_data_len = (data[off] << 8) | data[off + 1];

    car->AddModuleInfo(&dii, this, streamTag);
    return;
}

// DSI or DII message.
void Dsmcc::ProcessSectionIndication(const unsigned char *data,
                                     int length, unsigned short streamTag)
{
    DsmccSectionHeader section;
    if (!ProcessSectionHeader(&section, data + DSMCC_SECTION_OFFSET, length))
        return;

    const unsigned char *hdrData = data + DSMCC_MSGHDR_OFFSET;

    unsigned char protocol = hdrData[0];
    if (protocol != 0x11)
    {
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Server/Info invalid protocol %1")
                .arg(protocol));
        return;
    }

    unsigned char header_type = hdrData[1];
    if (header_type != 0x03)
    {
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Server/Info invalid header type %1")
                .arg(header_type));
        return;
    }

    unsigned message_id = (hdrData[2] << 8) | hdrData[3];

//    unsigned long transaction_id = (hdrData[4] << 24) | (hdrData[5] << 16) |
//                             (hdrData[6] << 8) | hdrData[7];

    /* Data[8] - reserved */
    /* Data[9] - adapationLength 0x00 */

    unsigned message_len = (hdrData[10] << 8) | hdrData[11];
    if (message_len > 4076) // Beyond valid length
    {
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Server/Info invalid length %1")
                .arg(message_len));
        return;
    }

    if (message_id == DSMCC_MESSAGE_DSI)
    {
        LOG(VB_DSMCC, LOG_DEBUG, "[dsmcc] Server Gateway");
        // We only process DSI messages if they are received on the initial
        // stream. Because we add taps eagerly we could see a DSI on a
        // different stream before we see the one we actually want.
        if (streamTag == m_startTag)
        {
            ProcessDownloadServerInitiate(data + DSMCC_DSI_OFFSET,
                                          length - DSMCC_DSI_OFFSET);
        }
        else
        {
            LOG(VB_DSMCC, LOG_WARNING,
                QString("[dsmcc] Discarding DSI from tag %1") .arg(streamTag));
        }
        // Otherwise discard it.
    }
    else if (message_id == DSMCC_MESSAGE_DII)
    {
        LOG(VB_DSMCC, LOG_DEBUG, "[dsmcc] Module Info");
        ProcessDownloadInfoIndication(data + DSMCC_DII_OFFSET, streamTag);
    }
    else
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] Unknown section");
        /* Error */
    }

}

// DDB Message.
void Dsmcc::ProcessSectionData(const unsigned char *data, int length)
{
    DsmccSectionHeader section;
    if (!ProcessSectionHeader(&section, data + DSMCC_SECTION_OFFSET, length))
        return;

    const unsigned char *hdrData = data + DSMCC_DATAHDR_OFFSET;

    unsigned char protocol = hdrData[0];
    if (protocol != 0x11)
    {
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Data invalid protocol %1")
                .arg(protocol));
        return;
    }

    unsigned char header_type = hdrData[1];
    if (header_type != 0x03)
    {
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Data invalid header type %1")
                .arg(header_type));
        return;
    }

    unsigned message_id = (hdrData[2] << 8) | hdrData[3];
    if (message_id != DSMCC_MESSAGE_DDB)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] Data unknown section");
        return;
    }

    unsigned long download_id = COMBINE32(hdrData, 4);
    /* skip reserved byte */
//    char adaptation_len = hdrData[9];
    unsigned message_len = (hdrData[10] << 8) | hdrData[11];

    const unsigned char *blockData = data + DSMCC_DDB_OFFSET;
    DsmccDb ddb;

    ddb.module_id      = (blockData[0] << 8) | blockData[1];
    ddb.module_version = blockData[2];
    /* skip reserved byte */
    ddb.block_number   = (blockData[4] << 8) | blockData[5];
    ddb.len = message_len - 6;

    LOG(VB_DSMCC, LOG_DEBUG,
        QString("[dsmcc] Data Block ModID %1 Pos %2 Version %3")
            .arg(ddb.module_id).arg(ddb.block_number).arg(ddb.module_version));

    ObjCarousel *car = GetCarouselById(download_id);
    if (car != NULL)
        car->AddModuleData(&ddb, blockData + 6);
    else
        LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Data Block ModID %1 Pos %2"
            " unknown carousel %3")
            .arg(ddb.module_id).arg(ddb.block_number).arg(download_id));

    return;
}

void Dsmcc::ProcessSectionDesc(const unsigned char *data, int length)
{
    DsmccSectionHeader section;
    ProcessSectionHeader(&section, data + DSMCC_SECTION_OFFSET, length);

    /* TODO */
}

// Called with a complete section.  Check it for integrity and then process it.
void Dsmcc::ProcessSection(const unsigned char *data, int length,
                           int componentTag, unsigned carouselId,
                           int dataBroadcastId)
{
    // Does this component tag match one of our carousels?
    QLinkedList<ObjCarousel*>::iterator it = carousels.begin();

    LOG(VB_DSMCC, LOG_DEBUG, QString("[dsmcc] Read block size %1 from tag %2 "
                                     "carousel id %3 data broadcast Id %4")
            .arg(length).arg(componentTag)
            .arg(carouselId).arg(dataBroadcastId,0,16));

    bool found = false;
    for (; it != carousels.end(); ++it)
    {
        ObjCarousel *car = *it;
        // Is the component tag one of the ones we know?
        vector<unsigned short>::iterator it2;
        for (it2 = car->m_Tags.begin(); it2 != car->m_Tags.end(); ++it2)
        {
            if (*it2 == componentTag)
            {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }
    if (!found && dataBroadcastId == 0x0106)
    {
        // We haven't seen this stream before but it has the correct
        // data_broadcast_id. Create a carousel for it.
        // This will only happen at start-up
        if (AddTap(componentTag, carouselId))
        {
            LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] Initial stream tag %1").arg(componentTag));
            m_startTag = componentTag;
            found = true;
        }
    }

    if (!found)
    {
        LOG(VB_DSMCC, LOG_INFO, QString("[dsmcc] Dropping block size %1 with tag %2"
                                        ", carouselID %3, dataBroadcastID 0x%4")
                .arg(length).arg(componentTag).arg(carouselId)
                .arg(dataBroadcastId,0,16));

        return; // Ignore this stream.
    }

    unsigned short section_len = ((data[1] & 0xF) << 8) | (data[2]);
    section_len += 3;/* 3 bytes before length count starts */
    if (section_len > length)
    {
        LOG(VB_DSMCC, LOG_WARNING, "[dsmcc] section length > data length");
        return;
    }

    /* Check CRC before trying to parse */
    unsigned long crc32_decode = crc32(data, section_len);

    if (crc32_decode != 0)
    {
        LOG(VB_DSMCC, LOG_WARNING,
            QString("[dsmcc] Dropping corrupt section (Got %1)")
                .arg(crc32_decode));
        return;
    }

    switch (data[0])
    {
        case DSMCC_SECTION_INDICATION:
            LOG(VB_DSMCC, LOG_DEBUG, "[dsmcc] Server/Info Section");
            ProcessSectionIndication(data, length, componentTag);
            break;
        case DSMCC_SECTION_DATA:
            LOG(VB_DSMCC, LOG_DEBUG, "[dsmcc] Data Section");
            ProcessSectionData(data, length);
            break;
        case DSMCC_SECTION_DESCR:
            LOG(VB_DSMCC, LOG_DEBUG, "[dsmcc] Descriptor Section");
            ProcessSectionDesc(data, length);
            break;
        default:
            LOG(VB_DSMCC, LOG_WARNING, QString("[dsmcc] Unknown Section %1")
                    .arg(data[0]));
            break;
    }
}

// Reset the object carousel and clear the caches.
void Dsmcc::Reset()
{
    LOG(VB_DSMCC, LOG_INFO, "[dsmcc] Resetting carousel");
    QLinkedList<ObjCarousel*>::iterator it = carousels.begin();
    for (; it != carousels.end(); ++it)
        delete *it;
    carousels.clear();
    m_startTag = 0;
}

int Dsmcc::GetDSMCCObject(QStringList &objectPath, QByteArray &result)
{
    QLinkedList<ObjCarousel*>::iterator it = carousels.begin();

    if (it == carousels.end())
        return 1; // Not yet loaded.

    // Can we actually have more than one carousel?
    for (; it != carousels.end(); ++it)
    {
        int res = (*it)->filecache.GetDSMObject(objectPath, result);
        if (res != -1)
            return res;
    }

    return -1;
}

// CRC code taken from libdtv (Rolf Hakenes)
// CRC32 lookup table for polynomial 0x04c11db7

static unsigned long crc_table[256] =
{
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static uint32_t crc32(const unsigned char *data, int len)
{
    register int i;
    uint32_t crc = 0xffffffff;

    for (i = 0; i < len; i++)
        crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];

    return crc;
}
