#ifndef _ICONMAP_H_
#define _ICONMAP_H_

class QString;
class QFile;

class IconData
{
  public:
    IconData() : quiet(false) {}

    void UpdateSourceIcons(int sourceid);
    void ImportIconMap(const QString &filename);
    void ExportIconMap(const QString &filename);
    void ResetIconMap(bool reset_icons);

  public:
    bool quiet;
};

#endif // _ICONMAP_H_
