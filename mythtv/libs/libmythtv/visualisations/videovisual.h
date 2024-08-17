#ifndef VIDEOVISUAL_H
#define VIDEOVISUAL_H

#include <cstdint>

#include <QRect>
#include <QList>
#include <QDateTime>

#include "libmyth/visual.h"
#include "libmythbase/mythlogging.h"
#include "libmythtv/mythtvexp.h"
#include "libmythui/mythpainter.h"
#include "libmythui/mythrender_base.h"

#include "videovisualdefs.h"

using namespace std::chrono_literals;

#define DESC QString("Visualiser: ")

class MythRender;
class AudioPlayer;

class VisualNode
{
  public:
    VisualNode(short *l, short *r, unsigned long n, std::chrono::milliseconds timecode)
        : m_left(l), m_right(r), m_length(n), m_offset(timecode) { }

    ~VisualNode()
    {
        delete [] m_left;
        delete [] m_right;
    }

    short *m_left   {nullptr};
    short *m_right  {nullptr};
    long   m_length;
    std::chrono::milliseconds m_offset;
};

class MTV_PUBLIC VideoVisual : public MythTV::Visual
{
  public:
    static VideoVisual* Create(const QString &name,
                               AudioPlayer *audio, MythRender *render);
    static QStringList GetVisualiserList(RenderType type);

    VideoVisual(AudioPlayer *audio, MythRender *render);
   ~VideoVisual() override;

    bool NeedsPrepare() const { return m_needsPrepare; }
    virtual void Prepare(const QRect /*Area*/) { }
    virtual void Draw(QRect area, MythPainter *painter,
                      QPaintDevice* device) = 0;
    virtual QString Name(void) = 0;

    void add(const void *b, unsigned long b_len,
             std::chrono::milliseconds timecode,
             int c, int p) override; // Visual
    void prepare() override; // Visual

  protected:
    VisualNode* GetNode(void);
    void DeleteNodes(void);
    std::chrono::milliseconds SetLastUpdate(void);

    AudioPlayer       *m_audio        { nullptr };
    bool               m_needsPrepare { false   };
    bool               m_disabled     { false   };
    QRect              m_area;
    MythRender        *m_render       { nullptr };
    QList<VisualNode*> m_nodes;
    QDateTime          m_lastUpdate;
};

class VideoVisualFactory
{
  public:
    VideoVisualFactory()
      : m_nextVideoVisualFactory(g_videoVisualFactory)
    {
        g_videoVisualFactory = this;
    }
    virtual ~VideoVisualFactory() = default;
    virtual const QString &name(void) const = 0;
    virtual VideoVisual* Create(AudioPlayer *audio,
                                MythRender *render) const = 0;
    static VideoVisualFactory* VideoVisualFactories()
    {
        return g_videoVisualFactory;
    }
    VideoVisualFactory* next() const
    {
        return m_nextVideoVisualFactory;
    }
    virtual bool SupportedRenderer(RenderType type) = 0;

  protected:
    static VideoVisualFactory* g_videoVisualFactory;
    VideoVisualFactory*        m_nextVideoVisualFactory {nullptr};
};
#endif // VIDEOVISUAL_H
