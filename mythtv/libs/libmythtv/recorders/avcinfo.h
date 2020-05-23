#ifndef AVC_INFO_H
#define AVC_INFO_H

// C++ headers
#include <cstdint>
#include <vector>
using namespace std;

// Qt headers
#include <QString>

static inline QString guid_to_string(uint64_t guid)
{
    return QString("%1").arg(guid, 16, 16, QLatin1Char('0')).toUpper();
}

static inline uint64_t string_to_guid(const QString &guid)
{
    return guid.toULongLong(nullptr, 16);
}

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
    uint8_t  m_unit_table[32]    {};
};

#endif // AVC_INFO_H
