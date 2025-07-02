#include "mhi.h"

#include <QRegion>
#include <QVector>
#include <QUrl>
#include <QPoint>                       // for QPoint
#include <QRgb>                         // for QRgb
#include <QVariant>                     // for QVariant
#include <QByteArray>                   // for QByteArray
#include <QStringList>                  // for QStringList
#include <QTime>                        // for QTime
#include <QHash>                        // for QHash

#include <algorithm>                    // for min
#include <cmath>                        // for round, sqrt
#include <cstdint>                      // for uint8_t
#include <cstring>                      // for memcpy, memset
#include <deque>                        // for _Deque_iterator, operator!=

#include "libmyth/mythaverror.h"
#include "libmyth/mythavframe.h"
#include "libmythbase/mthread.h"        // for MThread
#include "libmythbase/mythcorecontext.h"// for MythCoreContext, etc
#include "libmythbase/mythdb.h"         // for MythDB
#include "libmythbase/mythdbcon.h"      // for MSqlQuery
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythevent.h"      // for MythEvent
#include "libmythbase/mythlogging.h"
#include "libmythui/mythimage.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythpainter.h"
#include "libmythui/mythrect.h"         // for MythRect
#include "libmythui/mythuiactions.h"    // for ACTION_0, ACTION_1, etc
#include "libmythui/mythuiimage.h"

#include "dsmcc.h"                      // for Dsmcc
#include "interactivescreen.h"
#include "interactivetv.h"              // for InteractiveTV
#include "mythavutil.h"
#include "mythplayerui.h"
#include "tv_actions.h"                 // for ACTION_MENUTEXT, etc

extern "C" {
#include "libavutil/imgutils.h"
}

static bool       ft_loaded = false;
static FT_Library ft_library;

static constexpr uint8_t FONT_WIDTHRES   { 54 };
static constexpr uint8_t FONT_HEIGHTRES  { 72 }; // 1 pixel per point
static constexpr const char * FONT_TO_USE { "FreeSans.ttf" }; // Tiresias Screenfont.ttf is mandated


// LifecycleExtension tuneinfo:
const unsigned kTuneQuietly   = 1U<<0; // b0 tune quietly
const unsigned kTuneKeepApp   = 1U<<1; // b1 keep app running
const unsigned kTuneCarId     = 1U<<2; // b2 carousel id in bits 8..16
const unsigned kTuneCarReset  = 1U<<3; // b3 get carousel id from gateway info
//const unsigned kTuneBcastDisa = 1U<<4; // b4 broadcaster_interrupt disable
// b5..7 reserverd
// b8..15 carousel id
const unsigned kTuneKeepChnl  = 1U<<16; // Keep current channel
// b17..31 reserved

/** \class MHIImageData
 *  \brief Data for items in the interactive television display stack.
 */
class MHIImageData
{
  public:
    QImage m_image;
    int    m_x      {0};
    int    m_y      {0};
    bool   m_bUnder {false};
};

MHIContext::MHIContext(InteractiveTV *parent)
    : m_parent(parent),     m_dsmcc(new Dsmcc()),
      m_engine(MHCreateEngine(this))
{
    if (!ft_loaded)
    {
        FT_Error error = FT_Init_FreeType(&ft_library);
        if (!error)
            ft_loaded = true;
    }

    if (ft_loaded)
    {
        // TODO: We need bold and italic versions.
        if (LoadFont(FONT_TO_USE))
            m_faceLoaded = true;
    }
}

// Load the font.  Copied, generally, from OSD::LoadFont.
bool MHIContext::LoadFont(const QString& name)
{
    QString fullnameA = GetConfDir() + "/" + name;
    QByteArray fnameA = fullnameA.toLatin1();
    FT_Error errorA = FT_New_Face(ft_library, fnameA.constData(), 0, &m_face);
    if (!errorA)
        return true;

    QString fullnameB = GetFontsDir() + name;
    QByteArray fnameB = fullnameB.toLatin1();
    FT_Error errorB = FT_New_Face(ft_library, fnameB.constData(), 0, &m_face);
    if (!errorB)
        return true;

    QString fullnameC = GetShareDir() + "themes/" + name;
    QByteArray fnameC = fullnameC.toLatin1();
    FT_Error errorC = FT_New_Face(ft_library, fnameC.constData(), 0, &m_face);
    if (!errorC)
        return true;

    const QString& fullnameD = name;
    QByteArray fnameD = fullnameD.toLatin1();
    FT_Error errorD = FT_New_Face(ft_library, fnameD.constData(), 0, &m_face);
    if (!errorD)
        return true;

    LOG(VB_GENERAL, LOG_ERR, QString("[mhi] Unable to find font: %1").arg(name));
    return false;
}

MHIContext::~MHIContext()
{
    StopEngine();
    delete(m_engine);
    delete(m_dsmcc);
    if (m_faceLoaded) FT_Done_Face(m_face);

    ClearDisplay();
    ClearQueue();
}

// NB caller must hold m_display_lock
void MHIContext::ClearDisplay(void)
{
    for (auto & it : m_display)
        delete it;
    m_display.clear();
    m_videoDisplayRect = QRect();
}

// NB caller must hold m_dsmccLock
void MHIContext::ClearQueue(void)
{
    for (auto & it : m_dsmccQueue)
        delete it;
    m_dsmccQueue.clear();
}

// Ask the engine to stop and block until it has.
void MHIContext::StopEngine(void)
{
    if (nullptr == m_engineThread)
        return;

    m_stop = true;
    m_runLock.lock();
    m_engineWait.wakeAll();
    m_runLock.unlock();

    m_engineThread->wait();
    delete m_engineThread;
    m_engineThread = nullptr;
}


// Start or restart the MHEG engine.
void MHIContext::Restart(int chanid, int sourceid, bool isLive)
{
    int tuneinfo = m_tuneInfo.isEmpty() ? 0 : m_tuneInfo.takeFirst();

    LOG(VB_MHEG, LOG_INFO,
        QString("[mhi] Restart ch=%1 source=%2 live=%3 tuneinfo=0x%4")
        .arg(chanid).arg(sourceid).arg(isLive).arg(tuneinfo,0,16));

    if (m_currentSource != sourceid)
    {
        m_currentSource = sourceid;
        QMutexLocker locker(&m_channelMutex);
        m_channelCache.clear();
    }
    m_currentStream = (chanid) ? chanid : -1;
    if (!(tuneinfo & kTuneKeepChnl))
        m_currentChannel = m_currentStream;

    if (tuneinfo & kTuneKeepApp)
    {
        // We have tuned to the channel in order to find the streams.
        // Leave the MHEG engine running but restart the DSMCC carousel.
        // This is a bit of a mess but it's the only way to be able to
        // select streams from a different channel.
        {
            QMutexLocker locker(&m_dsmccLock);
            if (tuneinfo & kTuneCarReset)
                m_dsmcc->Reset();
            ClearQueue();
        }

        if (tuneinfo & (kTuneCarReset|kTuneCarId))
        {
            QMutexLocker locker(&m_runLock);
            m_engine->EngineEvent(10); // NonDestructiveTuneOK
        }
    }
    else
    {
        StopEngine();

        m_audioTag = -1;
        m_videoTag = -1;

        {
            QMutexLocker locker(&m_dsmccLock);
            m_dsmcc->Reset();
            ClearQueue();
        }

        {
            QMutexLocker locker(&m_keyLock);
            m_keyQueue.clear();
        }

        m_engine->SetBooting();
        ClearDisplay();
        m_updated = true;
        m_stop = false;
        m_isLive = isLive;
        // Don't set the NBI version here.  Restart is called
        // after the PMT is processed.
        m_engineThread = new MThread("MHEG", this);
        m_engineThread->start();
    }
}

void MHIContext::run(void)
{
    QMutexLocker locker(&m_runLock);

    while (!m_stop)
    {
        std::chrono::milliseconds toWait = 0ms;
        // Dequeue and process any key presses.
        int key = -1;
        while (key != 0)
        {
            NetworkBootRequested();
            ProcessDSMCCQueue();
            {
                QMutexLocker locker2(&m_keyLock);
                key = m_keyQueue.dequeue();
            }

            if (key != 0)
                m_engine->GenerateUserAction(key);

            // Run the engine and find out how long to pause.
            toWait = m_engine->RunAll();
            if (toWait < 0ms)
                return;
        }

        toWait = (toWait > 1s || toWait <= 0ms) ? 1s : toWait;

        if (!m_stop && (toWait > 0ms))
            m_engineWait.wait(locker.mutex(), toWait.count());
    }
}

