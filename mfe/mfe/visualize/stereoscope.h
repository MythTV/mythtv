#ifndef STEREOSCOPE_H_
#define STEREOSCOPE_H_
/*
	stereoscope.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A stereoscope visualization
	
*/

#include "visual.h"

class StereoScope : public MfeVisualizer
{

  public:

    StereoScope();
    virtual ~StereoScope();

    void resize(QSize new_size, QColor background_color);
    bool update(QPixmap *pixmap_to_draw_on);


  protected:

    bool process( VisualNode *node );
    bool draw( QPainter *p, const QColor &back );

    QColor startColor, targetColor;
    QMemArray<double> magnitudes;
    QSize my_size;
    bool rubberband;
    double falloff;
};


#endif

