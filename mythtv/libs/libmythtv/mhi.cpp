#include <unistd.h>

#include "mhi.h"
#include "osd.h"

static bool       ft_loaded = false;
static FT_Library ft_library;

#define FONT_WIDTHRES   48
#define FONT_HEIGHTRES  72
#define FONT_TO_USE "FreeSans.ttf"

/** \class MHIImageData
 *  \brief Data for items in the interactive television display stack.
 */
class MHIImageData
{
  public:
    QImage m_image;
    int    m_x;
    int    m_y;
};

MHIContext::MHIContext(InteractiveTV *parent)
    : m_parent(parent),     m_dsmcc(NULL),
      m_engine(NULL),       m_stop(false),
      m_stopped(false),     m_updated(false),
      m_displayWidth(StdDisplayWidth), m_displayHeight(StdDisplayHeight),
      m_face_loaded(false), m_currentChannel(-1),
      m_isLive(false),      m_audioTag(-1),
      m_videoTag(-1),       m_tuningTo(-1)
{
    m_display.setAutoDelete(true);
    m_dsmccQueue.setAutoDelete(true);

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
            m_face_loaded = true;
    }
}

// Load the font.  Copied, generally, from OSD::LoadFont.
bool MHIContext::LoadFont(QString name)
{
    QString fullname = MythContext::GetConfDir() + "/" + name;
    FT_Error error = FT_New_Face(ft_library, fullname.ascii(), 0, &m_face);
    if (!error)
        return true;

    fullname = gContext->GetShareDir() + name;
    error = FT_New_Face(ft_library, fullname.ascii(), 0, &m_face);
    if (!error)
        return true;

    fullname = gContext->GetShareDir() + "themes/" + name;
    error = FT_New_Face(ft_library, fullname.ascii(), 0, &m_face);
    if (!error)
        return true;

    fullname = name;
    error = FT_New_Face(ft_library, fullname.ascii(), 0, &m_face);
    if (!error)
        return true;
   
    VERBOSE(VB_IMPORTANT, QString("Unable to find font: %1").arg(name));
    return false;
}

MHIContext::~MHIContext()
{
    StopEngine();
    delete(m_engine);
    delete(m_dsmcc);
    if (m_face_loaded) FT_Done_Face(m_face);
}

// Ask the engine to stop and block until it has.
void MHIContext::StopEngine()
{
    if (m_engine)
    {
        while (!m_stopped)
        {
            m_stop = true;
            m_engine_wait.wakeAll();
            usleep(1000);
        }
    }
}


// Start or restart the MHEG engine.
void MHIContext::Restart(QString chanid, bool isLive)
{
    bool ok = false;
    m_currentChannel = chanid.toInt(&ok, 10);
    if (!ok)
        m_currentChannel = -1;

    if (m_currentChannel == m_tuningTo && m_currentChannel != -1)
    {
        // We have tuned to the channel in order to find the streams.
        // Leave the MHEG engine running but restart the DSMCC carousel.
        // This is a bit of a mess but it's the only way to be able to
        // select streams from a different channel.
        if (!m_dsmcc)
            m_dsmcc = new Dsmcc();
        {
            QMutexLocker locker(&m_dsmccLock);
            m_dsmcc->Reset();
            m_dsmccQueue.clear();
        }
    }
    else
    {
        StopEngine();

        if (!m_dsmcc)
            m_dsmcc = new Dsmcc();

        {
            QMutexLocker locker(&m_dsmccLock);
            m_dsmcc->Reset();
            m_dsmccQueue.clear();
        }

        {
            QMutexLocker locker(&m_keyLock);
            m_keyQueue.clear();
        }

        if (!m_engine)
            m_engine = MHCreateEngine(this);

        m_engine->SetBooting();
        m_display.clear();
        m_updated = true;
        m_stop = false;
        m_isLive = isLive;
        m_stopped = pthread_create(&m_engineThread, NULL,
                                   StartMHEGEngine, this) != 0;
        m_audioTag = -1;
        m_videoTag = -1;
        m_tuningTo = -1;
    }
}

