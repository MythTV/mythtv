#ifndef MYTHCDROM_H
#define MYTHCDROM_H

#include "mythmedia.h"

class MBASE_PUBLIC MythCDROM : public MythMediaDevice
{
    Q_OBJECT
  protected:
    MythCDROM(QObject* par, const QString& DevicePath, bool SuperMount,
              bool AllowEject);
  public:
    ~MythCDROM() override = default;

    virtual bool mediaChanged(void) { return false; }
    virtual bool checkOK(void)      { return true; }
    MythMediaStatus checkMedia(void) override // MythMediaDevice
    {
        return setStatus(MEDIASTAT_UNKNOWN, false);
    }
    void setDeviceSpeed(const char *devicePath, int speed) override; // MythMediaDevice

    static MythCDROM* get(QObject* par, const QString& devicePath,
                                  bool SuperMount, bool AllowEject);

    enum ImageType : std::uint8_t
    {
        kUnknown,
        kBluray,
        kDVD
    };

    static ImageType inspectImage(const QString& path);

  protected:
    void onDeviceMounted() override; // MythMediaDevice
};

#endif
