#ifndef XMLPARSE_H_
#define XMLPARSE_H_

#include <QDomDocument>
#include <QString>
#include <QRect>

class MythUIHelper;

class MPUBLIC XMLParse
{
  public:
    XMLParse();
   ~XMLParse();

    fontProp *GetFont(const QString &, bool checkGlobal = true);
    LayerSet *GetSet(const QString &text);

    void SetWMult(double wm) { wmult = wm; }
    void SetHMult(double hm) { hmult = hm; }
    void SetFontSizeType(QString s) { fontSizeType = s; }

    bool LoadTheme(QDomElement &, QString, QString sf = "");
    QString getFirstText(QDomElement &);
    void parseFont(QDomElement &);
    void normalizeRect(QRect &);
    QPoint parsePoint(QString);
    QRect parseRect(QString);
    void parsePopup(QDomElement &);
    void parseContainer(QDomElement &, QString &, int &, QRect &);
    void parseListArea(LayerSet *, QDomElement &);
    bool parseDefaultCategoryColors(QMap<QString, QString> &catColors);
    void parseManagedTreeList(LayerSet *, QDomElement &);
    void parseTextArea(LayerSet *, QDomElement &);
    void parseStatusBar(LayerSet *, QDomElement &);
    void parseImage(LayerSet *, QDomElement &);
    void parseRepeatedImage(LayerSet *, QDomElement &);
    void parsePushButton(LayerSet *, QDomElement &);
    void parseTextButton(LayerSet *, QDomElement &);
    void parseBlackHole(LayerSet *, QDomElement &);
    void parseListBtnArea(LayerSet *, QDomElement &);
    void parseListTreeArea(LayerSet *, QDomElement &);
    void parseKeyboard(LayerSet *, QDomElement &);
    void parseKey(LayerSet *, QDomElement &);

  private:
    bool doLoadTheme(QDomElement &, QString, QString);
    QMap<QString, fontProp> fontMap;
    QMap<QString, LayerSet*> layerMap;
    vector<LayerSet *> *allTypes;

    double wmult;
    double hmult;

    QString fontSizeType;

    MythUIHelper *ui;
};

#endif