// Dequeue and process any DSMCC packets.
void MHIContext::ProcessDSMCCQueue(void)
{
    DSMCCPacket *packet = nullptr;
    do
    {
        QMutexLocker locker(&m_dsmccLock);
        packet = m_dsmccQueue.dequeue();
        if (packet)
        {
            m_dsmcc->ProcessSection(
                packet->m_data,           packet->m_length,
                packet->m_componentTag,   packet->m_carouselId,
                packet->m_dataBroadcastId);

            delete packet;
        }
    } while (packet);
}

void MHIContext::QueueDSMCCPacket(
    unsigned char *data, int length, int componentTag,
    unsigned carouselId, int dataBroadcastId)
{
    auto *dataCopy = (unsigned char*) malloc(length * sizeof(unsigned char));

    if (dataCopy == nullptr)
        return;

    memcpy(dataCopy, data, length*sizeof(unsigned char));
    {
        QMutexLocker locker(&m_dsmccLock);
        m_dsmccQueue.enqueue(new DSMCCPacket(dataCopy,     length,
                                             componentTag, carouselId,
                                             dataBroadcastId));
    }
    m_engineWait.wakeAll();
}

// A NetworkBootInfo sub-descriptor is present in the PMT.
void MHIContext::SetNetBootInfo(const unsigned char *data, uint length)
{
    if (length < 2) // A valid descriptor should always have at least 2 bytes.
        return;

    LOG(VB_MHEG, LOG_INFO, QString("[mhi] SetNetBootInfo version %1 mode %2 len %3")
        .arg(data[0]).arg(data[1]).arg(length));

    QMutexLocker locker(&m_dsmccLock);
    // The carousel should be reset now as the stream has changed
    m_dsmcc->Reset();
    ClearQueue();
    // Save the data from the descriptor.
    m_nbiData.resize(0);
    m_nbiData.reserve(length);
    m_nbiData.insert(m_nbiData.begin(), data, data+length);
    // If there is no Network Boot Info or we're setting it
    // for the first time just update the "last version".
    if (m_lastNbiVersion == NBI_VERSION_UNSET)
        m_lastNbiVersion = data[0];
    else
        m_engineWait.wakeAll();
}

// Called only by m_engineThread
void MHIContext::NetworkBootRequested(void)
{
    QMutexLocker locker(&m_dsmccLock);
    if (m_nbiData.size() >= 2 && m_nbiData[0] != m_lastNbiVersion)
    {
        m_lastNbiVersion = m_nbiData[0]; // Update the saved version
        switch (m_nbiData[1])
        {
        case 1:
            m_dsmcc->Reset();
            m_engine->SetBooting();
            locker.unlock();
            {QMutexLocker locker2(&m_displayLock);
            ClearDisplay();
            m_updated = true;}
            break;
        case 2:
            m_engine->EngineEvent(9); // NetworkBootInfo EngineEvent
            break;
        default:
            LOG(VB_MHEG, LOG_INFO, QString("[mhi] Unknown NetworkBoot type %1")
                .arg(m_nbiData[1]));
            break;
        }
    }
}

// Called by the engine to check for the presence of an object in the carousel.
bool MHIContext::CheckCarouselObject(const QString& objectPath)
{
    if (objectPath.startsWith("http:") || objectPath.startsWith("https:"))
    {
        QByteArray cert;

        // Verify access to server
        if (!CheckAccess(objectPath, cert))
            return false;

        return m_ic.CheckFile(objectPath, cert);
    }

    QStringList path = objectPath.split(QChar('/'), Qt::SkipEmptyParts);
    QByteArray result; // Unused
    QMutexLocker locker(&m_dsmccLock);
    int res = m_dsmcc->GetDSMCCObject(path, result);
    return res == 0; // It's available now.
}

bool MHIContext::GetDSMCCObject(const QString &objectPath, QByteArray &result)
{
    QStringList path = objectPath.split(QChar('/'), Qt::SkipEmptyParts);
    QMutexLocker locker(&m_dsmccLock);
    int res = m_dsmcc->GetDSMCCObject(path, result);
    return (res == 0);
}

bool MHIContext::CheckAccess(const QString &objectPath, QByteArray &cert)
{
    cert.clear();

    // Verify access to server
    QByteArray servers;
    if (!GetDSMCCObject("/auth.servers", servers))
    {
        LOG(VB_MHEG, LOG_INFO, QString(
            "[mhi] CheckAccess(%1) No auth.servers").arg(objectPath) );
        return false;
    }

    QByteArray host = QUrl(objectPath).host().toLocal8Bit();
    if (!servers.contains(host))
    {
        LOG(VB_MHEG, LOG_INFO, QString("[mhi] CheckAccess(%1) Host not known")
            .arg(objectPath) );
        LOG(VB_MHEG, LOG_DEBUG, QString("[mhi] Permitted servers: %1")
            .arg(servers.constData()) );

        // BUG: https://securegate.iplayer.bbc.co.uk is not listed
        if (!objectPath.startsWith("https:"))
            return false;
    }

    if (!objectPath.startsWith("https:"))
        return true;

    // Use TLS cert from carousel file auth.tls.<x>
    if (!GetDSMCCObject("/auth.tls.1", cert))
        return false;

    // The cert has a 5 byte header: 16b cert_count + 24b cert_len
    cert = cert.mid(5);
    return true;
}

// Called by the engine to request data from the carousel.
// Caller must hold m_runLock
bool MHIContext::GetCarouselData(const QString& objectPath, QByteArray &result)
{
    QByteArray cert;
    bool const isIC = objectPath.startsWith("http:") || objectPath.startsWith("https:");
    if (isIC)
    {
        // Verify access to server
        if (!CheckAccess(objectPath, cert))
            return false;
    }

    // Get the path components.  The string will normally begin with "//"
    // since this is an absolute path but that will be removed by split.
    QStringList path = objectPath.split(QChar('/'), Qt::SkipEmptyParts);
    // Since the DSMCC carousel and the MHEG engine are currently on the
    // same thread this is safe.  Otherwise we need to make a deep copy of
    // the result.

    bool bReported = false;
    QElapsedTimer t; t.start();
    while (!m_stop)
    {
        if (isIC)
        {
            switch (m_ic.GetFile(objectPath, result, cert))
            {
            case MHInteractionChannel::kSuccess:
                if (bReported)
                    LOG(VB_MHEG, LOG_INFO, QString("[mhi] Received %1").arg(objectPath));
                return true;
            case MHInteractionChannel::kError:
                if (bReported)
                    LOG(VB_MHEG, LOG_INFO, QString("[mhi] Not found %1").arg(objectPath));
                return false;
            case MHInteractionChannel::kPending:
                break;
            }
        }
        else
        {
            QMutexLocker locker(&m_dsmccLock);
            int res = m_dsmcc->GetDSMCCObject(path, result);
            if (res == 0)
            {
                if (bReported)
                    LOG(VB_MHEG, LOG_INFO, QString("[mhi] Received %1").arg(objectPath));
                return true; // Found it
            }
            // NB don't exit if -1 (not present) is returned as the object may
            // arrive later.  Exiting can cause the inital app to not be found
        }

        if (t.hasExpired(60000)) // TODO get this from carousel info
        {
            if (bReported)
                LOG(VB_MHEG, LOG_INFO, QString("[mhi] timed out %1").arg(objectPath));
            return false; // Not there.
        }
        // Otherwise we block.
        if (!bReported)
        {
            bReported = true;
            LOG(VB_MHEG, LOG_INFO, QString("[mhi] Waiting for %1").arg(objectPath));
        }
        // Process DSMCC packets then block for a while or until we receive
        // some more packets.  We should eventually find out if this item is
        // present.
        ProcessDSMCCQueue();
        m_engineWait.wait(&m_runLock, 300);
    }
    return false; // Stop has been set.  Say the object isn't present.
}

