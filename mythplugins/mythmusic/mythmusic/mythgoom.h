#ifndef MYTHGOOM 
#define MYTHGOOM

#include "mainvisual.h"

class Goom : public VisualBase
{
public:
    Goom(void);
    ~Goom() override;

    void resize(const QSize &size) override; // VisualBase
    bool process(VisualNode *node) override; // VisualBase
    bool draw(QPainter *p, const QColor &back) override; // VisualBase
    void handleKeyPress([[maybe_unused]] const QString &action) override {}; // VisualBase

private:
    QSize m_size;

    unsigned int *m_buffer {nullptr};
    int           m_scalew {2};
    int           m_scaleh {2};
};

#endif //MYTHGOOM
