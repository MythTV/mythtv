#include <QImage>
#include <QImageIOHandler>
#include <QString>

class DcrawHandler : public QImageIOHandler
{
  public:
    bool canRead() const;
    bool read(QImage *image);
    static int loadThumbnail(QImage *image, QString fileName);
};

