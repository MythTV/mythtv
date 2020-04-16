#ifndef MYTHINPUTDEVICEHANDLER_H
#define MYTHINPUTDEVICEHANDLER_H

// Qt
#include <QObject>

// MythTV
#ifdef USING_LIBCEC
#include "devices/mythcecadapter.h"
#endif

class MythInputDeviceHandler : public QObject
{
    Q_OBJECT

  public:
    explicit MythInputDeviceHandler(QWidget* Parent);
   ~MythInputDeviceHandler() override;

    void Start           (void);
    void Stop            (bool Finishing = true);
    void Reset           (void);
    void Action          (const QString &Action);
    void IgnoreKeys      (bool Ignore);

  public slots:
    void MainWindowReady (void);
    void customEvent     (QEvent *Event) override;

  private:
    Q_DISABLE_COPY(MythInputDeviceHandler)

    QWidget*       m_parent     { nullptr };
    bool           m_ignoreKeys { false   };
#ifdef USING_LIBCEC
    MythCECAdapter m_cecAdapter { };
#endif
};

#endif
