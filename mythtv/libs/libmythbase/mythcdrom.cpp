#include "mythcdrom.h"

#ifdef HAVE_LIBUDFREAD
#include <udfread/udfread.h>
#include <udfread/blockinput.h>
#else
#include "udfread.h"
#include "blockinput.h"
#endif

#include <QtGlobal>
#include <QDir>
#include <QFileInfo>

#include "compat.h"
#include "mythconfig.h"
#include "mythlogging.h"
#include "remotefile.h"

#ifdef __linux__
#   include "mythcdrom-linux.h"
#elif defined(__FreeBSD__)
#   include "mythcdrom-freebsd.h"
#elif defined(Q_OS_DARWIN)
#   include "mythcdrom-darwin.h"
#endif

// If your DVD has directories in lowercase, then it is wrongly mounted!
// DVDs use the UDF filesystem, NOT ISO9660. Fix your /etc/fstab.

// This allows a warning for the above mentioned OS setup fault
static constexpr const char* PATHTO_BAD_DVD_MOUNT { "/video_ts"   };

static constexpr const char* PATHTO_DVD_DETECT    { "/VIDEO_TS"   };
static constexpr const char* PATHTO_BD_DETECT     { "/BDMV"       };
static constexpr const char* PATHTO_VCD_DETECT    { "/vcd"        };
static constexpr const char* PATHTO_SVCD_DETECT   { "/svcd"       };

// Mac OS X mounts audio CDs ready to use
static constexpr const char* PATHTO_AUDIO_DETECT  { "/.TOC.plist" };


MythCDROM* MythCDROM::get(QObject* par, const QString& devicePath,
                          bool SuperMount, bool AllowEject)
{
#if defined(__linux__) && !defined(Q_OS_ANDROID)
    return GetMythCDROMLinux(par, devicePath, SuperMount, AllowEject);
#elif defined(__FreeBSD__)
    return GetMythCDROMFreeBSD(par, devicePath, SuperMount, AllowEject);
#elif defined(Q_OS_DARWIN)
    return GetMythCDROMDarwin(par, devicePath, SuperMount, AllowEject);
#else
    return new MythCDROM(par, devicePath, SuperMount, AllowEject);
#endif
}

MythCDROM::MythCDROM(QObject* par, const QString& DevicePath, bool SuperMount,
                     bool AllowEject)
         : MythMediaDevice(par, DevicePath, SuperMount, AllowEject)
{
}

void MythCDROM::onDeviceMounted()
{
    if (!QDir(m_mountPath).exists())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Mountpoint '%1' doesn't exist")
                                     .arg(m_mountPath));
        m_mediaType = MEDIATYPE_UNKNOWN;
        m_status    = MEDIASTAT_ERROR;
        return;
    }

    QFileInfo audio = QFileInfo(m_mountPath + PATHTO_AUDIO_DETECT);
    QDir        dvd = QDir(m_mountPath  + PATHTO_DVD_DETECT);
    QDir       svcd = QDir(m_mountPath  + PATHTO_SVCD_DETECT);
    QDir        vcd = QDir(m_mountPath  + PATHTO_VCD_DETECT);
    QDir    bad_dvd = QDir(m_mountPath  + PATHTO_BAD_DVD_MOUNT);
    QDir         bd = QDir(m_mountPath  + PATHTO_BD_DETECT);

    // Default is data media
    m_mediaType = MEDIATYPE_DATA;

    // Default is mounted media
    m_status = MEDIASTAT_MOUNTED;

    if (dvd.exists())
    {
        LOG(VB_MEDIA, LOG_INFO, "Probable DVD detected.");
        m_mediaType = MEDIATYPE_DVD;
        m_status = MEDIASTAT_USEABLE;
    }
    else if (bd.exists())
    {
        LOG(VB_MEDIA, LOG_INFO, "Probable Blu-ray detected.");
        m_mediaType = MEDIATYPE_BD;
        m_status = MEDIASTAT_USEABLE;
    }
    else if (audio.exists())
    {
        LOG(VB_MEDIA, LOG_INFO, "Probable Audio CD detected.");
        m_mediaType = MEDIATYPE_AUDIO;
        m_status = MEDIASTAT_USEABLE;
    }
    else if (vcd.exists() || svcd.exists())
    {
        LOG(VB_MEDIA, LOG_INFO, "Probable VCD/SVCD detected.");
        m_mediaType = MEDIATYPE_VCD;
        m_status = MEDIASTAT_USEABLE;
    }
    else if (bad_dvd.exists())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "DVD incorrectly mounted? (ISO9660 instead of UDF)");
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
                QString("CD/DVD '%1' contained none of\n").arg(m_mountPath) +
            QString("\t\t\t%1, %2, %3 or %4")
                .arg(PATHTO_DVD_DETECT, PATHTO_AUDIO_DETECT,
                     PATHTO_VCD_DETECT,  PATHTO_SVCD_DETECT));
        LOG(VB_GENERAL, LOG_INFO, "Searching CD statistically - file by file!");
    }

    // If not DVD/AudioCD/VCD/SVCD, use parent's more generic version
    if (MEDIATYPE_DATA == m_mediaType)
        MythMediaDevice::onDeviceMounted();

    // Unlock the door, and if appropriate unmount the media,
    // so the user can press the manual eject button
    if (m_allowEject)
    {
        unlock();
        if (m_mediaType == MEDIATYPE_DVD || m_mediaType == MEDIATYPE_VCD)
            unmount();
    }
}

