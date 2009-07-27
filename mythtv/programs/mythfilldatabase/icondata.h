#ifndef _ICONMAP_H_
#define _ICONMAP_H_

#include <vector>
using namespace std;

#include <QUrl>
#include <QString>
#include <QMap>

#include "mythhttppool.h"

class IconData : private MythHttpListener
{
  public:
    IconData() {}
    ~IconData();

    void UpdateSourceIcons(uint sourceid);
    void ImportIconMap(const QString &filename);
    void ExportIconMap(const QString &filename);
    void ResetIconMap(bool reset_icons);

  private:
    class FI { public: QString filename; uint chanid; };
    typedef vector<FI> FIL;
    typedef QMap<QUrl,FIL> Url2FIL;

    // Implements MythHttpListener
    virtual void Update(QHttp::Error      error,
                        const QString    &error_str,
                        const QUrl       &url,
                        uint              http_status_id,
                        const QString    &http_status_str,
                        const QByteArray &data);

    bool Save(const FI &fi, const QByteArray &data);

    QMutex  m_u2fl_lock;
    Url2FIL m_u2fl;
};

#endif // _ICONMAP_H_
