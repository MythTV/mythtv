/*
	netflasher.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include "netflasher.h"

NetFlasher::NetFlasher(UIImageType *icon)
{
    if (!icon)
    {
        cerr << "netflasher.o: initialized with null icon" << endl;
    }

    my_icon = icon;
    flash_on_time = 200;
    flash_off_time = 300;
    cycle_counter = 0;
    
    my_timer = new QTimer(this);
    connect( my_timer, SIGNAL(timeout()), this, SLOT(timerDone()) );    
}

void NetFlasher::flash(int numb_times, int on_duration, int off_duration)
{
    stopNow();
    flash_on_time = on_duration;
    flash_off_time = off_duration;
    numb_cycles = numb_times;
    cycle_counter = 0;

    my_icon->show();
    my_timer->start( flash_on_time, TRUE ); 
}

void NetFlasher::stopNow()
{
    my_timer->stop();
    numb_cycles = 0;
    cycle_counter = 0;
    my_icon->hide();
}

void NetFlasher::stop()
{
    numb_cycles = 1;
    cycle_counter = 2;
}

void NetFlasher::timerDone()
{
    cycle_counter++;
    if (cycle_counter >= (numb_cycles * 2) && numb_cycles > 0)
    {
        my_timer->stop();
        my_icon->hide();
    }
    else
    {
        if (my_icon->isShown())
        {
            my_icon->hide();
            my_timer->start( flash_off_time, TRUE );
        }
        else
        {
            my_icon->show();
            my_timer->start( flash_on_time, TRUE );
        }
    }
}

NetFlasher::~NetFlasher()
{
    if (my_timer)
    {
        delete my_timer;
        my_timer = NULL;
    }
}