void MythCDROM::setDeviceSpeed(const char *devicePath, int speed)
{
    LOG(VB_MEDIA, LOG_INFO,
        QString("SetDeviceSpeed(%1,%2) - not implemented on this OS.")
        .arg(devicePath).arg(speed));
}

struct blockInput_t
{
    udfread_block_input m_input;  /* This *must* be the first entry in the struct */
    RemoteFile*         m_file;
};

static int def_close(udfread_block_input *p_gen)
{
    auto *p = (blockInput_t *)p_gen;
    int result = -1;

    if (p && p->m_file)
    {
        delete p->m_file;
        p->m_file = nullptr;
        result = 0;
    }

    return result;
}

static uint32_t def_size(udfread_block_input *p_gen)
{
    auto *p = (blockInput_t *)p_gen;

    return (uint32_t)(p->m_file->GetRealFileSize() / UDF_BLOCK_SIZE);
}

static int def_read(udfread_block_input *p_gen, uint32_t lba,
                    void *buf, uint32_t nblocks,
                    [[maybe_unused]] int flags)
{
    int result = -1;
    auto *p = (blockInput_t *)p_gen;

    if (p && p->m_file && (p->m_file->Seek(static_cast<long long>(lba) * UDF_BLOCK_SIZE, SEEK_SET) != -1))
        result = p->m_file->Read(buf, nblocks * UDF_BLOCK_SIZE) / UDF_BLOCK_SIZE;

    return result;
}

MythCDROM::ImageType MythCDROM::inspectImage(const QString &path)
{
    ImageType imageType = kUnknown;

    if (path.startsWith("bd:"))
        imageType = kBluray;
    else if (path.startsWith("dvd:"))
        imageType = kDVD;
    else
    {
        blockInput_t blockInput {};

        blockInput.m_file = new RemoteFile(path); // Normally deleted via a call to udfread_close
        blockInput.m_input.close = def_close;
        blockInput.m_input.read = def_read;
        blockInput.m_input.size = def_size;

        if (blockInput.m_file->isOpen())
        {
            udfread *udf = udfread_init();
            if (udfread_open_input(udf, &blockInput.m_input) == 0)
            {
                UDFDIR *dir = udfread_opendir(udf, "/BDMV");

                if (dir)
                {
                    LOG(VB_MEDIA, LOG_INFO, QString("Found Bluray at %1").arg(path));
                    imageType = kBluray;
                }
                else
                {
                    dir = udfread_opendir(udf, "/VIDEO_TS");

                    if (dir)
                    {
                        LOG(VB_MEDIA, LOG_INFO, QString("Found DVD at %1").arg(path));
                        imageType = kDVD;
                    }
                    else
                    {
                        LOG(VB_MEDIA, LOG_ERR, QString("inspectImage - unknown"));
                    }
                }

                if (dir)
                {
                    udfread_closedir(dir);
                }

                udfread_close(udf);
            }
            else
            {
                // Open failed, so we have clean this up here
                delete blockInput.m_file;
            }
        }
        else
        {
            LOG(VB_MEDIA, LOG_ERR, QString("inspectImage - unable to open \"%1\"").arg(path));
            delete blockInput.m_file;
        }
    }

    return imageType;
}
