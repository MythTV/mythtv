#ifndef MHI_H
#define MHI_H

// Misc header
#include <ft2build.h>
#include FT_FREETYPE_H

// STL headers
#include <list>
#include <vector>

// Qt headers
#include <QWaitCondition>
#include <QRunnable>
#include <QString>
#include <QMutex>
#include <QImage>
#include <QList>
#include <QRect>
#include <QPair>
#include <QMap>                         // for QMap
#include <QRegion>                      // for QRegion
#include <QSize>                        // for QSize

// MythTV headers
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdeque.h"
#include "libmythfreemheg/freemheg.h"

#include "mhegic.h"

class MythPainter;
class InteractiveScreen;
class DSMCCPacket;
class MHIImageData;
class MHIContext;
class MythAVCopy;
class Dsmcc;
class InteractiveTV;
class MHStream;
class MThread;
class QByteArray;

// Special value for the NetworkBootInfo version.  Real values are a byte.
static constexpr uint16_t NBI_VERSION_UNSET { 257 };

/** \class MHIContext
 *  \brief Contains various utility functions for interactive television.
 */
class MHIContext : public MHContext, public QRunnable
{
  public:
    explicit MHIContext(InteractiveTV *parent);
    ~MHIContext() override;

    void QueueDSMCCPacket(unsigned char *data, int length, int componentTag,
        unsigned carouselId, int dataBroadcastId);
    // A NetworkBootInfo sub-descriptor is present in the PMT.
    void SetNetBootInfo(const unsigned char *data, uint length);
    /// Restart the MHEG engine.
    void Restart(int chanid, int sourceid, bool isLive);
    // Offer a key press.  Returns true if it accepts it.
    // This will depend on the current profile.
    bool OfferKey(const QString& key);
    /// Update the display
    void UpdateOSD(InteractiveScreen *osdWindow, MythPainter *osdPainter);
    /// The display area has changed.
    void Reinit(QRect videoRect, QRect dispRect, float aspect);

    /// Stop the MHEG engine if it's running and waits until it has.
    void StopEngine(void);

    // Test for an object in the carousel.
    // Returns true if the object is present and
    // so a call to GetCarouselData will not block and will return the data.
    // Returns false if the object is not currently present because it has not
    // yet appeared and also if it is not present in the containing directory.
    bool CheckCarouselObject(const QString& objectPath) override; // MHContext

    // Get an object from the carousel.  Returns true and sets the data if
    // it was able to retrieve the named object.  Blocks if the object seems
    // to be present but has not yet appeared.  Returns false if the object
    // cannot be retrieved.
    bool GetCarouselData(const QString& objectPath, QByteArray &result) override; // MHContext

    // Set the input register.  This sets the keys that are to be handled
    // by MHEG.  Flushes the key queue.
    void SetInputRegister(int num) override; // MHContext

    /// An area of the screen/image needs to be redrawn.
    void RequireRedraw(const QRegion &region) override; // MHContext

    /// Check whether we have requested a stop.
    bool CheckStop(void) override // MHContext
        { return m_stop; }

    /// Get the initial component tags.
    void GetInitialStreams(int &audioTag, int &videoTag) const;

    /// Creation functions for various visibles.
    MHDLADisplay *CreateDynamicLineArt(
        bool isBoxed, MHRgba lineColour, MHRgba fillColour) override; // MHContext
    MHTextDisplay *CreateText(void) override; // MHContext
    MHBitmapDisplay *CreateBitmap(bool tiled) override; // MHContext
    /// Additional drawing functions.
    void DrawRect(int xPos, int yPos, int width, int height,
                  MHRgba colour) override; // MHContext
    void DrawBackground(const QRegion &reg) override; // MHContext
    void DrawVideo(const QRect &videoRect, const QRect &dispRect) override; // MHContext

    void DrawImage(int x, int y, QRect rect, const QImage &image,
        bool bScaled = false, bool bUnder = false);

    int GetChannelIndex(const QString &str) override; // MHContext
    /// Get netId etc from the channel index.
    bool GetServiceInfo(int channelId, int &netId, int &origNetId,
                        int &transportId, int &serviceId) override; // MHContext
    bool TuneTo(int channel, int tuneinfo) override; // MHContext

    /// Begin playing the specified stream
    bool BeginStream(const QString &str, MHStream* notify) override; // MHContext
    void EndStream() override; // MHContext
    // Called when the stream starts or stops
    bool StreamStarted(bool bStarted = true);
    /// Begin playing audio
    bool BeginAudio(int tag) override; // MHContext
    /// Stop playing audio
    void StopAudio() override; // MHContext
    /// Begin displaying video
    bool BeginVideo(int tag) override; // MHContext
    /// Stop displaying video
    void StopVideo() override; // MHContext
    // Get current stream position, -1 if unknown
    std::chrono::milliseconds GetStreamPos() override; // MHContext
    // Get current stream size, -1 if unknown
    std::chrono::milliseconds GetStreamMaxPos() override; // MHContext
    // Set current stream position
    std::chrono::milliseconds SetStreamPos(std::chrono::milliseconds pos) override; // MHContext
    // Play or pause a stream
    void StreamPlay(bool play) override; // MHContext