// Mapping from key name & UserInput register to UserInput EventData
class MHKeyLookup
{
    using key_t = QPair< QString, int /*UserInput register*/ >;

public:
    MHKeyLookup();

    int Find(const QString &name, int reg) const
        { return m_map.value(key_t(name,reg), 0); }

private:
    void key(const QString &name, int code, int r1,
        int r2=0, int r3=0, int r4=0, int r5=0, int r6=0, int r7=0, int r8=0, int r9=0);

    QHash<key_t,int /*EventData*/ > m_map;
};

void MHKeyLookup::key(const QString &name, int code, int r1,
    int r2, int r3, int r4, int r5, int r6, int r7, int r8, int r9)
{
    if (r1 > 0)
      m_map.insert(key_t(name,r1), code);
    if (r2 > 0)
        m_map.insert(key_t(name,r2), code);
    if (r3 > 0)
        m_map.insert(key_t(name,r3), code);
    if (r4 > 0)
        m_map.insert(key_t(name,r4), code);
    if (r5 > 0)
        m_map.insert(key_t(name,r5), code);
    if (r6 > 0)
        m_map.insert(key_t(name,r6), code);
    if (r7 > 0)
        m_map.insert(key_t(name,r7), code);
    if (r8 > 0)
        m_map.insert(key_t(name,r8), code);
    if (r9 > 0)
        m_map.insert(key_t(name,r9), code);
}

MHKeyLookup::MHKeyLookup()
{
    // Use a modification of the standard key mapping for RC's with a single
    // stop button which is used for both Esc and TEXTEXIT (Back).
    // This mapping doesn't pass Esc to the MHEG app in registers 3 or 5 and
    // hence allows the user to exit playback when the red button icon is shown
    QStringList keylist = GET_KEY("TV Playback", "TEXTEXIT").split(QChar(','));
    bool strict = !keylist.contains("Esc", Qt::CaseInsensitive);

    // This supports the UK and NZ key profile registers.
    // The UK uses 3, 4 and 5 and NZ 13, 14 and 15.  These are
    // similar but the NZ profile also provides an EPG key.
    // ETSI ES 202 184 V2.2.1 (2011-03) adds group 6 for ICE.
    // The BBC use group 7 for ICE
    key(ACTION_UP,           1, 4,5,6,7,14,15);
    key(ACTION_DOWN,         2, 4,5,6,7,14,15);
    key(ACTION_LEFT,         3, 4,5,6,7,14,15);
    key(ACTION_RIGHT,        4, 4,5,6,7,14,15);
    key(ACTION_0,            5, 4,6,7,14);
    key(ACTION_1,            6, 4,6,7,14);
    key(ACTION_2,            7, 4,6,7,14);
    key(ACTION_3,            8, 4,6,7,14);
    key(ACTION_4,            9, 4,6,7,14);
    key(ACTION_5,           10, 4,6,7,14);
    key(ACTION_6,           11, 4,6,7,14);
    key(ACTION_7,           12, 4,6,7,14);
    key(ACTION_8,           13, 4,6,7,14);
    key(ACTION_9,           14, 4,6,7,14);
    key(ACTION_SELECT,      15, 4,5,6,7,14,15);
    key(ACTION_TEXTEXIT,    16, strict ? 3 : 0,4,strict ? 5 : 0,6,7,13,14,15); // 16= Cancel
    // 17= help
    // 18..99 reserved by DAVIC
    key(ACTION_MENURED,    100, 3,4,5,6,7,13,14,15);
    key(ACTION_MENUGREEN,  101, 3,4,5,6,7,13,14,15);
    key(ACTION_MENUYELLOW, 102, 3,4,5,6,7,13,14,15);
    key(ACTION_MENUBLUE,   103, 3,4,5,6,7,13,14,15);
    key(ACTION_MENUTEXT,   104, 3,4,5,6,7);
    key(ACTION_MENUTEXT,   105, 13,14,15); // NB from original Myth code
    // 105..119 reserved for future spec
    key(ACTION_STOP,       120, 6,7);
    key(ACTION_PLAY,       121, 6,7);
    key(ACTION_PAUSE,      122, 6,7);
    key(ACTION_JUMPFFWD,   123, 6,7); // 123= Skip Forward
    key(ACTION_JUMPRWND,   124, 6,7); // 124= Skip Back
#if 0 // These conflict with left & right
    key(ACTION_SEEKFFWD,   125, 6,7); // 125= Fast Forward
    key(ACTION_SEEKRWND,   126, 6,7); // 126= Rewind
#endif
    key(ACTION_PLAYBACK,   127, 6,7);
    // 128..256 reserved for future spec
    // 257..299 vendor specific
    key(ACTION_MENUEPG,    300, 13,14,15);
    // 301.. Vendor specific
}

// Called from tv_play when a key is pressed.
// If it is one in the current profile we queue it for the engine
// and return true otherwise we return false.
bool MHIContext::OfferKey(const QString& key)
{
    static const MHKeyLookup kKeymap;
    int action = kKeymap.Find(key, m_keyProfile);
    if (action == 0)
        return false;

    LOG(VB_GENERAL, LOG_INFO, QString("[mhi] Adding MHEG key %1:%2:%3")
        .arg(key).arg(action).arg(m_keyQueue.size()) );
    { QMutexLocker locker(&m_keyLock);
    m_keyQueue.enqueue(action);}
    m_engineWait.wakeAll();
    return true;
}

// Called from MythPlayer::VideoStart and MythPlayer::ReinitOSD
void MHIContext::Reinit(const QRect videoRect, const QRect dispRect, float aspect)
{
    LOG(VB_MHEG, LOG_INFO,
         QString("[mhi] Reinit video(y:%1 x:%2 w:%3 h:%4) "
            "vis(y:%5 x:%6 w:%7 h:%8) aspect=%9")
        .arg(videoRect.y()).arg(videoRect.x())
        .arg(videoRect.width()).arg(videoRect.height())
        .arg(dispRect.y()).arg(dispRect.x())
        .arg(dispRect.width()).arg(dispRect.height()).arg(aspect));
    m_videoDisplayRect = QRect();

    // MHEG presumes square pixels
    enum : std::uint8_t { kNone, kHoriz, kBoth };
    int mode = gCoreContext->GetNumSetting("MhegAspectCorrection", kNone);
    auto const aspectd = static_cast<double>(aspect);
    double const vz = (mode == kBoth) ? std::min(1.15, 1. / sqrt(aspectd)) : 1.;
    double const hz = (mode > kNone) ? vz * aspectd : 1.;

    m_displayRect = QRect( int(dispRect.width() * (1 - hz) / 2),
        int(dispRect.height() * (1 - vz) / 2),
        int(dispRect.width() * hz), int(dispRect.height() * vz) );
    m_videoRect = QRect( dispRect.x() + m_displayRect.x(),
        dispRect.y() + int(dispRect.height() * (1 - hz) / 2),
        int(dispRect.width() * hz), int(dispRect.height() * hz) );
}

void MHIContext::SetInputRegister(int num)
{
    LOG(VB_MHEG, LOG_INFO, QString("[mhi] SetInputRegister %1").arg(num));
    QMutexLocker locker(&m_keyLock);
    m_keyQueue.clear();
    m_keyProfile = num;
}

int MHIContext::GetICStatus()
{
   // 0= Active, 1= Inactive, 2= Disabled
    return MHInteractionChannel::status();
}

