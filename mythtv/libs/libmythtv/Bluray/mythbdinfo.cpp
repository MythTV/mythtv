// Qt
#include <QDir>
#include <QCryptographicHash>

// MythTV
#include "libmythbase/mythcdrom.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"

#include "io/mythiowrapper.h"
#include "Bluray/mythbdiowrapper.h"
#include "Bluray/mythbdinfo.h"

// Std
#include <fcntl.h>

// BluRay
#ifdef HAVE_LIBBLURAY
#include <libbluray/bluray.h>
#include <libbluray/log_control.h>
#include <libbluray/meta_data.h>
#else
#include "libbluray/bluray.h"
#include "util/log_control.h"
#include "libbluray/bdnav/meta_data.h"
#endif

#define LOC QString("BDInfo: ")

MythBDInfo::MythBDInfo(const QString &Filename)
{
    LOG(VB_PLAYBACK, LOG_INFO, QString("BDInfo: Trying %1").arg(Filename));
    QString name = Filename;

    if (name.startsWith("bd:"))
    {
        name.remove(0,3);
        while (name.startsWith("//"))
            name.remove(0,1);
    }

    // clean path filename
    name = QDir(QDir::cleanPath(name)).canonicalPath();
    if (name.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("'%1' nonexistent").arg(name));
        name = Filename;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Opened MythBDBuffer device at '%1'").arg(name));

    // Make sure log messages from the Bluray library appear in our logs
    bd_set_debug_handler([](const char *Message) { LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString(Message).trimmed()); });
    bd_set_debug_mask(DBG_CRIT | DBG_NAV | DBG_BLURAY);

    // Use our own wrappers for file and directory access
    MythBDIORedirect();

    int bdhandle = -1;
    BLURAY* bdnav = nullptr;
    if (Filename.startsWith("myth:") && MythCDROM::inspectImage(Filename) != MythCDROM::kUnknown)
    {
        // Use streaming for remote images.
        // Streaming encrypted images causes a SIGSEGV in aacs code when
        // using the makemkv libraries due to the missing "device" name.
        // Since a local device (which is likely to be encrypted) can be
        // opened directly, only use streaming for remote images, which
        // presumably won't be encrypted.
        bdhandle = MythFileOpen(Filename.toLocal8Bit().data(), O_RDONLY);
        if (bdhandle >= 0)
        {
            bdnav = bd_init();
            if (bdnav)
            {
                bd_open_stream(bdnav, &bdhandle,
                    [](void *Handle, void *Buf, int LBA, int NumBlocks) {
                    if (MythFileSeek(*(static_cast<int*>(Handle)), LBA * 2048LL, SEEK_SET) != -1)
                        return static_cast<int>(MythFileRead(*(static_cast<int*>(Handle)), Buf,
                                                             static_cast<size_t>(NumBlocks) * 2048) / 2048);
                    return -1;
                });
            }
        }
    }
    else
    {
        QByteArray keyfile = QString("%1/KEYDB.cfg").arg(GetConfDir()).toLatin1();
        bdnav = bd_open(name.toLocal8Bit().data(), keyfile.constData());
    }

    if (!bdnav)
    {
        m_lastError = tr("Could not open Blu-ray device: %1").arg(name);
        LOG(VB_GENERAL, LOG_ERR, LOC + m_lastError);
        m_isValid = false;
    }
    else
    {
        GetNameAndSerialNum(bdnav, m_name, m_serialnumber, name, QString("BDInfo: "));
        bd_close(bdnav);
    }

    if (bdhandle >= 0)
        MythfileClose(bdhandle);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Done");
}

void MythBDInfo::GetNameAndSerialNum(BLURAY* BluRay, QString &Name,
                                     QString &SerialNum, const QString &Filename,
                                     const QString &LogPrefix)
{
    const meta_dl *metaDiscLibrary = bd_get_meta(BluRay);

    if (metaDiscLibrary)
    {
        Name = QString(metaDiscLibrary->di_name);
    }
    else
    {
        // Use the directory name for the Bluray name
        QDir dir(Filename);
        Name = dir.dirName();
        LOG(VB_PLAYBACK, LOG_DEBUG, LogPrefix + QString("Generated bd name '%1'")
            .arg(Name));
    }

    SerialNum.clear();

    // Try to find the first clip info file and
    // use its SHA1 hash as a serial number.
    for (uint32_t idx = 0; idx < 200; idx++)
    {
        QString clip = QString("BDMV/CLIPINF/%1.clpi").arg(idx, 5, 10, QChar('0'));
        void*   buffer     = nullptr;
        int64_t buffersize = 0;
        if (bd_read_file(BluRay, clip.toLocal8Bit().data(), &buffer, &buffersize) != 0)
        {
            QCryptographicHash crypto(QCryptographicHash::Sha1);
            // Add the clip number to the hash
            QByteArray ba = QByteArray::fromRawData(reinterpret_cast<const char*>(&idx), sizeof(idx));
            crypto.addData(ba);
            // then the length of the file
            ba = QByteArray::fromRawData(reinterpret_cast<const char*>(&buffersize), sizeof(buffersize));
            crypto.addData(ba);
            // and then the contents
            ba = QByteArray::fromRawData(reinterpret_cast<const char*>(buffer), buffersize);
            crypto.addData(ba);
            SerialNum = QString("%1__gen").arg(QString(crypto.result().toBase64()));
            free(buffer);
            LOG(VB_PLAYBACK, LOG_DEBUG, LogPrefix + QString("Generated serial number '%1'")
                .arg(SerialNum));
            break;
        }
    }

    if (SerialNum.isEmpty())
        LOG(VB_GENERAL, LOG_ERR, LogPrefix + "Unable to generate serial number");
}

bool MythBDInfo::IsValid(void) const
{
    return m_isValid;
}

bool MythBDInfo::GetNameAndSerialNum(QString &Name, QString &SerialNum)
{
    Name      = m_name;
    SerialNum = m_serialnumber;
    return !(Name.isEmpty() && SerialNum.isEmpty());
}

QString MythBDInfo::GetLastError(void) const
{
    return m_lastError;
}
