#ifndef VISUAL_H_
#define VISUAL_H_
/*
	visual.h

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Base class for mfe visualizations
	
*/
#include <deque>
using namespace std;
#include <qwidget.h>
#include <mfdclient/visualbase.h>
#include "visualnode.h"


enum MfeVisualizationType
{
    MVT_MONOSCOPE = 0,
    MVT_STEREOSCOPE,
    MVT_GOOM
};

class MfeVisualizer : public VisualBase
{

  public:

    MfeVisualizer();
    virtual ~MfeVisualizer();

    //
    //  This is called in a separate thread, when PCM audio data arrives. It
    //  is the raw audio data that has come over rtsp. Note that channels
    //  will always be 2 and bits per channel will always be 16 for the
    //  forseeable future (unless someone rewrites all the mfd rtsp
    //  streaming stuff to handle over formats).
    //

    virtual void add(
                        uchar          *raw_audio_data_buffer, 
                        unsigned long   length_of_buffer, 
                        int             numb_channels, 
                        int             bits_per_channel
                    );


    //
    //  This is called whenever the visualizer needs to resize (usually
    //  going from small to fullscreen and back again).
    //

    virtual void resize(QSize new_size, QColor background_color);

    //
    //  This is called whenever it should draw itself. Returns true if it
    //  drew anything.
    //

    virtual bool update(QPixmap *pixmap_to_draw_on);
    
    //
    //  A way to get the desired frames per second for any given visualization
    //
  
    virtual int getFps(){ return fps; }
  
    //
    //  Programmatically find out what the current viz is
    //
    
    MfeVisualizationType    getVisualizationType(){ return visualization_type; }
  
  protected:
  
    int                     fps;
    MfeVisualizationType    visualization_type;
//    QPtrList<VisualNode>    nodes;
    std::deque<VisualNode*> nodes;
    
};

#endif