// Called by the video player to redraw the image.
void MHIContext::UpdateOSD(InteractiveScreen *osdWindow,
                           MythPainter *osdPainter)
{
    if (!osdWindow || !osdPainter)
        return;

    QMutexLocker locker(&m_displayLock);

    // In MHEG the video is just another item in the display stack
    // but when we create the OSD we overlay everything over the video.
    // We need to cut out anything belowthe video on the display stack
    // to leave the video area clear.
    auto it = m_display.begin();
    for (; it != m_display.end(); ++it)
    {
        MHIImageData *data = *it;
        if (!data->m_bUnder)
            continue;

        QRect imageRect(data->m_x, data->m_y,
                        data->m_image.width(), data->m_image.height());
        if (!m_videoDisplayRect.intersects(imageRect))
            continue;

        // Replace this item with a set of cut-outs.
        it = m_display.erase(it);

        for (const QRect& rect : QRegion(imageRect)-QRegion(m_videoDisplayRect))
        {
            QImage image =
                data->m_image.copy(rect.x()-data->m_x, rect.y()-data->m_y,
                                   rect.width(), rect.height());
            auto *newData = new MHIImageData;
            newData->m_image = image;
            newData->m_x = rect.x();
            newData->m_y = rect.y();
            newData->m_bUnder = true;
            it = m_display.insert(it, newData);
            ++it;
        }
        --it;
        delete data;
    }

    m_updated = false;
    osdWindow->DeleteAllChildren();
    // Copy all the display items into the display.
    it = m_display.begin();
    for (int count = 0; it != m_display.end(); ++it, count++)
    {
        MHIImageData *data = *it;
        MythImage* image = osdPainter->GetFormatImage();
        if (!image)
            continue;

        image->Assign(data->m_image);
        auto *uiimage = new MythUIImage(osdWindow, QString("itv%1").arg(count));
        if (uiimage)
        {
            uiimage->SetImage(image);
            uiimage->SetArea(MythRect(data->m_x, data->m_y,
                             data->m_image.width(), data->m_image.height()));
        }
        image->DecrRef();
    }
    osdWindow->OptimiseDisplayedArea();
    // N.B. bypasses OSD class hence no expiry set
    osdWindow->SetVisible(true);
}

void MHIContext::GetInitialStreams(int &audioTag, int &videoTag) const
{
    audioTag = m_audioTag;
    videoTag = m_videoTag;
}


// An area of the screen/image needs to be redrawn.
// Called from the MHEG engine.
// We always redraw the whole scene.
void MHIContext::RequireRedraw(const QRegion & /*region*/)
{
    m_updated = false;
    m_displayLock.lock();
    ClearDisplay();
    m_displayLock.unlock();
    // Always redraw the whole screen
    m_engine->DrawDisplay(QRegion(0, 0, kStdDisplayWidth, kStdDisplayHeight));
    m_updated = true;
}

inline int MHIContext::ScaleX(int n, bool roundup) const
{
    return (n * m_displayRect.width() + (roundup ? kStdDisplayWidth - 1 : 0)) / kStdDisplayWidth;
}

inline int MHIContext::ScaleY(int n, bool roundup) const
{
    return (n * m_displayRect.height() + (roundup ? kStdDisplayHeight - 1 : 0)) / kStdDisplayHeight;
}

inline QRect MHIContext::Scale(const QRect r) const
{
    return { m_displayRect.topLeft() + QPoint(ScaleX(r.x()), ScaleY(r.y())),
             QSize(ScaleX(r.width(), true), ScaleY(r.height(), true)) };
}

inline int MHIContext::ScaleVideoX(int n, bool roundup) const
{
    return (n * m_videoRect.width() + (roundup ? kStdDisplayWidth - 1 : 0)) / kStdDisplayWidth;
}

inline int MHIContext::ScaleVideoY(int n, bool roundup) const
{
    return (n * m_videoRect.height() + (roundup ? kStdDisplayHeight - 1 : 0)) / kStdDisplayHeight;
}

inline QRect MHIContext::ScaleVideo(const QRect r) const
{
    return { m_videoRect.topLeft() + QPoint(ScaleVideoX(r.x()), ScaleVideoY(r.y())),
             QSize(ScaleVideoX(r.width(), true), ScaleVideoY(r.height(), true)) };
}

void MHIContext::AddToDisplay(const QImage &image, const QRect displayRect, bool bUnder /*=false*/)
{
    const QRect scaledRect = Scale(displayRect);

    auto *data = new MHIImageData;

    data->m_image = image.convertToFormat(QImage::Format_ARGB32).scaled(
        scaledRect.width(), scaledRect.height(),
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    data->m_x = scaledRect.x();
    data->m_y = scaledRect.y();
    data->m_bUnder = bUnder;

    QMutexLocker locker(&m_displayLock);
    if (!bUnder)
        m_display.push_back(data);
    else
    {
        // Replace any existing items under the video with this
        auto it = m_display.begin();
        while (it != m_display.end())
        {
            MHIImageData *old = *it;
            if (!old->m_bUnder)
                ++it;
            else
            {
                it = m_display.erase(it);
                delete old;
            }
        }
        m_display.push_front(data);
    }
}

inline int Roundup(int n, int r)
{
    // NB assumes 2's complement arithmetic
    return n + (-n & (r - 1));
}

// The videoRect gives the size and position to which the video must be scaled.
// The displayRect gives the rectangle reserved for the video.
// e.g. part of the video may be clipped within the displayRect.
void MHIContext::DrawVideo(const QRect &videoRect, const QRect &dispRect)
{
    // tell the video player to resize the video stream
    if (m_parent->GetPlayer())
    {
        QRect vidRect;
        if (videoRect != QRect(QPoint(0,0),QSize(kStdDisplayWidth,kStdDisplayHeight)))
        {
            vidRect = ScaleVideo(videoRect);
            vidRect.setWidth(Roundup(vidRect.width(), 2));
            vidRect.setHeight(Roundup(vidRect.height(), 2));
        }
        emit m_parent->GetPlayer()->ResizeForInteractiveTV(vidRect);
    }

    m_videoDisplayRect = Scale(dispRect);

    // Mark all existing items in the display stack as under the video
    QMutexLocker locker(&m_displayLock);
    for (auto & it : m_display)
        it->m_bUnder = true;
}

// Caller must hold m_channelMutex
bool MHIContext::LoadChannelCache()
{
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT networkid, serviceid, transportid, chanid "
        "FROM channel, dtv_multiplex "
        "WHERE channel.deleted  IS NULL "
        "  AND channel.mplexid  = dtv_multiplex.mplexid "
        "  AND channel.sourceid = dtv_multiplex.sourceid "
        "  AND channel.sourceid = :SOURCEID ;" );
    query.bindValue(":SOURCEID", m_currentSource);
    if (!query.exec())
    {
        MythDB::DBError("MHIContext::LoadChannelCache", query);
        return false;
    }
    if (!query.isActive())
        return false;
    while (query.next())
    {
        int nid = query.value(0).toInt();
        int sid = query.value(1).toInt();
        int tid = query.value(2).toInt();
        int cid = query.value(3).toInt();
        m_channelCache.insert( Key_t(nid, sid), Val_t(tid, cid) );
    }
    return true;
}

// Tuning.  Get the index corresponding to a given channel.
// The format of the service is dvb://netID.[transPortID].serviceID
// where the IDs are in hex.
// or rec://svc/lcn/N where N is the "logical channel number"
// i.e. the Freeview channel.
// Returns -1 if it cannot find it.
int MHIContext::GetChannelIndex(const QString &str)
{
    int nResult = -1;

    for (int i = 0; i < 1 ; i++) // do once
    {
        if (str.startsWith("dvb://"))
        {
            QStringList list = str.mid(6).split('.');
            if (list.size() != 3)
                break; // Malformed.
            // The various fields are expressed in hexadecimal.
            // Convert them to decimal for the DB.
            bool ok = false;
            int netID = list[0].toInt(&ok, 16);
            if (!ok)
                break;
            int transportID = !list[1].isEmpty() ? list[1].toInt(&ok, 16) : -1;
            if (!ok)
                break;
            int serviceID = list[2].toInt(&ok, 16);
            if (!ok)
                break;

            QMutexLocker locker(&m_channelMutex);
            if (m_channelCache.isEmpty())
                LoadChannelCache();

            ChannelCache_t::const_iterator it = m_channelCache.constFind(
                Key_t(netID,serviceID) );
            if (it == m_channelCache.constEnd())
                break;
            if (transportID < 0)
                nResult = Cid(it);
            else
            {
                for ( ; it != m_channelCache.constEnd() ; it++)
                {
                    if (Tid(it) == transportID)
                    {
                        nResult = Cid(it);
                        break;
                    }
                }
            }
        }
        else if (str.startsWith("rec://svc/lcn/"))
        {
            // I haven't seen this yet so this is untested.
            bool ok = false;
            int channelNo = str.mid(14).toInt(&ok); // Decimal integer
            if (!ok)
                break;
            MSqlQuery query(MSqlQuery::InitCon());
            query.prepare("SELECT chanid "
                          "FROM channel "
                          "WHERE deleted IS NULL AND "
                          "      channum = :CHAN AND "
                          "      channel.sourceid = :SOURCEID");
            query.bindValue(":CHAN", channelNo);
            query.bindValue(":SOURCEID", m_currentSource);
            if (query.exec() && query.isActive() && query.next())
                nResult = query.value(0).toInt();
        }
        else if (str == "rec://svc/cur")
        {
            nResult = m_currentStream > 0 ? m_currentStream : m_currentChannel;
        }
        else if (str == "rec://svc/def")
        {
            nResult = m_currentChannel;
        }
        else
        {
            LOG(VB_GENERAL, LOG_WARNING,
                QString("[mhi] GetChannelIndex -- Unrecognized URL %1")
                .arg(str));
        }
    }

    LOG(VB_MHEG, LOG_INFO, QString("[mhi] GetChannelIndex %1 => %2")
        .arg(str).arg(nResult));
    return nResult;

}

