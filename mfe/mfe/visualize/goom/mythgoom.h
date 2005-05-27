#ifndef MYTHGOOM_H_ 
#define MYTHGOOM_H_
/*
	mythgoom.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A goom visualization
	
*/


#include "../visual.h"

class Goom : public MfeVisualizer
{

public:
    Goom();
    virtual ~Goom();

    void resize(QSize size, QColor qc);
    bool update(QPixmap *pixmap_to_draw_on);

private:

    bool process(VisualNode *node);
    bool draw(QPainter *p, const QColor &back);

    QSize size;
    unsigned int *buffer;
    int scalew, scaleh;
};

#endif

