/*
	visualwrapper.cpp

	Copyright (c) 2005 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	A class that "holds" all the potential visualizations and provides and
	interface to switch between them
	
*/


#include <iostream>
using namespace std;

#include <qpainter.h>

#include "visualwrapper.h"
#include "visual.h"
#include "stereoscope.h"


VisualWrapper::VisualWrapper(QWidget *parent)
              :QWidget(parent, "visualchooser")
{
    setEraseColor(Qt::black);
    is_full_screen = false;
    small_geometry = QRect(0, 0, 100, 100);
    large_geometry = QRect(0, 0, 100, 100);
    current_visualizer = new StereoScope();

    //
    //  Start the timer that calls the visualization redrawing
    //
    
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    timer->start(1000 / 100);    // 40 = fps


    //
    //  Defaults for text overlay stuff
    //    
    
    text_overlay_brightness = 0;
    current_text_overlay_index = 0;
    text_overlay_mode = MTOM_BRIGHTENING;
}

void VisualWrapper::add(uchar *b, unsigned long b_len, int c, int p)
{
    current_visualizer_mutex.lock();
        if(current_visualizer)
        {
            current_visualizer->add(b, b_len, c, p);
        }
    current_visualizer_mutex.unlock();
}

void VisualWrapper::paintEvent( QPaintEvent* )
{
    bool update_happened = false;
    current_visualizer_mutex.lock();
        if(current_visualizer)
        {
            update_happened = current_visualizer->update(&pixmap);
        }
    current_visualizer_mutex.unlock();

    if(update_happened)
    {
        blitPixmapToScreen();
    }
}

void VisualWrapper::resizeEvent( QResizeEvent *re )
{
    pixmap.resize(re->size());
    pixmap.fill(Qt::black);
    current_visualizer_mutex.lock();
        if(current_visualizer)
        {
            current_visualizer->resize(re->size(), Qt::black);
        }
    current_visualizer_mutex.unlock();
}

void VisualWrapper::timeout()
{
    bool update_happened = false;
    current_visualizer_mutex.lock();
        if(current_visualizer)
        {
            update_happened = current_visualizer->update(&pixmap);
        }
    current_visualizer_mutex.unlock();
    if(update_happened)
    {
        blitPixmapToScreen();
    }
}


void VisualWrapper::toggleSize()
{
    if(is_full_screen)
    {
        setGeometry(small_geometry);
        is_full_screen = false;
    }
    else
    {
        setGeometry(large_geometry);
        is_full_screen = true;
        text_overlay_mode = MTOM_BRIGHTENING;
        text_overlay_brightness = 0;
        current_text_overlay_index = 0;
    }
}

void VisualWrapper::blitPixmapToScreen()
{

    //
    //  If were in full screen mode, add some text
    //
    
    if(is_full_screen)
    {
        doTextOverlay();
    }        
        
    bitBlt(this, 0, 0, &pixmap);
}

void VisualWrapper::doTextOverlay()
{
    QPainter p(&pixmap);
    QFont font( "Arial", 18 );
    p.setFont( font );

    if (text_overlay_mode == MTOM_BRIGHTENING)
    {
        p.setPen(QColor(text_overlay_brightness, text_overlay_brightness, text_overlay_brightness));
        text_overlay_brightness += 5;
        if(text_overlay_brightness >= 255)
        {
            text_overlay_brightness = 255;
            text_overlay_mode = MTOM_SOLID;
        }
    }
    else if (text_overlay_mode == MTOM_SOLID)
    {
        p.setPen(QColor(255, 255, 255));
        text_overlay_brightness--;
        if(text_overlay_brightness < 0)
        {
            text_overlay_brightness = 255;
            text_overlay_mode = MTOM_FADING;
        }
    }
    else if (text_overlay_mode == MTOM_FADING)
    {
        p.setPen(QColor(text_overlay_brightness, text_overlay_brightness, text_overlay_brightness));
        text_overlay_brightness -= 5;
        if(text_overlay_brightness < 0)
        {
            text_overlay_brightness = 0;
            text_overlay_mode = MTOM_BLANK;
        }
    }
    else if (text_overlay_mode == MTOM_BLANK)
    {
        p.setPen(QColor(0, 0, 0));
        text_overlay_brightness += 1;
        if(text_overlay_brightness >= 255)
        {
            text_overlay_brightness = 0;
            text_overlay_mode = MTOM_BRIGHTENING;
            current_text_overlay_index++;
            if(current_text_overlay_index >= text_overlay_strings.count())
            {
                current_text_overlay_index = 0;
            }
        }
    }
        
    if(
        text_overlay_mode != MTOM_BLANK &&
        current_text_overlay_index < text_overlay_strings.count()
      )
    {
        p.drawText(60, this->height() - 50, text_overlay_strings[current_text_overlay_index]);
    }
}

void VisualWrapper::assignOverlayText(QStringList string_list)
{
    text_overlay_strings = string_list;
    current_text_overlay_index = 0;
    text_overlay_mode = MTOM_BRIGHTENING;
    text_overlay_brightness = 0;
}

VisualWrapper::~VisualWrapper()
{
    if(current_visualizer)
    {
        delete current_visualizer;
        current_visualizer = NULL;
    }
}

