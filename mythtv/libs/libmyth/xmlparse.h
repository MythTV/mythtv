#ifndef XMLPARSE_H_
#define XMLPARSE_H_

#include "uitypes.h"
#include "mythcontext.h"
#include <qimage.h>
#include <qrect.h>
#include <qlabel.h>
#include <qdom.h>

class XMLParse
{
  public:
    XMLParse();
   ~XMLParse();

    fontProp *GetFont(const QString &);
    LayerSet *GetSet(const QString &text);

    void SetWMult(double wm) { wmult = wm; }
    void SetHMult(double hm) { hmult = hm; }

    bool LoadTheme(QDomElement &, QString, QString sf = "");
    QString getFirstText(QDomElement &);
    void parseFont(QDomElement &);
    void normalizeRect(QRect &);
    QPoint parsePoint(QString);
    QRect parseRect(QString);
    void parsePopup(QDomElement &);
    void parseContainer(QDomElement &, QString &, int &, QRect &);
    void parseListArea(LayerSet *, QDomElement &);
    void parseBar(LayerSet *, QDomElement &);
    void parseGuideGrid(LayerSet *, QDomElement &);
    void parseManagedTreeList(LayerSet *, QDomElement &);
    void parseTextArea(LayerSet *, QDomElement &);
    void parseStatusBar(LayerSet *, QDomElement &);
    void parseImage(LayerSet *, QDomElement &);
    void resizeImage(QPixmap *, QString);
  
  private:
    QMap<QString, fontProp> fontMap;
    QMap<QString, LayerSet*> layerMap;
    vector<LayerSet *> *allTypes;

    double wmult;
    double hmult;
};

#endif
