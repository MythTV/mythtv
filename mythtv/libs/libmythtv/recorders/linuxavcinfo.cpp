#include "linuxavcinfo.h"

// MythTV headers
#include "libmyth/mythcontext.h"

#define LOC      QString("LAVCInfo(): ")

bool LinuxAVCInfo::Update(uint64_t _guid, raw1394handle_t handle,
                          uint _port, uint _node)
{
    m_port = _port;
    m_node = _node;

    if (m_guid == _guid)
        return true; // we're done

    //////////////////////////
    // get basic info

    rom1394_directory dir;
    if (rom1394_get_directory(handle, _node, &dir) < 0)
        return false;

    m_guid     = _guid;
    m_vendorid = dir.vendor_id;
    m_modelid  = dir.model_id;
    m_specid   = dir.unit_spec_id;
    m_firmware_revision = dir.unit_sw_version;
    m_product_name      = QString("%1").arg(dir.label);

    if (avc1394_subunit_info(handle, m_node, (uint32_t*)m_unit_table.data()) < 0)
        m_unit_table.fill(0xff);

    return true;
}

bool LinuxAVCInfo::OpenPort(void)
{
    LOG(VB_RECORD, LOG_INFO,
        LOC + QString("Getting raw1394 handle for port %1").arg(m_port));
    m_fwHandle = raw1394_new_handle_on_port(m_port);

    if (!m_fwHandle)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to get handle for " +
                QString("port: %1").arg(m_port) + ENO);

        return false;
    }

    return true;
}

bool LinuxAVCInfo::ClosePort(void)
{
    if (m_fwHandle)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Releasing raw1394 handle");
        raw1394_destroy_handle(m_fwHandle);
        m_fwHandle = nullptr;
    }

    return true;
}

bool LinuxAVCInfo::SendAVCCommand(
    const std::vector<uint8_t>  &_cmd,
    std::vector<uint8_t>        &result,
    int                     retry_cnt)
{
    retry_cnt = (retry_cnt < 0) ? 2 : retry_cnt;

    result.clear();

    if (!m_fwHandle || (m_node < 0))
        return false;

    std::vector<uint8_t> cmd = _cmd;
    while (cmd.size() & 0x3)
        cmd.push_back(0x00);

    if (cmd.size() > 4096)
        return false;

    std::array<uint32_t,1024> cmdbuf {};
    for (size_t i = 0; i < cmd.size(); i+=4)
        cmdbuf[i>>2] = cmd[i]<<24 | cmd[i+1]<<16 | cmd[i+2]<<8 | cmd[i+3];

    uint result_length = 0;

    uint32_t *ret = avc1394_transaction_block2(
        m_fwHandle, m_node, cmdbuf.data(), cmd.size() >> 2,
        &result_length, retry_cnt);

    if (!ret)
        return false;

    for (uint i = 0; i < result_length; i++)
    {
        result.push_back((ret[i]>>24) & 0xff);
        result.push_back((ret[i]>>16) & 0xff);
        result.push_back((ret[i]>>8)  & 0xff);
        result.push_back((ret[i])     & 0xff);
    }

    avc1394_transaction_block_close(m_fwHandle);

    return true;
}