// Thread function to run the MHEG engine.
void *MHIContext::StartMHEGEngine(void *param)
{
    //VERBOSE(VB_GENERAL, "Starting MHEG Engine");
    MHIContext *context = (MHIContext*) param;
    context->RunMHEGEngine();
    context->m_stopped = true;
    return NULL;
}

void MHIContext::RunMHEGEngine(void)
{
    while (!m_stop)
    {
        int toWait;
        // Dequeue and process any key presses.
        int key = 0;
        do
        {
            ProcessDSMCCQueue();
            {
                QMutexLocker locker(&m_keyLock);
                if (m_keyQueue.empty())
                    key = 0;
                else
                {
                    key = m_keyQueue.last();
                    m_keyQueue.pop_back();
                }
            }

            if (key != 0)
                m_engine->GenerateUserAction(key);

            // Run the engine and find out how long to pause.
            toWait = m_engine->RunAll();
        } while (key != 0);

        if (toWait > 1000 || toWait == 0)
            toWait = 1000;

        m_engine_wait.wait(toWait);
    }
}

// Dequeue and process any DSMCC packets.
void MHIContext::ProcessDSMCCQueue(void)
{
    DSMCCPacket *packet = NULL;
    do
    {
        {
            QMutexLocker locker(&m_dsmccLock);
            packet = m_dsmccQueue.dequeue();
        }

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
    unsigned char *dataCopy =
        (unsigned char*) malloc(length * sizeof(unsigned char));

    if (dataCopy == NULL)
        return;

    memcpy(dataCopy, data, length*sizeof(unsigned char));
    QMutexLocker locker(&m_dsmccLock);
    m_dsmccQueue.enqueue(new DSMCCPacket(dataCopy,     length,
                                         componentTag, carouselId,
                                         dataBroadcastId));
    m_engine_wait.wakeAll();
}

// Called by the engine to check for the presence of an object in the carousel.
bool MHIContext::CheckCarouselObject(QString objectPath)
{
    QStringList path = QStringList::split(QChar('/'), objectPath);
    QByteArray result; // Unused
    int res = m_dsmcc->GetDSMCCObject(path, result);
    return res == 0; // It's available now.
}

// Called by the engine to request data from the carousel.
bool MHIContext::GetCarouselData(QString objectPath, QByteArray &result)
{
    // Get the path components.  The string will normally begin with "//"
    // since this is an absolute path but that will be removed by split.
    QStringList path = QStringList::split(QChar('/'), objectPath);
    // Since the DSMCC carousel and the MHEG engine are currently on the
    // same thread this is safe.  Otherwise we need to make a deep copy of
    // the result.
    while (!m_stop)
    {
        int res = m_dsmcc->GetDSMCCObject(path, result);
        if (res == 0)
            return true; // Found it
        else if (res < 0)
            return false; // Not there.
        // Otherwise we block.
        // Process DSMCC packets then block for a second or until we receive
        // some more packets.  We should eventually find out if this item is
        // present.
        ProcessDSMCCQueue();
        m_engine_wait.wait(1000);
    }
    return false; // Stop has been set.  Say the object isn't present.
}

// Called from tv_play when a key is pressed.
// If it is one in the current profile we queue it for the engine 
// and return true otherwise we return false.
bool MHIContext::OfferKey(QString key)
{
    int action = 0;
    QMutexLocker locker(&m_keyLock);

    if (key == "UP")
    {
        if (m_keyProfile == 4 || m_keyProfile == 5)
            action = 1;
    }
    else if (key == "DOWN")
    {
        if (m_keyProfile == 4 || m_keyProfile == 5)
            action = 2;
    }
    else if (key == "LEFT")
    {
        if (m_keyProfile == 4 || m_keyProfile == 5)
            action = 3;
    }
    else if (key == "RIGHT")
    {
        if (m_keyProfile == 4 || m_keyProfile == 5)
            action = 4;
    }
    else if (key == "0" || key == "1" || key == "2" ||
             key == "3" || key == "4" || key == "5" ||
             key == "6" || key == "7" || key == "8" ||
             key == "9")
    {
        if (m_keyProfile == 4)
            action = key.toInt() + 5;
    }
    else if (key == "SELECT")
    {
        if (m_keyProfile == 4 || m_keyProfile == 5)
            action = 15;
    }
    // The EXIT key is supposed to be passed to the MHEG application
    // in all circumstances but we need it for 
    else if (key == "TEXTEXIT")
        action = 16;
    else if (key == "MENURED")
        action = 100;
    else if (key == "MENUGREEN")
        action = 101;
    else if (key == "MENUYELLOW")
        action = 102;
    else if (key == "MENUBLUE")
        action = 103;
    else if (key == "TOGGLECC")
        action = 104;

    if (action != 0)
    {
        m_keyQueue.push_front(action);
        VERBOSE(VB_IMPORTANT, "Adding MHEG key "<<key<<":"<<action
                <<":"<<m_keyQueue.size());
        m_engine_wait.wakeAll();
        return true;
    }

    return false;
}

void MHIContext::Reinit(const QRect &display)
{
    m_displayWidth = display.width();
    m_displayHeight = display.height();
}

void MHIContext::SetInputRegister(int num)
{
    QMutexLocker locker(&m_keyLock);
    m_keyQueue.clear();
    m_keyProfile = num;
}


// Called by the video player to redraw the image.
void MHIContext::UpdateOSD(OSDSet *osdSet)
{
    QMutexLocker locker(&m_display_lock);
    m_updated = false;
    osdSet->Clear();
    // Copy all the display items into the display.
    for (MHIImageData *data = m_display.first(); data;
         data = m_display.next())
    {
        OSDTypeImage* image = new OSDTypeImage();
        image->SetPosition(QPoint(data->m_x, data->m_y), 1.0, 1.0);
        image->LoadFromQImage(data->m_image);
        osdSet->AddType(image);
    }
}

void MHIContext::GetInitialStreams(int &audioTag, int &videoTag)
{
    audioTag = m_audioTag;
    videoTag = m_videoTag;
}


// An area of the screen/image needs to be redrawn.
// Called from the MHEG engine.
// We always redraw the whole scene. 
void MHIContext::RequireRedraw(const QRegion &)
{
    m_display_lock.lock();
    m_display.clear();
    m_display_lock.unlock();
    // Always redraw the whole screen
    m_engine->DrawDisplay(QRegion(0, 0, StdDisplayWidth, StdDisplayHeight));
    m_updated = true;
}

void MHIContext::AddToDisplay(const QImage &image, int x, int y)
{
    MHIImageData *data = new MHIImageData;
    data->m_image = image;
    data->m_x = x;
    data->m_y = y;
    QMutexLocker locker(&m_display_lock);
    m_display.append(data);
}

// In MHEG the video is just another item in the display stack 
// but when we create the OSD we overlay everything over the video.
// We need to cut out anything belowthe video on the display stack
// to leave the video area clear.
// The videoRect gives the size and position to which the video must be scaled.
// The displayRect gives the rectangle reserved for the video.
// e.g. part of the video may be clipped within the displayRect.
void MHIContext::DrawVideo(const QRect &videoRect, const QRect &dispRect)
{
    (void) videoRect;

    QMutexLocker locker(&m_display_lock);
    QRect displayRect(dispRect.x() * m_displayWidth/StdDisplayWidth,
                      dispRect.y() * m_displayHeight/StdDisplayHeight,
                      dispRect.width() * m_displayWidth/StdDisplayWidth,
                      dispRect.height() * m_displayHeight/StdDisplayHeight);

    for (uint i = 0; i < m_display.count(); i++)
    {
        MHIImageData *data = m_display.at(i);
        QRect imageRect(data->m_x, data->m_y,
                        data->m_image.width(), data->m_image.height());
        if (displayRect.intersects(imageRect))
        {
            // Replace this item with a set of cut-outs.
            (void)m_display.take(i--);
            QMemArray<QRect> rects = (QRegion(imageRect)
                                      - QRegion(displayRect)).rects();
            for (uint j = 0; j < rects.size(); j++)
            {
                QRect &rect = rects[j];
                QImage image =
                    data->m_image.copy(rect.x()-data->m_x, rect.y()-data->m_y,
                                       rect.width(), rect.height());
                MHIImageData *newData = new MHIImageData;
                newData->m_image = image;
                newData->m_x = rect.x();
                newData->m_y = rect.y();
                m_display.insert(++i, newData);
            }
            delete(data);
        }
    }
}


// Tuning.  Get the index corresponding to a given channel.
// The format of the service is dvb://netID.[transPortID].serviceID
// where the IDs are in hex.
// or rec://svc/lcn/N where N is the "logical channel number"
// i.e. the Freeview channel.
// Returns -1 if it cannot find it.
int MHIContext::GetChannelIndex(const QString &str)
{
    if (str.startsWith("dvb://"))
    {
        QStringList list = QStringList::split('.', str.mid(6), true);
        MSqlQuery query(MSqlQuery::InitCon());
        if (list.size() != 3) return -1; // Malformed.
        // The various fields are expressed in hexadecimal.
        // Convert them to decimal for the DB.
        bool ok;
        int netID = list[0].toInt(&ok, 16);
        if (!ok)
            return -1;
        int serviceID = list[2].toInt(&ok, 16);
        if (!ok)
            return -1;
        if (list[1].isEmpty()) // TransportID is not specified
        {
            query.prepare(
                "SELECT chanid "
                "FROM channel, dtv_multiplex "
                "WHERE networkid       = :NETID AND"
                "      channel.mplexid = dtv_multiplex.mplexid AND "
                "      serviceid       = :SERVICEID");
        }
        else
        {
            int transportID = list[1].toInt(&ok, 16);
            if (!ok)
                return -1;
            query.prepare(
                "SELECT chanid "
                "FROM channel, dtv_multiplex "
                "WHERE networkid       = :NETID AND"
                "      channel.mplexid = dtv_multiplex.mplexid AND "
                "      serviceid       = :SERVICEID AND "
                "      transportid     = :TRANSID");
            query.bindValue(":TRANSID", transportID);
        }
        query.bindValue(":NETID", netID);
        query.bindValue(":SERVICEID", serviceID);
        if (query.exec() && query.isActive() && query.next())
        {
            int nResult = query.value(0).toInt();
            return nResult;
        }
    }
    else if (str.startsWith("rec://svc/lcn/"))
    {
        // I haven't seen this yet so this is untested.
        bool ok;
        int channelNo = str.mid(14).toInt(&ok); // Decimal integer
        if (!ok) return -1;
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare("SELECT chanid "
                      "FROM channel "
                      "WHERE channum = :CHAN");
        query.bindValue(":CHAN", channelNo);
        if (query.exec() && query.isActive() && query.next())
            return query.value(0).toInt();
    }
    else if (str == "rec://svc/cur")
        return m_currentChannel;
    else if (str.startsWith("rec://"))
    {
    }
    return -1;
}

bool MHIContext::TuneTo(int channel)
{
    if (!m_isLive)
        return false; // Can't tune if this is a recording.

    // Post an event requesting a channel change.
    MythEvent me(QString("NETWORK_CONTROL CHANID %1").arg(channel));
    gContext->dispatch(me);

    return true;
}

// Begin playing audio from the specified stream
bool MHIContext::BeginAudio(const QString &stream, int tag)
{
    int chan = GetChannelIndex(stream);

    if (chan != m_currentChannel)
    {
        // We have to tune to the channel where the audio is to be found.
        // Because the audio and video are both components of an MHEG stream
        // they will both be on the same channel.
        m_tuningTo = chan;
        m_audioTag = tag;
        return TuneTo(chan);
    }

    if (tag < 0)
        return true; // Leave it at the default.
    else if (m_parent->GetNVP())
        return m_parent->GetNVP()->SetAudioByComponentTag(tag);
    else
        return false;
}

// Stop playing audio
void MHIContext::StopAudio(void)
{
    // Do nothing at the moment.
}

// Begin displaying video from the specified stream
bool MHIContext::BeginVideo(const QString &stream, int tag)
{
    int chan = GetChannelIndex(stream);
    if (chan != m_currentChannel)
    {
        // We have to tune to the channel where the video is to be found.
        m_tuningTo = chan;
        m_videoTag = tag;
        return TuneTo(chan);
    }
    if (tag < 0)
        return true; // Leave it at the default.
    else if (m_parent->GetNVP())
        return m_parent->GetNVP()->SetVideoByComponentTag(tag);

    return false;
}

// Stop displaying video
void MHIContext::StopVideo(void)
{
    // Do nothing at the moment.
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

    QRgb qColour = qRgba(colour.red(), colour.green(),
                         colour.blue(), colour.alpha());

    // This is a bit of a mess: we should be able to create a rectangle object.
    // Scale the image to the current display size
    int scaledWidth = width * GetWidth() / MHIContext::StdDisplayWidth;
    int scaledHeight = height * GetHeight() / MHIContext::StdDisplayHeight;
    QImage qImage(scaledWidth, scaledHeight, 32);
    qImage.setAlphaBuffer(true);

    // As far as I can tell this is the only way to draw with an
    // intermediate transparency.
    for (int i = 0; i < scaledHeight; i++)
    {
        for (int j = 0; j < scaledWidth; j++)
        {
            qImage.setPixel(j, i, qColour);
        }
    }

    AddToDisplay(qImage,
        xPos * GetWidth() / MHIContext::StdDisplayWidth,
        yPos * GetHeight() / MHIContext::StdDisplayHeight);
}

// Draw an image at the specified position.
// Generally the whole of the image is drawn but sometimes the
// image may be clipped. x and y define the origin of the bitmap
// and usually that will be the same as the origin of the bounding
// box (clipRect).
void MHIContext::DrawImage(int x, int y, const QRect &clipRect,
                           const QImage &qImage)
{
    if (qImage.isNull())
        return;

    QRect imageRect(x, y, qImage.width(), qImage.height());
    QRect displayRect = QRect(clipRect.x(), clipRect.y(),
                              clipRect.width(), clipRect.height()) & imageRect;

    if (displayRect == imageRect) // No clipping required
    {
        // LoadFromQImage seems to have a problem with non-32 bit images.
        // We need to work around that and force 32 bits.
        QImage scaled =
            qImage.smoothScale(
                displayRect.width() * GetWidth() / MHIContext::StdDisplayWidth,
                displayRect.height() *
                GetHeight() / MHIContext::StdDisplayHeight);
        AddToDisplay(scaled.convertDepth(32),
                     x * GetWidth() / MHIContext::StdDisplayWidth,
                     y * GetHeight() / MHIContext::StdDisplayHeight);
    }
    else if (!displayRect.isEmpty())
    { // We must clip the image.
        QImage clipped = qImage.convertDepth(32)
            .copy(displayRect.x() - x, displayRect.y() - y,
                  displayRect.width(), displayRect.height());
        QImage scaled =
            clipped.smoothScale(
                displayRect.width() * GetWidth() / MHIContext::StdDisplayWidth,
                displayRect.height() *
                GetHeight() / MHIContext::StdDisplayHeight);
        AddToDisplay(scaled,
                     displayRect.x() *
                     GetWidth() / MHIContext::StdDisplayWidth,
                     displayRect.y() *
                     GetHeight() / MHIContext::StdDisplayHeight);
    }
    // Otherwise draw nothing.
}

// Fill in the background.  This is only called if there is some area of
// the screen that is not covered with other visibles.
void MHIContext::DrawBackground(const QRegion &reg)
{
    if (reg.isNull() || reg.isEmpty())
        return;

    QRect bounds = reg.boundingRect();
    DrawRect(bounds.x(), bounds.y(), bounds.width(), bounds.height(), 
             MHRgba(0, 0, 0, 255)/* black. */);
}

MHIText::MHIText(MHIContext *parent): m_parent(parent)
{
    m_fontsize = 12;
    m_fontItalic = false;
    m_fontBold = false;
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
    m_fontsize = size;
    m_fontItalic = isItalic;
    m_fontBold = isBold;
    // TODO: Only the size is currently used.
    // Bold and Italic are currently ignored.
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
        return QRect(0,0,0,0);

    FT_Face face = m_parent->GetFontFace();
    FT_Error error = FT_Set_Char_Size(face, 0, m_fontsize*64,
                                      FONT_WIDTHRES, FONT_HEIGHTRES);
    if (error)
        return QRect(0,0,0,0);

    FT_GlyphSlot slot = face->glyph; /* a small shortcut */

    int maxAscent = 0, maxDescent = 0, width = 0;
    FT_Bool useKerning = FT_HAS_KERNING(face);
    FT_UInt previous = 0;

    for (int n = 0; n < strLen; n++)
    {
        QChar ch = str[n];
        FT_UInt glyphIndex = FT_Get_Char_Index(face, ch.unicode());
        int kerning = 0;

        if (useKerning && previous != 0 && glyphIndex != 0)
        {
            FT_Vector delta;
            FT_Get_Kerning(face, previous, glyphIndex,
                           FT_KERNING_DEFAULT, &delta);
            kerning = delta.x;
        } 

        error = FT_Load_Glyph(face, glyphIndex, 0); // Don't need to render.

        if (error)
            continue; // ignore errors.

        if (maxSize >= 0)
        {
            if ((width + slot->advance.x + kerning + (1<<6)-1) >> 6 > maxSize)
            {
                // There isn't enough space for this character.
                strLen = n;
                break;
            }
        }
        // Calculate the ascent and descent of this glyph.
        int descent = slot->metrics.height - slot->metrics.horiBearingY;

        if (slot->metrics.horiBearingY > maxAscent)
            maxAscent = slot->metrics.horiBearingY;

        if (descent > maxDescent)
            maxDescent = descent;

        width += slot->advance.x + kerning;
        previous = glyphIndex;
    }

    maxAscent = (maxAscent + (1<<6)-1) >> 6;
    maxDescent = (maxDescent + (1<<6)-1) >> 6;

    return QRect(0, -maxAscent,
                 (width+(1<<6)-1) >> 6, maxAscent + maxDescent);
}

// Reset the image and fill it with transparent ink.
// The UK MHEG profile says that we should consider the background
// as paper and the text as ink.  We have to consider these as two
// different layers.  The background is drawn separately as a rectangle.
void MHIText::Clear(void)
{
    m_image = QImage(m_width, m_height, 32);
    // 
    m_image.setAlphaBuffer(true);
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
    FT_GlyphSlot slot = face->glyph;
    FT_Error error = FT_Set_Char_Size(face, 0, m_fontsize*64,
                                      FONT_WIDTHRES, FONT_HEIGHTRES);

    // X positions are computed to 64ths and rounded.
    // Y positions are in pixels
    int posX = x << 6;
    // Add in the (negative) descender rounded up
    int pixelY = y + ((face->descender - ((1<<6) -1)) >> 6);
    FT_Bool useKerning = FT_HAS_KERNING(face);
    FT_UInt previous = 0;

    int len = str.length();
    for (int n = 0; n < len; n++)
    {
        // Load the glyph.
        QChar ch = str[n];
        FT_UInt glyphIndex = FT_Get_Char_Index(face, ch.unicode());
        if (useKerning && previous != 0 && glyphIndex != 0)
        {
            FT_Vector delta;
            FT_Get_Kerning(face, previous, glyphIndex,
                           FT_KERNING_DEFAULT, &delta);
            posX += delta.x;
        } 
        error = FT_Load_Glyph(face, glyphIndex, FT_LOAD_RENDER);

        if (error)
            continue; // ignore errors

        if (slot->format != FT_GLYPH_FORMAT_BITMAP)
            continue; // Problem

        if ((enum FT_Pixel_Mode_)slot->bitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
            continue;

        unsigned char *source = slot->bitmap.buffer;
        // Get the origin for the bitmap
        int baseX = ((posX + (1 << 5)) >> 6) + slot->bitmap_left;
        int baseY = pixelY - slot->bitmap_top;
        // Copy the bitmap into the image.
        for (int i = 0; i < slot->bitmap.rows; i++)
        {
            for (int j = 0; j < slot->bitmap.width; j++)
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

    int imageWidth = m_image.width(), imageHeight = m_image.height();
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
    m_image = QImage(m_width, m_height, 32);
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
                           m_lineWidth, m_height - m_lineWidth * 2,
                           m_boxLineColour);

        m_parent->DrawRect(x + m_width - m_lineWidth, y + m_lineWidth,
                           m_lineWidth, m_height - m_lineWidth * 2,
                           m_boxLineColour);

        // Deflate the box to within the border.
        bounds = QRect(bounds.x() + m_lineWidth,
                       bounds.y() + m_lineWidth,
                       bounds.width() - 2*m_lineWidth,
                       bounds.height() - 2*m_lineWidth);
    }

    // Draw the background.
    m_parent->DrawRect(x + m_lineWidth,
                       y + m_lineWidth,
                       m_width  - m_lineWidth * 2,
                       m_height - m_lineWidth * 2,
                       m_boxFillColour);

    // Now the drawing.
    m_parent->DrawImage(x, y, bounds, m_image);
}

// The UK MHEG profile defines exactly how transparency is supposed to work.
// The drawings are made using possibly transparent ink with any crossings
// just set to that ink and then the whole drawing is alpha-merged with the
// underlying graphics.  Since DynamicLineArt is hardly used in practice,
// the only example I've seen so far is in the YooGames' Tetris and
// that only uses opaque rectangles, there's no point in doing very much here.

// Lines
void MHIDLA::DrawLine(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/)
{
    VERBOSE(VB_IMPORTANT, "Dynamic Line Art: DrawLinel not implemented");
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
                 m_lineWidth, height - m_lineWidth * 2,
                 m_lineColour);

        DrawRect(x + width - m_lineWidth, y + m_lineWidth,
                 m_lineWidth, height - m_lineWidth * 2,
                 m_lineColour);

        // Fill the rectangle.
        DrawRect(x + m_lineWidth, y + m_lineWidth,
                 width - m_lineWidth * 2, height - m_lineWidth * 2,
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
    VERBOSE(VB_IMPORTANT, "Dynamic Line Art: DrawOval not implemented");
}

// Arcs and sectors
void MHIDLA::DrawArcSector(int /*x*/, int /*y*/,
                           int /*width*/, int /*height*/,
                           int /*start*/, int /*arc*/, bool /*isSector*/)
{
    VERBOSE(VB_IMPORTANT, "Dynamic Line Art: DrawArcSector not implemented");
}

// Polygons
void MHIDLA::DrawPoly(bool/* isFilled*/, const QPointArray &/*points*/)
{
    VERBOSE(VB_IMPORTANT, "Dynamic Line Art: DrawPoly not implemented");
}


void MHIBitmap::Draw(int x, int y, QRect rect, bool tiled)
{
    if (tiled)
    {
        if (m_image.width() == 0 || m_image.height() == 0)
            return;
        // Construct an image the size of the bounding box and tile the
        // bitmap over this.
        QImage tiledImage = QImage(rect.width(), rect.height(),
                                   m_image.depth());

        for (int i = 0; i < rect.width(); i += m_image.width())
        {
            for (int j = 0; j < rect.height(); j += m_image.height())
            {
                bitBlt(&tiledImage, i, j, &m_image, 0, 0, -1, -1, 0);
            }
        }
        m_parent->DrawImage(rect.x(), rect.y(), rect, tiledImage);
    }
    else
    {
        m_parent->DrawImage(x, y, rect, m_image);
    }
}

// Create a bitmap from PNG.
void MHIBitmap::CreateFromPNG(const unsigned char *data, int length)
{
    m_image.reset();

    if (!m_image.loadFromData(data, length, "PNG"))
    {
        m_image.reset();
        return;
    }

    // Assume that if it has an alpha buffer then it's partly transparent.
    m_opaque = ! m_image.hasAlphaBuffer();
}

// Convert an MPEG I-frame into a bitmap.  This is used as the way of
// sending still pictures.  We convert the image to a QImage even
// though that actually means converting it from YUV and eventually
// converting it back again but we do this very infrequently so the
// cost is outweighed by the simplification.
void MHIBitmap::CreateFromMPEG(const unsigned char *data, int length)
{
    AVCodecContext *c = NULL;
    AVFrame *picture = NULL;
    uint8_t *buff = NULL, *buffPtr;
    int gotPicture = 0, len;
    m_image.reset();

    // Find the mpeg2 video decoder.
    AVCodec *codec = avcodec_find_decoder(CODEC_ID_MPEG2VIDEO);
    if (!codec)
        return;

    c = avcodec_alloc_context();
    picture = avcodec_alloc_frame();

    if (avcodec_open(c, codec) < 0)
        goto Close;

    // Copy the data into a new buffer with sufficient padding.
    buff = (uint8_t *)malloc(length + FF_INPUT_BUFFER_PADDING_SIZE);
    if (!buff)
        goto Close;

    memcpy(buff, data, length);
    memset(buff + length, 0, FF_INPUT_BUFFER_PADDING_SIZE);
    buffPtr = buff;

    while (length > 0 && ! gotPicture)
    {
        len = avcodec_decode_video(c, picture, &gotPicture, buffPtr, length);
        if (len < 0) // Error
            goto Close;
        length -= len;
        buffPtr += len;
    }

    if (!gotPicture)
    {
        // Process any buffered data
        if (avcodec_decode_video(c, picture, &gotPicture, NULL, 0) < 0)
            goto Close;
    }

    if (gotPicture)
    {
        int nContentWidth = c->width;
        int nContentHeight = c->height;
        m_image = QImage(nContentWidth, nContentHeight, 32);
        m_opaque = true; // MPEG images are always opaque.

        AVPicture retbuf;
        bzero(&retbuf, sizeof(AVPicture));

        int bufflen = nContentWidth * nContentHeight * 3;
        unsigned char *outputbuf = new unsigned char[bufflen];

        avpicture_fill(&retbuf, outputbuf, PIX_FMT_RGB24,
                       nContentWidth, nContentHeight);

        img_convert(&retbuf, PIX_FMT_RGB24, (AVPicture*)picture, c->pix_fmt,
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
        delete outputbuf;
    }

Close:
    free(buff);
    avcodec_close(c);
    av_free(c);
    av_free(picture);
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
        m_image.reset();
        return;
    }

    m_image = m_image.smoothScale(newWidth, newHeight);
}
