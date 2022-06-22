#ifndef MYTHDVDINFO_H
#define MYTHDVDINFO_H

// C++
#include <array>

// Qt
#include <QCoreApplication>

// MythTV
#include "libmythtv/mythtvexp.h"

// libdvd
#include "dvdnav/dvdnav.h"

static constexpr size_t DVD_BLOCK_SIZE { 2048 };
using DvdBuffer = std::array<uint8_t,DVD_BLOCK_SIZE>;

class MTV_PUBLIC MythDVDInfo
{
    friend class MythDVDBuffer;
    Q_DECLARE_TR_FUNCTIONS(DVDInfo)

  public:
    explicit MythDVDInfo(const QString &Filename);
   ~MythDVDInfo(void);

    bool    IsValid             (void) const;
    bool    GetNameAndSerialNum (QString &Name, QString &SerialNumber);
    QString GetLastError        (void) const;

  protected:
    static void GetNameAndSerialNum(dvdnav_t* Nav, QString &Name,
                                    QString &Serialnum, const QString &Filename,
                                    const QString &LogPrefix);

    dvdnav_t *m_nav { nullptr };
    QString   m_name;
    QString   m_serialnumber;
    QString   m_lastError;
};

#endif // MYTHDVDINFO_H
