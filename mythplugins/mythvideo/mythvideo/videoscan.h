#ifndef VIDEO_SCANNER_H
#define VIDEO_SCANNER_H

#include <qstringlist.h> // optional

class VideoScanner
{
    public:
        VideoScanner();
        ~VideoScanner();

        void doScan(const QStringList &dirs);

    private:
        class VideoScannerImp *m_imp;
};

#endif
