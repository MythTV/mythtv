// Qt headers

// MythTV headers
#include "mythcontext.h"
#include "imagescan.h"



ImageScan* ImageScan::m_instance = NULL;

ImageScan::ImageScan()
{
    m_imageScanThread = new ImageScanThread();
}



ImageScan::~ImageScan()
{
    if (m_imageScanThread)
    {
        delete m_imageScanThread;
        m_imageScanThread = NULL;
    }
}



ImageScan* ImageScan::getInstance()
{
    if (!m_instance)
        m_instance = new ImageScan();

    return m_instance;
}



void ImageScan::StartSync()
{
    if (m_imageScanThread && !m_imageScanThread->isRunning())
    {
        m_imageScanThread->m_continue = true;
        m_imageScanThread->start();
    }
}



void ImageScan::StopSync()
{
    if (m_imageScanThread && m_imageScanThread->isRunning())
        m_imageScanThread->m_continue = false;
}



bool ImageScan::SyncIsRunning()
{
    if (m_imageScanThread)
        return m_imageScanThread->isRunning();

    return false;
}



int ImageScan::GetCurrent()
{
    if (m_imageScanThread)
        return m_imageScanThread->m_progressCount;

    return 0;
}



int ImageScan::GetTotal()
{
    if (m_imageScanThread)
        return m_imageScanThread->m_progressTotalCount;

    return 0;
}
