#ifndef MYTHXMLPARSER_H_
#define MYTHXMLPARSER_H_
/*
	xmlparser.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Based on old libmyth xml parser, which was (C) .... uhm ... dunno?
	
*/

#include <qdom.h>
#include "mythfontproperties.h"
#include "mythcontainer.h"
#include "mythuitype.h"

class MythXMLParser
{
  public:
    MythXMLParser();
   ~MythXMLParser();

    void SetWMult(double wm) { wmult = wm; }
    void SetHMult(double hm) { hmult = hm; }
    void setOwner(MythUIType *l_owner) { owner = l_owner;}
    void setTheme(const QString &theme_file, const QString &theme_dir);

    //
    //  The method that actually loads the xml theme file and fills the
    //  target variable (&ele) with the xml data that describes a given
    //  screen
    //

    bool LoadTheme(QDomElement &ele, QString screen_name);
                  
    void                parseFont(QDomElement &);
    MythFontProperties  *GetFont(const QString &);
    QString             getFirstText(QDomElement &);
    void                normalizeRect(QRect &);
    QPoint              parsePoint(QString);
    QRect               parseRect(QString);
    MythUIContainer*    parseContainer(QDomElement &e);
    MythUIContainer*    getContainer(const QString &a_name);
    void                parseImage(MythUIContainer *, QDomElement &);
    void                parseTextArea(MythUIContainer *, QDomElement &);


/*

    void SetWMult(double wm) { wmult = wm; }
    void SetHMult(double hm) { hmult = hm; }
    void SetFontSizeType(QString s) { fontSizeType = s; }

    void parsePopup(QDomElement &);
    void parseListArea(LayerSet *, QDomElement &);
    void parseBar(LayerSet *, QDomElement &);
    void parseGuideGrid(LayerSet *, QDomElement &);
    void parseManagedTreeList(LayerSet *, QDomElement &);
    void parseTextArea(LayerSet *, QDomElement &);
    void parseMultiTextArea(LayerSet *, QDomElement &);
    void parseStatusBar(LayerSet *, QDomElement &);
    void parseAnimatedImage(LayerSet *, QDomElement &);
    void parseRepeatedImage(LayerSet *, QDomElement &);
    void parsePushButton(LayerSet *, QDomElement &);
    void parseTextButton(LayerSet *, QDomElement &);
    void parseCheckBox(LayerSet *, QDomElement &);
    void parseSelector(LayerSet *, QDomElement &);
    void parseBlackHole(LayerSet *, QDomElement &);
    void parseListBtnArea(LayerSet *, QDomElement &); 
    void parseListTreeArea(LayerSet *, QDomElement &);
    void parseKeyboard(LayerSet *, QDomElement &);
    void parseKey(LayerSet *, QDomElement &);

*/
 
  private:

    MythUIType                        *owner;
    QMap<QString, MythFontProperties> fontMap;
    QMap<QString, MythUIContainer*>   m_containers;
    double                            wmult;
    double                            hmult;
    QString                           m_themedir;
    QString                           m_themedir_alt;
    QString                           m_basedir;
    QString                           m_basedir_alt;
    QString                           m_theme_file;
};

#endif
