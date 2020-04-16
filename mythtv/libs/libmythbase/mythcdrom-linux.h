#ifndef MYTHCDROM_LINUX_H
#define MYTHCDROM_LINUX_H

class MythCDROM *GetMythCDROMLinux(class QObject* par, const QString& devicePath,
                                   bool SuperMount, bool AllowEject);

#endif // MYTHCDROM_LINUX_H
