#ifndef _AVC_INFO_H_
#define _AVC_INFO_H_

#include <stdint.h>

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QString>

QString guid_to_string(uint64_t guid);
uint64_t string_to_guid(const QString &guid);

class AVCInfo
{
  public:
    AVCInfo();
    AVCInfo(const AVCInfo &o);
    AVCInfo &operator=(const AVCInfo &o);
    virtual ~AVCInfo() { }

    virtual bool SendAVCCommand(
        const vector<uint8_t> &/*cmd*/,
        vector<uint8_t>       &/*result*/,
        int                   /*retry_cnt*/)
    {
        return false;
    }

    bool GetSubunitInfo(void);

    bool IsSubunitType(int subunit_type) const;
    QString GetSubunitInfoString(void) const;
    QString GetGUIDString(void) const { return guid_to_string(guid); }

  public:
    int      port;
    int      node;
    uint64_t guid;
    uint     specid;
    uint     vendorid;
    uint     modelid;
    uint     firmware_revision;
    QString  product_name;
    uint8_t  unit_table[32];
};

#endif // _AVC_INFO_H_
