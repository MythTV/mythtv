#ifndef MYTHGOOM 
#define MYTHGOOM

#include "../mainvisual.h"
#include "../config.h"

using namespace std;

#ifdef SDL_SUPPORT
#include <SDL.h>

class Goom : public VisualBase
{
public:
    Goom(long int winid);
    virtual ~Goom();

    void resize(const QSize &size);
    bool process(VisualNode *node);
    bool draw(QPainter *p, const QColor &back);

private:
    QSize size;

    SDL_Surface *surface;

    unsigned int *buffer;
    int scalew, scaleh;
};

class GoomFactory : public VisFactory
{
  public:
    const QString &name(void) const;
    const QString &description(void) const;
    VisualBase *create(MainVisual *parent, long int winid);
};

#endif

#endif // __mainvisual_h
