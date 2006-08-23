#ifndef _MHI_H_
#define _MHI_H_

// POSIX header
#include <pthread.h>

// Misc header
#include <ft2build.h>
#include FT_FREETYPE_H

// Qt headers
#include <qmutex.h>
#include <qcstring.h>
#include <qstring.h>
#include <qptrlist.h>
#include <qwaitcondition.h>
#include <qimage.h>
#include <qvaluelist.h>
#include <qptrqueue.h>

// MythTV headers
#include "../libmythfreemheg/freemheg.h"
#include "interactivetv.h"
#include "dsmcc.h"
#include "osdtypes.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "NuppelVideoPlayer.h"

#include "../libavcodec/avcodec.h" // to decode single MPEG I-frames

class OSDSet;
class DSMCCPacket;
class MHIImageData;

/** \class MHIContext
 *  \brief Contains various utility functions for interactive television.
 */
class MHIContext : public MHContext
{
  public:
    MHIContext(InteractiveTV *parent);
    virtual ~MHIContext();

    void QueueDSMCCPacket(unsigned char *data, int length, int componentTag,
        unsigned carouselId, int dataBroadcastId);
    /// Restart the MHEG engine.
    void Restart(uint chanid, uint cardid, bool isLive);
    // Offer a key press.  Returns true if it accepts it.
    // This will depend on the current profile.
    bool OfferKey(QString key);
    /// Update the display
    void UpdateOSD(OSDSet *osdSet);
    /// The display area has changed.
    void Reinit(const QRect &display);

    /// Stop the MHEG engine if it's running and waits until it has.
    void StopEngine(void);

    // Test for an object in the carousel.
    // Returns true if the object is present and
    // so a call to GetCarouselData will not block and will return the data.
    // Returns false if the object is not currently present because it has not
    // yet appeared and also if it is not present in the containing directory.
    virtual bool CheckCarouselObject(QString objectPath);

    // Get an object from the carousel.  Returns true and sets the data if
    // it was able to retrieve the named object.  Blocks if the object seems
    // to be present but has not yet appeared.  Returns false if the object
    // cannot be retrieved.
    virtual bool GetCarouselData(QString objectPath, QByteArray &result);

    // Set the input register.  This sets the keys that are to be handled
    // by MHEG.  Flushes the key queue.
    virtual void SetInputRegister(int nReg);

    /// An area of the screen/image needs to be redrawn.
    virtual void RequireRedraw(const QRegion &region);

    /// Check whether we have requested a stop.
    virtual bool CheckStop(void) { return m_stop; }

    /// Get the initial component tags.
    void GetInitialStreams(int &audioTag, int &videoTag);

    /// Creation functions for various visibles.
    virtual MHDLADisplay *CreateDynamicLineArt(
        bool isBoxed, MHRgba lineColour, MHRgba fillColour);
    virtual MHTextDisplay *CreateText(void);
    virtual MHBitmapDisplay *CreateBitmap(bool tiling);
    /// Additional drawing functions.
    virtual void DrawRect(int xPos, int yPos, int width, int height,
                          MHRgba colour);
    virtual void DrawBackground(const QRegion &reg);
    virtual void DrawVideo(const QRect &videoRect, const QRect &displayRect);

    void DrawImage(int x, int y, const QRect &rect, const QImage &image);

    virtual int GetChannelIndex(const QString &str);
    virtual bool TuneTo(int channel);

    /// Begin playing audio from the specified stream
    virtual bool BeginAudio(const QString &stream, int tag);
    /// Stop playing audio
    virtual void StopAudio(void);
    /// Begin displaying video from the specified stream
    virtual bool BeginVideo(const QString &stream, int tag);
    /// Stop displaying video
    virtual void StopVideo(void);

    // Get the context id strings.  The format of these strings is specified
    // by the UK MHEG profile.
    virtual const char *GetReceiverId(void)
        { return "MYT001001"; } // Version number?
    virtual const char *GetDSMCCId(void)
        { return "DSMMYT001"; } // DSMCC version.

    // Operations used by the display classes
    // Add an item to the display vector
    void AddToDisplay(const QImage &image, int x, int y);

    FT_Face GetFontFace(void) { return m_face; }
    bool IsFaceLoaded(void) { return m_face_loaded; }
    bool LoadFont(QString name);
    bool ImageUpdated(void) { return m_updated; }

    static const int StdDisplayWidth = 720;
    static const int StdDisplayHeight = 576;
    int GetWidth(void) { return m_displayWidth; }
    int GetHeight(void) { return m_displayHeight; }

  protected:
    static void *StartMHEGEngine(void *param);
    void RunMHEGEngine(void);
    void ProcessDSMCCQueue(void);

    InteractiveTV   *m_parent;

    Dsmcc           *m_dsmcc;  // Pointer to the DSMCC object carousel.
    QMutex           m_dsmccLock;
    QPtrQueue<DSMCCPacket> m_dsmccQueue;

