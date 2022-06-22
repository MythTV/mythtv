#ifndef SATIPRTCPPACKET_H
#define SATIPRTCPPACKET_H

// Qt includes
#include <QString>
#include <QIODevice>
#include <QDataStream>

// MythTV includes
#include "libmythbase/mythlogging.h"

static constexpr uint8_t RTCP_TYPE_APP { 204 };

class SatIPRTCPPacket
{
  public:
    explicit SatIPRTCPPacket(QByteArray &data);

    bool IsValid() const { return m_satipData.length() > 0; };
    QString Data() const { return m_satipData; };

  private:
    void parse();

  private:
    QByteArray m_data;
    QString m_satipData;
};

#endif // SATIPRTCPPACKET_H
