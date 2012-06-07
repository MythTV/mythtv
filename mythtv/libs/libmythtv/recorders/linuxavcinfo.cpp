#include "linuxavcinfo.h"

// MythTV headers
#include "mythcontext.h"

#define LOC      QString("LAVCInfo(): ")

bool LinuxAVCInfo::Update(uint64_t _guid, raw1394handle_t handle,
                          uint _port, uint _node)
{
    port = _port;
    node = _node;

    if (guid == _guid)
        return true; // we're done

    //////////////////////////
    // get basic info

    rom1394_directory dir;
    if (rom1394_get_directory(handle, _node, &dir) < 0)
        return false;

    guid     = _guid;
    vendorid = dir.vendor_id;
    modelid  = dir.model_id;
    specid   = dir.unit_spec_id;
    firmware_revision = dir.unit_sw_version;
    product_name      = QString("%1").arg(dir.label);

    if (avc1394_subunit_info(handle, node, (uint32_t*)unit_table) < 0)
        memset(unit_table, 0xff, sizeof(unit_table));

    return true;
}

bool LinuxAVCInfo::OpenPort(void)
{
    LOG(VB_RECORD, LOG_INFO,
        LOC + QString("Getting raw1394 handle for port %1").arg(port));
    fw_handle = raw1394_new_handle_on_port(port);

    if (!fw_handle)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Unable to get handle for " +
                QString("port: %1").arg(port) + ENO);

        return false;
    }

    return true;
}

bool LinuxAVCInfo::ClosePort(void)
{
    if (fw_handle)
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Releasing raw1394 handle");
        raw1394_destroy_handle(fw_handle);
        fw_handle = NULL;
    }

    return true;
}

bool LinuxAVCInfo::SendAVCCommand(
    const vector<uint8_t>  &_cmd,
    vector<uint8_t>        &result,
    int                     retry_cnt)
{
    retry_cnt = (retry_cnt < 0) ? 2 : retry_cnt;

    result.clear();

    if (!fw_handle || (node < 0))
        return false;

    vector<uint8_t> cmd = _cmd;
    while (cmd.size() & 0x3)
        cmd.push_back(0x00);

    if (cmd.size() > 4096)
        return false;

    uint32_t cmdbuf[1024];
    for (uint i = 0; i < cmd.size(); i+=4)
        cmdbuf[i>>2] = cmd[i]<<24 | cmd[i+1]<<16 | cmd[i+2]<<8 | cmd[i+3];

    uint result_length = 0;

    uint32_t *ret = avc1394_transaction_block2(
        fw_handle, node, cmdbuf, cmd.size() >> 2,
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

    avc1394_transaction_block_close(fw_handle);

    return true;
}