    QMutex           m_keyLock;
    QValueList<int>  m_keyQueue;
    int              m_keyProfile;

    MHEG            *m_engine; // Pointer to the MHEG engine

    QWaitCondition   m_engine_wait;
    bool             m_stop;
    bool             m_stopped;
    QMutex           m_display_lock;
    bool             m_updated;
    int              m_displayWidth;
    int              m_displayHeight;


    QPtrList<MHIImageData> m_display; // List of items to display

    FT_Face          m_face;
    bool             m_face_loaded;

    pthread_t        m_engineThread;

    int              m_currentChannel;
    bool             m_isLive;
    int              m_currentCard;

    int              m_audioTag;
    int              m_videoTag;
    int              m_tuningTo;
};

// Object for drawing text.
class MHIText : public MHTextDisplay
{
  public:
    MHIText(MHIContext *parent);
    virtual ~MHIText() {}

    virtual void Draw(int x, int y);
    virtual void Clear(void);
    virtual void AddText(int x, int y, const QString &, MHRgba colour);

    virtual void SetSize(int width, int height);
    virtual void SetFont(int size, bool isBold, bool isItalic);

    virtual QRect GetBounds(const QString &str, int &strLen, int maxSize = -1);

  public:
    MHIContext *m_parent;
    QImage      m_image;
    int         m_fontsize;
    bool        m_fontItalic;
    bool        m_fontBold;
    int         m_width;
    int         m_height;
};

/** \class MHIBitmap
 *  \brief Object for drawing bitmaps.
 */
class MHIBitmap : public MHBitmapDisplay
{
  public:
    MHIBitmap(MHIContext *parent, bool tiled)
        : m_parent(parent), m_tiled(tiled), m_opaque(false) {}
    virtual ~MHIBitmap() {}

    /// Create bitmap from PNG
    virtual void CreateFromPNG(const unsigned char *data, int length);

    /// Create bitmap from single I frame MPEG
    virtual void CreateFromMPEG(const unsigned char *data, int length);

    /** \fn MHIBitmap::Draw(int,int,QRect,bool)
     *  \brief Draw the completed drawing onto the display.
     *
     *  \param x     Horizontal position of the image relative to the screen.
     *  \param y     Vertical position of the image relative to the screen.
     *  \param rect  Bounding box for the image relative to the screen.
     */
    virtual void Draw(int x, int y, QRect rect, bool tiled);

    /// Scale the bitmap.  Only used for image derived from MPEG I-frames.
    virtual void ScaleImage(int newWidth, int newHeight);

    // Gets
    virtual QSize GetSize(void) { return m_image.size(); }
    virtual bool IsOpaque(void) { return !m_image.isNull() && m_opaque; }

  public:
    MHIContext *m_parent;
    bool        m_tiled;
    QImage      m_image;
    bool        m_opaque;
};

/** \class MHIDLA
 *  \brief Object for displaying Dynamic Line Art
 */
class MHIDLA : public MHDLADisplay
{
  public:
    MHIDLA(MHIContext *parent, bool isBoxed,
           MHRgba lineColour, MHRgba fillColour)
        : m_parent(parent), m_boxed(isBoxed),
          m_boxLineColour(lineColour), m_boxFillColour(fillColour) {}
    /// Draw the completed drawing onto the display.
    virtual void Draw(int x, int y);
    /// Set the box size.  Also clears the drawing.
    virtual void SetSize(int width, int height)
    {
        m_width  = width;
        m_height = height;
        Clear();
    }
    virtual void SetLineSize(int width)       { m_lineWidth = width;   }
    virtual void SetLineColour(MHRgba colour) { m_lineColour = colour; }
    virtual void SetFillColour(MHRgba colour) { m_fillColour = colour; }

    /// Clear the drawing
    virtual void Clear(void);

    // add items to the drawing.
    virtual void DrawLine(int x1, int y1, int x2, int y2);
    virtual void DrawBorderedRectangle(int x, int y, int width, int height);
    virtual void DrawOval(int x, int y, int width, int height);
    virtual void DrawArcSector(int x, int y, int width, int height,
                               int start, int arc, bool isSector);
    virtual void DrawPoly(bool isFilled, const QPointArray &points);

  protected:
    void DrawRect(int x, int y, int width, int height, MHRgba colour);

  protected:
    MHIContext *m_parent;
    QImage      m_image;
    int         m_width;         ///< Width of the drawing
    int         m_height;        ///< Height of the drawing
    bool        m_boxed;         ///< Does it have a border?
    MHRgba      m_boxLineColour; ///< Line colour for the background
    MHRgba      m_boxFillColour; ///< Fill colour for the background
    MHRgba      m_lineColour;    ///< Current line colour.
    MHRgba      m_fillColour;    ///< Current fill colour.
    int         m_lineWidth;     ///< Current line width.
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
    unsigned char *m_data;
    int            m_length;
    int            m_componentTag;
    unsigned       m_carouselId;
    int            m_dataBroadcastId;
};

#endif // _MHI_H_
