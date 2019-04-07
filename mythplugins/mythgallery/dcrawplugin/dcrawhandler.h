#include <QImage>
#include <QImageIOHandler>
#include <QString>

class DcrawHandler : public QImageIOHandler
{
  public:
    bool canRead() const override; // QImageIOHandler
    bool read(QImage *image) override; // QImageIOHandler
    static int loadThumbnail(QImage *image, const QString& fileName);
};

