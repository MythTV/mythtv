#ifndef VIDEOVISUAL_H
#define VIDEOVISUAL_H

#include <QRect>
#include <QList>
#include "stdint.h"

#include "mythverbose.h"
#include "visual.h"
#include "mythpainter.h"
#include "videovisualdefs.h"

#define DESC QString("Visualiser: ")

class MythRender;
class AudioPlayer;

class VisualNode
{
  public:
    VisualNode(short *l, short *r, unsigned long n, unsigned long o)
        : left(l), right(r), length(n), offset(o) { }

    ~VisualNode()
    {
        delete [] left;
        delete [] right;
    }

    short *left, *right;
    long length, offset;
};

class VideoVisual : public MythTV::Visual
{
  public:
    static bool CanVisualise(AudioPlayer *audio, MythRender *render);
    static VideoVisual* Create(AudioPlayer *audio, MythRender *render);

    VideoVisual(AudioPlayer *audio, MythRender *render);
   ~VideoVisual();

    virtual void Draw(const QRect &area, MythPainter *painter,
                      QPaintDevice* device) = 0;

    virtual void add(uchar *b, unsigned long b_len, unsigned long w, int c, int p);
    virtual void prepare();

  protected:
    VisualNode* GetNode(void);
    void DeleteNodes(void);
    int64_t SetLastUpdate(void);

    AudioPlayer       *m_audio;
    bool               m_disabled;
    QRect              m_area;
    MythRender        *m_render;
    QList<VisualNode*> m_nodes;
    QDateTime          m_lastUpdate;
};

#endif // VIDEOVISUAL_H
