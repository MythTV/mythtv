#ifndef MYTHDVDINFO_H
#define MYTHDVDINFO_H

// Qt
#include <QCoreApplication>

// MythTV
#include "mythtvexp.h"

// libdvd
#include "dvdnav/dvdnav.h"

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
