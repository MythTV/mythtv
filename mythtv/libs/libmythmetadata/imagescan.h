#ifndef IMAGESCAN_H
#define IMAGESCAN_H

// Qt headers

// MythTV headers
#include "imagescanthread.h"
#include "mythmetaexp.h"



class META_PUBLIC ImageScan
{
public:
    static ImageScan*    getInstance();

    void StartSync();
    void StopSync();
    bool SyncIsRunning();

    int  GetCurrent();
    int  GetTotal();

private:
    ImageScan();
    ~ImageScan();
    static ImageScan    *m_instance;

    ImageScanThread     *m_imageScanThread;
};

#endif // IMAGESCAN_H
