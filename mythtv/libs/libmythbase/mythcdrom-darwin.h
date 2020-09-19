#ifndef MYTHCDROM_DARWIN_H_
#define MYTHCDROM_DARWIN_H_

class MythCDROM *GetMythCDROMDarwin(QObject* par, const QString& devicePath,
                                    bool SuperMount, bool AllowEject);

#endif // MYTHCDROM_DARWIN_H_