// Get netId etc from the channel index.  This is the inverse of GetChannelIndex.
bool MHIContext::GetServiceInfo(int channelId, int &netId, int &origNetId,
                                int &transportId, int &serviceId)
{
    QMutexLocker locker(&m_channelMutex);
    if (m_channelCache.isEmpty())
        LoadChannelCache();

    for (auto it = m_channelCache.cbegin(); it != m_channelCache.cend(); ++it)
    {
        if (Cid(it) == channelId)
        {
            transportId = Tid(it);
            netId = Nid(it);
            origNetId = netId; // We don't have this in the database.
            serviceId = Sid(it);
            LOG(VB_MHEG, LOG_INFO, QString("[mhi] GetServiceInfo %1 => NID=%2 TID=%3 SID=%4")
                .arg(channelId).arg(netId).arg(transportId).arg(serviceId));
            return true;
        }
    }

    LOG(VB_MHEG, LOG_WARNING, QString("[mhi] GetServiceInfo %1 failed").arg(channelId));
    return false;
}

bool MHIContext::TuneTo(int channel, int tuneinfo)
{
    if (!m_isLive)
    {
        LOG(VB_MHEG, LOG_WARNING, QString("[mhi] Can't TuneTo %1 0x%2 while not live")
            .arg(channel).arg(tuneinfo,0,16));
        return false; // Can't tune if this is a recording.
    }

    LOG(VB_GENERAL, LOG_INFO, QString("[mhi] TuneTo %1 0x%2")
        .arg(channel).arg(tuneinfo,0,16));
    m_tuneInfo.append(tuneinfo);

    // Post an event requesting a channel change.
    MythEvent me(QString("NETWORK_CONTROL CHANID %1").arg(channel));
    gCoreContext->dispatch(me);
    // Reset the NBI version here to prevent a reboot.
    QMutexLocker locker(&m_dsmccLock);
    m_lastNbiVersion = NBI_VERSION_UNSET;
    m_nbiData.resize(0);
    return true;
}


// Begin playing the specified stream
bool MHIContext::BeginStream(const QString &stream, MHStream *notify)
{
    LOG(VB_MHEG, LOG_INFO, QString("[mhi] BeginStream %1 0x%2")
        .arg(stream).arg((quintptr)notify,0,16));

    m_audioTag = -1;
    m_videoTag = -1;
    m_notify = notify;

    if (stream.startsWith("http://") || stream.startsWith("https://"))
    {
        m_currentStream = -1;

        // The url is sometimes only http:// during stream startup
        if (QUrl(stream).authority().isEmpty())
            return false;

        emit m_parent->GetPlayer()->SetInteractiveStream(stream);
        return !stream.isEmpty();
    }

    int chan = GetChannelIndex(stream);
    if (chan < 0)
        return false;
    if (VERBOSE_LEVEL_CHECK(VB_MHEG, LOG_ANY))
    {
        int netId = 0;
        int origNetId = 0;
        int transportId = 0;
        int serviceId = 0;
        GetServiceInfo(chan, netId, origNetId, transportId, serviceId);
    }

    if (chan != m_currentStream)
    {
        // We have to tune to the channel where the stream is to be found.
        // Because the audio and video are both components of an MHEG stream
        // they will both be on the same channel.
        m_currentStream = chan;
        return TuneTo(chan, kTuneKeepChnl|kTuneQuietly|kTuneKeepApp);
    }

    return true;
}

void MHIContext::EndStream()
{
    LOG(VB_MHEG, LOG_INFO, QString("[mhi] EndStream 0x%1")
        .arg((quintptr)m_notify,0,16) );

    m_notify = nullptr;
    emit m_parent->GetPlayer()->SetInteractiveStream(QString());
}

// Callback from MythPlayer when a stream starts or stops
bool MHIContext::StreamStarted(bool bStarted)
{
    if (!m_engine || !m_notify)
        return false;

    LOG(VB_MHEG, LOG_INFO, QString("[mhi] Stream 0x%1 %2")
        .arg((quintptr)m_notify,0,16).arg(bStarted ? "started" : "stopped"));

    QMutexLocker locker(&m_runLock);
    m_engine->StreamStarted(m_notify, bStarted);
    if (!bStarted)
        m_notify = nullptr;
    return m_currentStream == -1; // Return true if it's an http stream
}

// Begin playing audio
bool MHIContext::BeginAudio(int tag)
{
    LOG(VB_MHEG, LOG_INFO, QString("[mhi] BeginAudio %1").arg(tag));

    if (tag < 0)
        return true; // Leave it at the default.

    m_audioTag = tag;
    if (m_parent->GetPlayer())
        return m_parent->GetPlayer()->SetAudioByComponentTag(tag);
    return false;
 }

// Stop playing audio
void MHIContext::StopAudio()
{
    // Do nothing at the moment.
}

// Begin displaying video from the specified stream
bool MHIContext::BeginVideo(int tag)
{
    LOG(VB_MHEG, LOG_INFO, QString("[mhi] BeginVideo %1").arg(tag));

    if (tag < 0)
        return true; // Leave it at the default.

    m_videoTag = tag;
    if (m_parent->GetPlayer())
        return m_parent->GetPlayer()->SetVideoByComponentTag(tag);
    return false;
}

 // Stop displaying video
void MHIContext::StopVideo()
{
    // Do nothing at the moment.
}

// Get current stream position, -1 if unknown
std::chrono::milliseconds MHIContext::GetStreamPos()
{
    return m_parent->GetPlayer() ? m_parent->GetPlayer()->GetStreamPos() : -1ms;
}

// Get current stream size, -1 if unknown
std::chrono::milliseconds MHIContext::GetStreamMaxPos()
{
    return m_parent->GetPlayer() ? m_parent->GetPlayer()->GetStreamMaxPos() : -1ms;
}

// Set current stream position
std::chrono::milliseconds MHIContext::SetStreamPos(std::chrono::milliseconds pos)
{
    if (m_parent->GetPlayer())
        emit m_parent->GetPlayer()->SetInteractiveStreamPos(pos);
    // Note: return value is never used
    return 0ms;
}

// Play or pause a stream
void MHIContext::StreamPlay(bool play)
{
    if (m_parent->GetPlayer())
        emit m_parent->GetPlayer()->PlayInteractiveStream(play);
}

// Create a new object to draw dynamic line art.
MHDLADisplay *MHIContext::CreateDynamicLineArt(
    bool isBoxed, MHRgba lineColour, MHRgba fillColour)
{
    return new MHIDLA(this, isBoxed, lineColour, fillColour);
}

// Create a new object to draw text.
MHTextDisplay *MHIContext::CreateText()
{
    return new MHIText(this);
}

// Create a new object to draw bitmaps.
MHBitmapDisplay *MHIContext::CreateBitmap(bool tiled)
{
    return new MHIBitmap(this, tiled);
}

// Draw a rectangle.  This is complicated if we want to get transparency right.
void MHIContext::DrawRect(int xPos, int yPos, int width, int height,
                          MHRgba colour)
{
    if (colour.alpha() == 0 || height == 0 || width == 0)
        return; // Fully transparent

    QImage qImage(width, height, QImage::Format_ARGB32);
    qImage.fill(qRgba(colour.red(), colour.green(), colour.blue(), colour.alpha()));

    AddToDisplay(qImage, QRect(xPos, yPos, width, height));
}

