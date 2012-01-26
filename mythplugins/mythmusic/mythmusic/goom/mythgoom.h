#ifndef MYTHGOOM 
#define MYTHGOOM

#include "../mainvisual.h"
#include "../config.h"

using namespace std;

class Goom : public VisualBase
{
public:
    Goom(void);
    virtual ~Goom();

    void resize(const QSize &size);
    bool process(VisualNode *node);
    bool draw(QPainter *p, const QColor &back);
    void handleKeyPress(const QString &action) {(void) action;}

private:
    QSize m_size;

    unsigned int *m_buffer;
    int m_scalew, m_scaleh;
};

#endif //MYTHGOOM