    // Get the context id strings.  The format of these strings is specified
    // by the UK MHEG profile.
    const char *GetReceiverId(void) override // MHContext
        { return "MYT001001"; } // Version number?
    const char *GetDSMCCId(void) override // MHContext
        { return "DSMMYT001"; } // DSMCC version.

    // InteractionChannel
    // 0= Active, 1= Inactive, 2= Disabled
    int GetICStatus() override; // MHContext

    // Operations used by the display classes
    // Add an item to the display vector
    void AddToDisplay(const QImage &image, QRect rect, bool bUnder = false);
    int ScaleX(int n, bool roundup = false) const;
    int ScaleY(int n, bool roundup = false) const;
    QRect Scale(QRect r) const;
    int ScaleVideoX(int n, bool roundup = false) const;
    int ScaleVideoY(int n, bool roundup = false) const;
    QRect ScaleVideo(QRect r) const;

    FT_Face GetFontFace(void) { return m_face; }
    bool IsFaceLoaded(void) const { return m_faceLoaded; }
    bool LoadFont(const QString& name);
    bool ImageUpdated(void) const { return m_updated; }

    static const int kStdDisplayWidth = 720;
    static const int kStdDisplayHeight = 576;

  protected:
    void run(void) override; // QRunnable
    void ProcessDSMCCQueue(void);
    void NetworkBootRequested(void);
    void ClearDisplay(void);
    void ClearQueue(void);
    bool LoadChannelCache();
    bool GetDSMCCObject(const QString &objectPath, QByteArray &result);
    bool CheckAccess(const QString &objectPath, QByteArray &cert);

    InteractiveTV   *m_parent         {nullptr};

    // Pointer to the DSMCC object carousel.
    Dsmcc           *m_dsmcc          {nullptr};
    QMutex           m_dsmccLock;
    MythDeque<DSMCCPacket*> m_dsmccQueue;

    MHInteractionChannel m_ic;  // Interaction channel
    MHStream        *m_notify         {nullptr};

    QMutex           m_keyLock;
    MythDeque<int>   m_keyQueue;
    int              m_keyProfile     {0};

    MHEG            *m_engine; // Pointer to the MHEG engine

    mutable QMutex   m_runLock;
    QWaitCondition   m_engineWait; // protected by m_runLock
    bool             m_stop           {false}; // protected by m_runLock
    QMutex           m_displayLock;
    bool             m_updated        {false};

    std::list<MHIImageData*> m_display; // List of items to display

    FT_Face          m_face           {nullptr};
    bool             m_faceLoaded     {false};

    MThread         *m_engineThread   {nullptr};

    int              m_currentChannel {-1};
    int              m_currentStream  {-1};
    bool             m_isLive         {false};
    int              m_currentSource  {-1};

    int              m_audioTag       {-1};
    int              m_videoTag       {-1};
    QList<int>       m_tuneInfo;

    uint             m_lastNbiVersion {NBI_VERSION_UNSET};
    std::vector<unsigned char> m_nbiData;

    QRect            m_videoRect, m_videoDisplayRect;
    QRect            m_displayRect;

    // Channel index database cache
    using Val_t = QPair< int, int >; // transportid, chanid
    using Key_t = QPair< int, int >; // networkid, serviceid
    using ChannelCache_t = QMultiMap< Key_t, Val_t >;
    ChannelCache_t  m_channelCache;
    QMutex          m_channelMutex;
    static int Tid(ChannelCache_t::const_iterator it) { return it->first; }
    static int Cid(ChannelCache_t::const_iterator it) { return it->second; }
    static int Nid(ChannelCache_t::const_iterator it) { return it.key().first; }
    static int Sid(ChannelCache_t::const_iterator it) { return it.key().second; }
};

// Object for drawing text.
class MHIText : public MHTextDisplay
{
  public:
    explicit MHIText(MHIContext *parent)
        : m_parent(parent) {}
    ~MHIText() override = default;

    void Draw(int x, int y) override; // MHTextDisplay
    void Clear(void) override; // MHTextDisplay
    void AddText(int x, int y, const QString &str, MHRgba colour) override; // MHTextDisplay

    void SetSize(int width, int height) override; // MHTextDisplay
    void SetFont(int size, bool isBold, bool isItalic) override; // MHTextDisplay

    QRect GetBounds(const QString &str, int &strLen, int maxSize = -1) override; // MHTextDisplay

  public:
    MHIContext *m_parent     {nullptr};
    QImage      m_image;
    int         m_fontSize   {12};
    bool        m_fontItalic {false};
    bool        m_fontBold   {false};
    int         m_width      {0};
    int         m_height     {0};
};

/** \class MHIBitmap
 *  \brief Object for drawing bitmaps.
 */