// Draw an image at the specified position.
// Generally the whole of the image is drawn but sometimes the
// image may be clipped. x and y define the origin of the bitmap
// and usually that will be the same as the origin of the bounding
// box (clipRect).
void MHIContext::DrawImage(int x, int y, const QRect clipRect,
                           const QImage &qImage, bool bScaled, bool bUnder)
{
    if (qImage.isNull())
        return;

    QRect imageRect(x, y, qImage.width(), qImage.height());
    QRect displayRect = clipRect & imageRect;

    if (bScaled || displayRect == imageRect) // No clipping required
    {
        AddToDisplay(qImage, displayRect, bUnder);
    }
    else if (!displayRect.isEmpty())
    { // We must clip the image.
        QImage clipped = qImage.copy(displayRect.translated(-x, -y));
        AddToDisplay(clipped, displayRect, bUnder);
    }
    // Otherwise draw nothing.
}

// Fill in the background.  This is only called if there is some area of
// the screen that is not covered with other visibles.
void MHIContext::DrawBackground(const QRegion &reg)
{
    if (reg.isEmpty())
        return;

    QRect bounds = reg.boundingRect();
    DrawRect(bounds.x(), bounds.y(), bounds.width(), bounds.height(),
             MHRgba(0, 0, 0, 255)/* black. */);
}

void MHIText::Draw(int x, int y)
{
    m_parent->DrawImage(x, y, QRect(x, y, m_width, m_height), m_image);
}

void MHIText::SetSize(int width, int height)
{
    m_width = width;
    m_height = height;
}

void MHIText::SetFont(int size, bool isBold, bool isItalic)
{
    m_fontSize = size;
    m_fontItalic = isItalic;
    m_fontBold = isBold;
    // TODO: Only the size is currently used.
    // Bold and Italic are currently ignored.
}

// FT sizes are in 26.6 fixed point form
const int kShift = 6;
static inline FT_F26Dot6 Point2FT(int pt)
{
    return pt << kShift;
}

static inline int FT2Point(FT_F26Dot6 fp)
{
    return (fp + (1<<(kShift-1))) >> kShift;
}

// Return the bounding rectangle for a piece of text drawn in the
// current font. If maxSize is non-negative it sets strLen to the
// number of characters that will fit in the space and returns the
// bounds for those characters.
// N.B.  The box is relative to the origin so the y co-ordinate will
// be negative. It's also possible that the x co-ordinate could be
// negative for slanted fonts but that doesn't currently happen.
QRect MHIText::GetBounds(const QString &str, int &strLen, int maxSize)
{
    if (!m_parent->IsFaceLoaded())
        return {0,0,0,0};

    FT_Face face = m_parent->GetFontFace();
    FT_Error error = FT_Set_Char_Size(face, 0, Point2FT(m_fontSize),
                                      FONT_WIDTHRES, FONT_HEIGHTRES);
    if (error)
        return {0,0,0,0};

    int maxAscent  =  face->size->metrics.ascender;
    int maxDescent = -face->size->metrics.descender;
    int width = 0;
    FT_Bool useKerning = FT_HAS_KERNING(face);
    FT_UInt previous = 0;

    for (int n = 0; n < strLen; n++)
    {
        QChar ch = str.at(n);
        FT_UInt glyphIndex = FT_Get_Char_Index(face, ch.unicode());

        if (glyphIndex == 0)
        {
            LOG(VB_MHEG, LOG_INFO, QString("[mhi] Unknown glyph 0x%1")
                .arg(static_cast<short>(ch.unicode()),0,16));
            previous = 0;
            continue;
        }

        int kerning = 0;

        if (useKerning && previous != 0)
        {
            FT_Vector delta;
            FT_Get_Kerning(face, previous, glyphIndex,
                           FT_KERNING_DEFAULT, &delta);
            kerning = delta.x;
        }

        error = FT_Load_Glyph(face, glyphIndex, 0); // Don't need to render.

        if (error)
            continue; // ignore errors.

        FT_GlyphSlot slot = face->glyph; /* a small shortcut */
        FT_Pos advance = slot->metrics.horiAdvance + kerning;

        if (maxSize >= 0)
        {
            if (FT2Point(width + advance) > maxSize)
            {
                // There isn't enough space for this character.
                strLen = n;
                break;
            }
        }
        // Calculate the ascent and descent of this glyph.
        int descent = slot->metrics.height - slot->metrics.horiBearingY;

        maxAscent = std::max<FT_Pos>(slot->metrics.horiBearingY, maxAscent);

        maxDescent = std::max(descent, maxDescent);

        width += advance;
        previous = glyphIndex;
    }

    return {0, -FT2Point(maxAscent), FT2Point(width), FT2Point(maxAscent + maxDescent)};
}

// Reset the image and fill it with transparent ink.
// The UK MHEG profile says that we should consider the background
// as paper and the text as ink.  We have to consider these as two
// different layers.  The background is drawn separately as a rectangle.
void MHIText::Clear(void)
{
    m_image = QImage(m_width, m_height, QImage::Format_ARGB32);
    // QImage::fill doesn't set the alpha buffer.
    for (int i = 0; i < m_height; i++)
    {
        for (int j = 0; j < m_width; j++)
        {
            m_image.setPixel(j, i, qRgba(0, 0, 0, 0));
        }
    }
}

// Draw a line of text in the given position within the image.
// It would be nice to be able to use TTFFont for this but it doesn't provide
// what we want.
void MHIText::AddText(int x, int y, const QString &str, MHRgba colour)
{
    if (!m_parent->IsFaceLoaded()) return;
    FT_Face face = m_parent->GetFontFace();

    FT_Set_Char_Size(face, 0, Point2FT(m_fontSize),
                                      FONT_WIDTHRES, FONT_HEIGHTRES);

    // X positions are computed to 64ths and rounded.
    // Y positions are in pixels
    int posX = Point2FT(x);
    int pixelY = y;
    FT_Bool useKerning = FT_HAS_KERNING(face);
    FT_UInt previous = 0;

    int len = str.length();
    for (int n = 0; n < len; n++)
    {
        // Load the glyph.
        QChar ch = str[n];
        FT_UInt glyphIndex = FT_Get_Char_Index(face, ch.unicode());
        if (glyphIndex == 0)
        {
            previous = 0;
            continue;
        }

        if (useKerning && previous != 0)
        {
            FT_Vector delta;
            FT_Get_Kerning(face, previous, glyphIndex,
                           FT_KERNING_DEFAULT, &delta);
            posX += delta.x;
        }
        FT_Error error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER);

        if (error)
            continue; // ignore errors

        FT_GlyphSlot slot = face->glyph;
        if (slot->format != FT_GLYPH_FORMAT_BITMAP)
            continue; // Problem

        if ((enum FT_Pixel_Mode_)slot->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
            continue;

        unsigned char *source = slot->bitmap.buffer;
        // Get the origin for the bitmap
        int baseX = FT2Point(posX) + slot->bitmap_left;
        int baseY = pixelY - slot->bitmap_top;
        // Copy the bitmap into the image.
        for (unsigned int i = 0; i < slot->bitmap.rows; i++)
        {
            for (unsigned int j = 0; j < slot->bitmap.width; j++)
            {
                int greyLevel = source[j];
                // Set the pixel to the specified colour but scale its
                // brightness according to the grey scale of the pixel.
                int red = colour.red();
                int green = colour.green();
                int blue = colour.blue();
                int alpha = colour.alpha() *
                    greyLevel / slot->bitmap.num_grays;
                int xBit =  j + baseX;
                int yBit =  i + baseY;

                // The bits ought to be inside the bitmap but
                // I guess there's the possibility
                // that rounding might put it outside.
                if (xBit >= 0 && xBit < m_width &&
                    yBit >= 0 && yBit < m_height)
                {
                    m_image.setPixel(xBit, yBit,
                                     qRgba(red, green, blue, alpha));
                }
            }
            source += slot->bitmap.pitch;
        }
        posX += slot->advance.x;
        previous = glyphIndex;
    }
}

