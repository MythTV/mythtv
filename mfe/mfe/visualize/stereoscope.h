#ifndef STEREOSCOPE_H_
#define STEREOSCOPE_H_
/*
	stereoscope.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A stereoscope visualization
	
*/

#include "visual.h"

class StereoScope : public QWidget
{

  Q_OBJECT

  public:

    StereoScope(QWidget *parent);
    virtual ~StereoScope();

    void add(uchar *b, unsigned long b_len, unsigned long w, int c, int p);
    //void resize( const QSize &size );
    bool process( VisualNode *node );
    bool draw( QPainter *p, const QColor &back );

    void paintEvent( QPaintEvent * );
    void resizeEvent( QResizeEvent * );
  
  public slots:

    void timeout();         



protected:

    QTimer  *timer;
    int     fps;
    QPixmap pixmap;
    QPtrList<VisualNode> nodes;

    QColor startColor, targetColor;
    QMemArray<double> magnitudes;
    QSize my_size;
    bool rubberband;
    double falloff;
};


#endif