class MHIBitmap : public MHBitmapDisplay
{
  public:
    MHIBitmap(MHIContext *parent, bool tiled);
    ~MHIBitmap() override;

    // Deleted functions should be public.
    MHIBitmap(const MHIBitmap &) = delete;            // not copyable
    MHIBitmap &operator=(const MHIBitmap &) = delete; // not copyable

    /// Create bitmap from PNG
    void CreateFromPNG(const unsigned char *data, int length) override; // MHBitmapDisplay

    /// Create bitmap from single I frame MPEG
    void CreateFromMPEG(const unsigned char *data, int length) override; // MHBitmapDisplay

    /// Create bitmap from JPEG
    void CreateFromJPEG(const unsigned char *data, int length) override; // MHBitmapDisplay

    /**
     *  \brief Draw the completed drawing onto the display.
     *
     *  \param x      Horizontal position of the image relative to the screen.
     *  \param y      Vertical position of the image relative to the screen.
     *  \param rect   Bounding box for the image relative to the screen.
     *  \param tiled  Tile the drawing to fit the parent window.
     *  \param bUnder Put the drawing at the behind any other widgets.
     */
    void Draw(int x, int y, QRect rect, bool tiled, bool bUnder) override; // MHBitmapDisplay

    /// Scale the bitmap.  Only used for image derived from MPEG I-frames.
    void ScaleImage(int newWidth, int newHeight) override; // MHBitmapDisplay

    // Gets
    QSize GetSize(void) override { return m_image.size(); } // MHBitmapDisplay
    bool IsOpaque(void) override { return !m_image.isNull() && m_opaque; } // MHBitmapDisplay

  public:
    MHIContext *m_parent  {nullptr};
    bool        m_tiled;
    QImage      m_image;
    bool        m_opaque  {false};
    MythAVCopy *m_copyCtx {nullptr};
};

/** \class MHIDLA
 *  \brief Object for displaying Dynamic Line Art
 */
class MHIDLA : public MHDLADisplay
{
  public:
    MHIDLA(MHIContext *parent, bool isBoxed,
           MHRgba lineColour, MHRgba fillColour)
        : m_parent(parent),            m_boxed(isBoxed),
          m_boxLineColour(lineColour), m_boxFillColour(fillColour) {}
    /// Draw the completed drawing onto the display.
    void Draw(int x, int y) override; // MHDLADisplay
    /// Set the box size.  Also clears the drawing.
    void SetSize(int width, int height) override // MHDLADisplay
    {
        m_width  = width;
        m_height = height;
        Clear();
    }
    void SetLineSize(int width) override // MHDLADisplay
        { m_lineWidth = width;   }
    void SetLineColour(MHRgba colour) override // MHDLADisplay
        { m_lineColour = colour; }
    void SetFillColour(MHRgba colour) override // MHDLADisplay
        { m_fillColour = colour; }

    /// Clear the drawing
    void Clear(void) override; // MHDLADisplay

    // add items to the drawing.
    void DrawLine(int x1, int y1, int x2, int y2) override; // MHDLADisplay
    void DrawBorderedRectangle(int x, int y, int width, int height) override; // MHDLADisplay
    void DrawOval(int x, int y, int width, int height) override; // MHDLADisplay
    void DrawArcSector(int x, int y, int width, int height,
                       int start, int arc, bool isSector) override; // MHDLADisplay
    void DrawPoly(bool isFilled, const MHPointVec& xArray, const MHPointVec& yArray) override; // MHDLADisplay

  protected:
    void DrawRect(int x, int y, int width, int height, MHRgba colour);
    void DrawLineSub(int x1, int y1, int x2, int y2, bool swapped);

  protected:
    MHIContext *m_parent        {nullptr};
    QImage      m_image;
    int         m_width         {0}; ///< Width of the drawing
    int         m_height        {0}; ///< Height of the drawing
    bool        m_boxed;             ///< Does it have a border?
    MHRgba      m_boxLineColour;     ///< Line colour for the background
    MHRgba      m_boxFillColour;     ///< Fill colour for the background
    MHRgba      m_lineColour;        ///< Current line colour.
    MHRgba      m_fillColour;        ///< Current fill colour.
    int         m_lineWidth     {0}; ///< Current line width.
};

/** \class DSMCCPacket
 *  \brief Data for the queued DSMCC tables
 */
class DSMCCPacket
{
  public:
    DSMCCPacket(unsigned char *data, int length, int tag,
                unsigned car, int dbid)
        : m_data(data),           m_length(length),
          m_componentTag(tag),    m_carouselId(car),
          m_dataBroadcastId(dbid)
    {
    }

    ~DSMCCPacket()
    {
        free(m_data);
    }

  public:
    unsigned char *m_data            {nullptr};
    int            m_length;
    int            m_componentTag;
    unsigned       m_carouselId;
    int            m_dataBroadcastId;
};

#endif // MHI_H