// Internal function to fill a rectangle with a colour
void MHIDLA::DrawRect(int x, int y, int width, int height, MHRgba colour)
{
    QRgb qColour = qRgba(colour.red(), colour.green(),
                         colour.blue(), colour.alpha());

    // Constrain the drawing within the image.
    if (x < 0)
    {
        width += x;
        x = 0;
    }

    if (y < 0)
    {
        height += y;
        y = 0;
    }

    if (width <= 0 || height <= 0)
        return;

    int imageWidth = m_image.width();
    int imageHeight = m_image.height();
    if (x+width > imageWidth)
        width = imageWidth - x;

    if (y+height > imageHeight)
        height = imageHeight - y;

    if (width <= 0 || height <= 0)
        return;

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            m_image.setPixel(x+j, y+i, qColour);
        }
    }
}

// Reset the drawing.
void MHIDLA::Clear()
{
    if (m_width == 0 || m_height == 0)
    {
        m_image = QImage();
        return;
    }
    m_image = QImage(m_width, m_height, QImage::Format_ARGB32);
    // Fill the image with "transparent colour".
    DrawRect(0, 0, m_width, m_height, MHRgba(0, 0, 0, 0));
}

void MHIDLA::Draw(int x, int y)
{
    QRect bounds(x, y, m_width, m_height);
    if (m_boxed && m_lineWidth != 0)
    {
        // Draw the lines round the outside.
        // These don't form part of the drawing.
        m_parent->DrawRect(x, y, m_width,
                           m_lineWidth, m_boxLineColour);

        m_parent->DrawRect(x, y + m_height - m_lineWidth,
                           m_width, m_lineWidth, m_boxLineColour);

        m_parent->DrawRect(x, y + m_lineWidth,
                           m_lineWidth, m_height - (m_lineWidth * 2),
                           m_boxLineColour);

        m_parent->DrawRect(x + m_width - m_lineWidth, y + m_lineWidth,
                           m_lineWidth, m_height - (m_lineWidth * 2),
                           m_boxLineColour);

        // Deflate the box to within the border.
        bounds = QRect(bounds.x() + m_lineWidth,
                       bounds.y() + m_lineWidth,
                       bounds.width() - (2*m_lineWidth),
                       bounds.height() - (2*m_lineWidth));
    }

    // Draw the background.
    m_parent->DrawRect(x + m_lineWidth,
                       y + m_lineWidth,
                       m_width  - (m_lineWidth * 2),
                       m_height - (m_lineWidth * 2),
                       m_boxFillColour);

    // Now the drawing.
    m_parent->DrawImage(x, y, bounds, m_image);
}

// The UK MHEG profile defines exactly how transparency is supposed to work.
// The drawings are made using possibly transparent ink with any crossings
// just set to that ink and then the whole drawing is alpha-merged with the
// underlying graphics.
// DynamicLineArt no longer seems to be used in transmissions in the UK
// although it appears that DrawPoly is used in New Zealand.  These are
// very basic implementations of the functions.

// Lines
void MHIDLA::DrawLine(int x1, int y1, int x2, int y2)
{
    // Get the arguments so that the lower x comes first and the
    // absolute gradient is less than one.
    if (abs(y2-y1) > abs(x2-x1))
    {
        if (y2 > y1)
            DrawLineSub(y1, x1, y2, x2, true);
        else
            DrawLineSub(y2, x2, y1, x1, true);
    }
    else
    {
        if (x2 > x1)
            DrawLineSub(x1, y1, x2, y2, false);
        else
            DrawLineSub(x2, y2, x1, y1, false);
    }
}

// Based on the Bresenham line drawing algorithm but extended to draw
// thick lines.
void MHIDLA::DrawLineSub(int x1, int y1, int x2, int y2, bool swapped)
{
    QRgb colour = qRgba(m_lineColour.red(), m_lineColour.green(),
                         m_lineColour.blue(), m_lineColour.alpha());
    int dx = x2-x1;
    int dy = abs(y2-y1);
    int yStep = y2 >= y1 ? 1 : -1;
    // Adjust the starting positions to take account of the
    // line width.
    int error2 = dx/2;
    for (int k = 0; k < m_lineWidth/2; k++)
    {
        y1--;
        y2--;
        error2 += dy;
        if (error2*2 > dx)
        {
            error2 -= dx;
            x1 += yStep;
            x2 += yStep;
        }
    }
    // Main loop
    int y = y1;
    int error = dx/2;
    for (int x = x1; x <= x2; x++) // Include both endpoints
    {
        error2 = dx/2;
        int j = 0;
        // Inner loop also uses the Bresenham algorithm to draw lines
        // perpendicular to the principal direction.
        for (int i = 0; i < m_lineWidth; i++)
        {
            if (swapped)
            {
                if (x+j >= 0 && y+i >= 0 && y+i < m_width && x+j < m_height)
                    m_image.setPixel(y+i, x+j, colour);
            }
            else
            {
                if (x+j >= 0 && y+i >= 0 && x+j < m_width && y+i < m_height)
                    m_image.setPixel(x+j, y+i, colour);
            }
            error2 += dy;
            if (error2*2 > dx)
            {
                error2 -= dx;
                j -= yStep;
                if (i < m_lineWidth-1)
                {
                    // Add another pixel in this case.
                    if (swapped)
                    {
                        if (x+j >= 0 && y+i >= 0 && y+i < m_width && x+j < m_height)
                            m_image.setPixel(y+i, x+j, colour);
                    }
                    else
                    {
                        if (x+j >= 0 && y+i >= 0 && x+j < m_width && y+i < m_height)
                            m_image.setPixel(x+j, y+i, colour);
                    }
                }
            }
        }
        error += dy;
        if (error*2 > dx)
        {
            error -= dx;
            y += yStep;
        }
    }
}

// Rectangles
void MHIDLA::DrawBorderedRectangle(int x, int y, int width, int height)
{
    if (m_lineWidth != 0)
    {
        // Draw the lines round the rectangle.
        DrawRect(x, y, width, m_lineWidth,
                 m_lineColour);

        DrawRect(x, y + height - m_lineWidth,
                 width, m_lineWidth,
                 m_lineColour);

        DrawRect(x, y + m_lineWidth,
                 m_lineWidth, height - (m_lineWidth * 2),
                 m_lineColour);

        DrawRect(x + width - m_lineWidth, y + m_lineWidth,
                 m_lineWidth, height - (m_lineWidth * 2),
                 m_lineColour);

        // Fill the rectangle.
        DrawRect(x + m_lineWidth, y + m_lineWidth,
                 width - (m_lineWidth * 2), height - (m_lineWidth * 2),
                 m_fillColour);
    }
    else
    {
        DrawRect(x, y, width, height, m_fillColour);
    }
}

// Ovals (ellipses)
void MHIDLA::DrawOval(int /*x*/, int /*y*/, int /*width*/, int /*height*/)
{
    // Not implemented.  Not actually used in practice.
}

// Arcs and sectors
void MHIDLA::DrawArcSector(int /*x*/, int /*y*/, int /*width*/, int /*height*/,
                           int /*start*/, int /*arc*/, bool /*isSector*/)
{
    // Not implemented.  Not actually used in practice.
}

// Polygons.  This is used directly and also to draw other figures.
// The UK profile says that MHEG should not contain concave or
// self-crossing polygons but we can get the former at least as
// a result of rounding when drawing ellipses.
struct lineSeg { int m_yBottom, m_yTop, m_xBottom; float m_slope; };

