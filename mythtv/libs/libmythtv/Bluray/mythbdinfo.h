#ifndef MYTHBDINFO_H
#define MYTHBDINFO_H

#ifdef HAVE_LIBBLURAY
#include <libbluray/bluray.h>
#else
#include "libbluray/bluray.h"
#endif

// Qt
#include <QCoreApplication>

// MythTV
#include "libmythtv/mythtvexp.h"

class MTV_PUBLIC MythBDInfo
{
    friend class MythBDBuffer;
    Q_DECLARE_TR_FUNCTIONS(MythBDInfo)

  public:
    explicit MythBDInfo(const QString &Filename);
   ~MythBDInfo(void) = default;
    bool    IsValid      (void) const;
    QString GetLastError (void) const;
    bool    GetNameAndSerialNum(QString &Name, QString &SerialNum);

  protected:
    static void GetNameAndSerialNum(BLURAY* BluRay, QString &Name,
                                    QString &SerialNum, const QString &Filename,
                                    const QString &LogPrefix);
    QString  m_name;
    QString  m_serialnumber;
    QString  m_lastError;
    bool     m_isValid { true };

  private:
    Q_DISABLE_COPY(MythBDInfo)
};

#endif // MYTHBDINFO_H
