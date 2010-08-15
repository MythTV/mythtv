#include <QImage>
#include <QImageIOHandler>

class DcrawHandler : public QImageIOHandler
{

public:

    bool canRead() const;
    bool read(QImage *image);

};