void MHIDLA::DrawPoly(bool isFilled, const MHPointVec& xArray, const MHPointVec& yArray)
{
    int nPoints = xArray.size();
    if (nPoints < 2)
        return;

    if (isFilled)
    {
        QVector <lineSeg> lineArray(nPoints);
        int nLines = 0;
        // Initialise the line segment array.  Include all lines
        // apart from horizontal.  Close the polygon by starting
        // with the last point in the array.
        int lastX = xArray[nPoints-1]; // Last point
        int lastY = yArray[nPoints-1];
        int yMin = lastY;
        int yMax = lastY;
        for (int k = 0; k < nPoints; k++)
        {
            int thisX = xArray[k];
            int thisY = yArray[k];
            if (lastY != thisY)
            {
                if (lastY > thisY)
                {
                    lineArray[nLines].m_yBottom = thisY;
                    lineArray[nLines].m_yTop = lastY;
                    lineArray[nLines].m_xBottom = thisX;
                }
                else
                {
                    lineArray[nLines].m_yBottom = lastY;
                    lineArray[nLines].m_yTop = thisY;
                    lineArray[nLines].m_xBottom = lastX;
                }
                lineArray[nLines++].m_slope =
                    (float)(thisX-lastX) / (float)(thisY-lastY);
            }
            yMin = std::min(thisY, yMin);
            yMax = std::max(thisY, yMax);
            lastX = thisX;
            lastY = thisY;
        }

        // Find the intersections of each line in the line segment array
        // with the scan line.  Because UK MHEG says that figures should be
        // convex we only need to consider two intersections.
        QRgb fillColour = qRgba(m_fillColour.red(), m_fillColour.green(),
                                m_fillColour.blue(), m_fillColour.alpha());
        for (int y = yMin; y < yMax; y++)
        {
            int crossings = 0;
            int xMin = 0;
            int xMax = 0;
            for (int l = 0; l < nLines; l++)
            {
                if (y >= lineArray[l].m_yBottom && y < lineArray[l].m_yTop)
                {
                    int x = (int)round((float)(y - lineArray[l].m_yBottom) *
                        lineArray[l].m_slope) + lineArray[l].m_xBottom;
                    if (crossings == 0 || x < xMin)
                        xMin = x;
                    if (crossings == 0 || x > xMax)
                        xMax = x;
                    crossings++;
                }
            }
            if (crossings == 2)
            {
                for (int x = xMin; x <= xMax; x++)
                    m_image.setPixel(x, y, fillColour);
            }
        }

        // Draw the boundary
        int lastXpoint = xArray[nPoints-1]; // Last point
        int lastYpoint = yArray[nPoints-1];
        for (int i = 0; i < nPoints; i++)
        {
            DrawLine(xArray[i], yArray[i], lastXpoint, lastYpoint);
            lastXpoint = xArray[i];
            lastYpoint = yArray[i];
        }
    }
    else // PolyLine - draw lines between the points but don't close it.
    {
        for (int i = 1; i < nPoints; i++)
        {
            DrawLine(xArray[i], yArray[i], xArray[i-1], yArray[i-1]);
        }
    }
}

MHIBitmap::MHIBitmap(MHIContext *parent, bool tiled)
    : m_parent(parent), m_tiled(tiled),
      m_copyCtx(new MythAVCopy())
{
}

MHIBitmap::~MHIBitmap()
{
    delete m_copyCtx;
}

void MHIBitmap::Draw(int x, int y, QRect rect, bool tiled, bool bUnder)
{
    if (tiled)
    {
        if (m_image.width() == 0 || m_image.height() == 0)
            return;
        // Construct an image the size of the bounding box and tile the
        // bitmap over this.
        QImage tiledImage = QImage(rect.width(), rect.height(),
                                   QImage::Format_ARGB32);

        for (int i = 0; i < rect.width(); i++)
        {
            for (int j = 0; j < rect.height(); j++)
            {
                tiledImage.setPixel(i, j, m_image.pixel(i % m_image.width(), j % m_image.height()));
            }
        }
        m_parent->DrawImage(rect.x(), rect.y(), rect, tiledImage, true, bUnder);
    }
    else
    {
        // NB THe BBC expects bitmaps to be scaled, not clipped
        m_parent->DrawImage(x, y, rect, m_image, true, bUnder);
    }
}

// Create a bitmap from PNG.
void MHIBitmap::CreateFromPNG(const unsigned char *data, int length)
{
    m_image = QImage();

    if (!m_image.loadFromData(data, length, "PNG"))
    {
        m_image = QImage();
        return;
    }

    // Assume that if it has an alpha buffer then it's partly transparent.
    m_opaque = ! m_image.hasAlphaChannel();
}

// Create a bitmap from JPEG.
//virtual
void MHIBitmap::CreateFromJPEG(const unsigned char *data, int length)
{
    m_image = QImage();

    if (!m_image.loadFromData(data, length, "JPG"))
    {
        m_image = QImage();
        return;
    }

    // Assume that if it has an alpha buffer then it's partly transparent.
    m_opaque = ! m_image.hasAlphaChannel();
}

// Convert an MPEG I-frame into a bitmap.  This is used as the way of
// sending still pictures.  We convert the image to a QImage even
// though that actually means converting it from YUV and eventually
// converting it back again but we do this very infrequently so the
// cost is outweighed by the simplification.
void MHIBitmap::CreateFromMPEG(const unsigned char *data, int length)
{
    AVCodecContext *c = nullptr;
    MythAVFrame picture;
    AVPacket pkt;
    uint8_t *buff = nullptr;
    bool gotPicture = false;
    m_image = QImage();

    // Find the mpeg2 video decoder.
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
    if (!codec)
        return;
    if (!picture)
        return;

    // Automatically clean up memory allocation at function exit
    auto cleanup_fn = [&](MHIBitmap */*x*/) {
        pkt.data = buff;
        av_packet_unref(&pkt);
        avcodec_free_context(&c);
    };
    std::unique_ptr<MHIBitmap,decltype(cleanup_fn)> cleanup { this, cleanup_fn };

    c = avcodec_alloc_context3(nullptr);

    if (avcodec_open2(c, codec, nullptr) < 0)
        return;

    // Copy the data into AVPacket
    if (av_new_packet(&pkt, length) < 0)
        return;

    memcpy(pkt.data, data, length);
    buff = pkt.data;

    // Get a picture from the packet. Allow 9 loops for
    // packet to be decoded. It should take only 2-3 loops
    for (int limit=0; limit<10 && !gotPicture; limit++)
    {
        int len = avcodec_receive_frame(c, picture);
        if (len == 0)
            gotPicture = true;
        if (len == AVERROR(EAGAIN))
            len = 0;
        if (len == 0)
            len = avcodec_send_packet(c, &pkt);
        if (len == AVERROR(EAGAIN) || len == AVERROR_EOF)
            len = 0;
        if (len < 0) // Error
        {
            std::string error;
            LOG(VB_GENERAL, LOG_ERR,
                QString("[mhi] video decode error: %1 (%2)")
                .arg(av_make_error_stdstring(error, len))
                .arg(gotPicture));
            return;
        }

        pkt.data = nullptr;
        pkt.size = 0;
    }

    if (gotPicture)
    {
        int nContentWidth = c->width;
        int nContentHeight = c->height;
        m_image = QImage(nContentWidth, nContentHeight, QImage::Format_ARGB32);
        m_opaque = true; // MPEG images are always opaque.

        AVFrame retbuf;
        memset(&retbuf, 0, sizeof(AVFrame));

        int bufflen = nContentWidth * nContentHeight * 3;
        auto *outputbuf = (unsigned char*)av_malloc(bufflen);

        av_image_fill_arrays(retbuf.data, retbuf.linesize,
            outputbuf, AV_PIX_FMT_RGB24,
            nContentWidth, nContentHeight,IMAGE_ALIGN);

        AVFrame *tmp = picture;
        m_copyCtx->Copy(&retbuf, AV_PIX_FMT_RGB24, tmp, c->pix_fmt,
                     nContentWidth, nContentHeight);

        uint8_t * buf = outputbuf;

        // Copy the data a pixel at a time.
        // This should handle endianness correctly.
        for (int i = 0; i < nContentHeight; i++)
        {
            for (int j = 0; j < nContentWidth; j++)
            {
                int red = *buf++;
                int green = *buf++;
                int blue = *buf++;
                m_image.setPixel(j, i, qRgb(red, green, blue));
            }
        }
        av_freep(reinterpret_cast<void*>(&outputbuf));
    }
}

// Scale the bitmap.  Only used for image derived from MPEG I-frames.
void MHIBitmap::ScaleImage(int newWidth, int newHeight)
{
    if (m_image.isNull())
        return;

    if (newWidth == m_image.width() && newHeight == m_image.height())
        return;

    if (newWidth <= 0 || newHeight <= 0)
    { // This would be a bit silly but handle it anyway.
        m_image = QImage();
        return;
    }

    m_image = m_image.scaled(newWidth, newHeight,
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}
