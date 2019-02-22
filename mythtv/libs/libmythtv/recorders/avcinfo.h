#ifndef _AVC_INFO_H_
#define _AVC_INFO_H_

// C++ headers
#include <cstdint>
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
    virtual ~AVCInfo() = default;

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
    QString GetGUIDString(void) const { return guid_to_string(m_guid); }

  public:
    int      m_port              {-1};
    int      m_node              {-1};
    uint64_t m_guid              {0};
    uint     m_specid            {0};
    uint     m_vendorid          {0};
    uint     m_modelid           {0};
    uint     m_firmware_revision {0};
    QString  m_product_name;
    uint8_t  m_unit_table[32];
};

#endif // _AVC_INFO_H_
