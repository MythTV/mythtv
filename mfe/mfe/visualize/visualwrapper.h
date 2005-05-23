#ifndef VISUALWRAPPER_H_
#define VISUALWRAPPER_H_
/*
	visualwrapper.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A class that "holds" all the potential visualizations and provides and
	interface to switch between them
	
*/

#include <qwidget.h>
#include <qtimer.h>
#include <qpixmap.h>
#include <mfdclient/visualbase.h>

class MfeVisualizer;

enum MfeTextOverlayMode
{
    MTOM_BRIGHTENING = 0,
    MTOM_SOLID,
    MTOM_FADING,
    MTOM_BLANK
};            

class VisualWrapper : public QWidget, public VisualBase
{

  Q_OBJECT

  public:

    VisualWrapper(QWidget *parent);
    ~VisualWrapper();

    void add(uchar *b, unsigned long b_len, int c, int p);

    void paintEvent( QPaintEvent *pe );
    void resizeEvent( QResizeEvent *re );

    void toggleSize();
    void storeSmallGeometry(QRect r){ small_geometry = r; }
    void storeLargeGeometry(QRect r){ large_geometry = r; }
    
    void assignOverlayText(QStringList string_list);
    bool isFullScreen(){ return is_full_screen; }
    void cycleVisualizations();

  public slots:
  
    void timeout();
 

  private:

    void blitPixmapToScreen();
    void doTextOverlay();
  
    MfeVisualizer  *current_visualizer;
    QMutex          current_visualizer_mutex;
    QTimer         *timer;
    bool            is_full_screen;
    QRect           small_geometry;
    QRect           large_geometry;
    QPixmap         pixmap;

    QTimer          text_overlay_timer;
    int             text_overlay_mode;
    QStringList     text_overlay_strings;
    QString         current_text_overlay_string;
    uint            current_text_overlay_index;
    int             text_overlay_brightness;
    
};

#endif
