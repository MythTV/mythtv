#ifndef NETFLASHER_H_
#define NETFLASHER_H_
/*
	netflasher.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <qtimer.h>

#include <mythtv/uitypes.h>

class NetFlasher : public QObject
{

  Q_OBJECT

  public:

    NetFlasher(UIImageType *my_icon);
    ~NetFlasher();

    void flash(int numb_times = 2, int on_duration = 200, int off_duration = 200);
    void stop();
    
  public slots:
  
    void timerDone();

  private:

    UIImageType *my_icon;
    QTimer *my_timer;
    int flash_on_time;
    int flash_off_time;
    int numb_cycles;
    int cycle_counter;
};



#endif
