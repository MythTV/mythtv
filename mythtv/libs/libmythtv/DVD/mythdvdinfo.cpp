// Std
#include <fcntl.h>
#include <zlib.h>

#undef Z_NULL
#define Z_NULL nullptr

// Qt
#include <QDir>

// MythTV
#include "libmythbase/mythlogging.h"

#include "io/mythiowrapper.h"
#include "mythdvdinfo.h"

MythDVDInfo::MythDVDInfo(const QString &Filename)
{
    LOG(VB_PLAYBACK, LOG_INFO, QString("DVDInfo: Trying %1").arg(Filename));
    QString name = Filename;
    if (name.startsWith("dvd:"))
    {
        name.remove(0,4);
        while (name.startsWith("//"))
            name.remove(0,1);
    }

    QByteArray fname = name.toLocal8Bit();
    dvdnav_status_t res = dvdnav_open(&m_nav, fname.constData());
    if (res == DVDNAV_STATUS_ERR)
    {
        m_lastError = tr("Failed to open device at %1").arg(fname.constData());
        LOG(VB_GENERAL, LOG_ERR, QString("DVDInfo: ") + m_lastError);
        return;
    }

    GetNameAndSerialNum(m_nav, m_name, m_serialnumber, name, QString("DVDInfo: "));
}

MythDVDInfo::~MythDVDInfo(void)
{
    if (m_nav)
        dvdnav_close(m_nav);
    LOG(VB_PLAYBACK, LOG_INFO, QString("DVDInfo: Finishing."));
}

bool MythDVDInfo::IsValid(void) const
{
    return m_nav != nullptr;
}

void MythDVDInfo::GetNameAndSerialNum(dvdnav_t* Nav,
                                  QString &Name,
                                  QString &Serialnum,
                                  const QString &Filename,
                                  const QString &LogPrefix)
{
    const char* dvdname = nullptr;
    const char* dvdserial = nullptr;

    if (dvdnav_get_title_string(Nav, &dvdname) == DVDNAV_STATUS_ERR)
        LOG(VB_GENERAL, LOG_ERR, LogPrefix + "Failed to get name.");
    if (dvdnav_get_serial_string(Nav, &dvdserial) == DVDNAV_STATUS_ERR)
        LOG(VB_GENERAL, LOG_ERR, LogPrefix + "Failed to get serial number.");

    Name = QString(dvdname);
    Serialnum = QString(dvdserial);

    if (Name.isEmpty() && Serialnum.isEmpty())
    {
        struct stat stat {};
        if ((MythFileStat(Filename.toLocal8Bit(), &stat) == 0) && S_ISDIR(stat.st_mode))
        {
            // Name and serial number are empty because we're reading
            // from a directory (and not a device or image).

            // Use the directory name for the DVD name
            QDir dir(Filename);
            Name = dir.dirName();
            LOG(VB_PLAYBACK, LOG_DEBUG, LogPrefix + QString("Generated dvd name '%1'")
                .arg(Name));

            // And use the CRC of VTS_01_0.IFO as a serial number
            QString ifo = Filename + QString("/VIDEO_TS/VTS_01_0.IFO");
            int fd = MythFileOpen(ifo.toLocal8Bit(), O_RDONLY);

            if (fd > 0)
            {
                DvdBuffer buf {};
                ssize_t read = 0;
                auto crc = static_cast<uint32_t>(crc32(0L, Z_NULL, 0));

                while((read = MythFileRead(fd, buf.data(), buf.size())) > 0)
                    crc = static_cast<uint32_t>(crc32(crc, buf.data(), static_cast<uint>(read)));

                MythfileClose(fd);
                Serialnum = QString("%1__gen").arg(crc, 0, 16, QChar('0'));
                LOG(VB_PLAYBACK, LOG_DEBUG, LogPrefix + QString("Generated serial number '%1'")
                    .arg(Serialnum));
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LogPrefix + QString("Unable to open %2 to generate serial number")
                    .arg(ifo));
            }
        }
    }
}

bool MythDVDInfo::GetNameAndSerialNum(QString &Name, QString &SerialNumber)
{
    Name         = m_name;
    SerialNumber = m_serialnumber;
    return !(Name.isEmpty() && SerialNumber.isEmpty());
}

QString MythDVDInfo::GetLastError(void) const
{
    return m_lastError;
}
