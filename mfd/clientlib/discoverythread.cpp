/*
	discoverythread.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Little thread that keeps looking for (dis)appearance of mfd's

*/

#include <iostream>
using namespace std;

#include "discoverythread.h"


DiscoveryThread::DiscoveryThread()
{
    keep_going = true;
}

void DiscoveryThread::run()
{
    while(keep_going)
    {
        cout << "discovery thread is running" << endl;
    }
}

void DiscoveryThread::stop()
{
    keep_going_mutex.lock();
        keep_going = false;
    keep_going_mutex.unlock();
}

DiscoveryThread::~DiscoveryThread()
{
}

 
