#ifndef MYTHCONTAINER_H_
#define MYTHCONTAINER_H_
/*
    mythcontainer.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Object that holds UI widgets in a logical group.
	
*/


#include <qstring.h>
#include <qrect.h>

class MythUIContainer
{
  public:

    MythUIContainer(const QString &name);
   ~MythUIContainer();

    void    setDebug(bool db) { m_debug = db; }
    void    setArea(QRect area ) { m_area = area; }
    QRect   getArea(){ return m_area; }
    QPoint  getPosition() { return m_area.topLeft(); }

/*
    void    Draw(QPainter *, int, int);
    void    DrawRegion(QPainter *, QRect &, int, int);

    QString GetName() { return m_name; }
    void    SetName(const QString &name) { m_name = name; }

    void    SetDrawOrder(int order) { m_order = order; }
    int     GetDrawOrder() const { return m_order; }

    void    SetAreaRect(QRect area) { m_area = area; }
    QRect   GetAreaRect() { return m_area; }

    void    SetContext(int con) { m_context = con; }
    int     GetContext(void) { return m_context; }

    void    bumpUpLayers(int a_number);
    int     getLayers(){return numb_layers;}

    void    AddType(UIType *);
    UIType  *GetType(const QString &name);
    vector<UIType *> *getAllTypes(){return allTypes;}

    void    ClearAllText(void);
    void    SetText(QMap<QString, QString> &infoMap);

    void    SetDrawFontShadow(bool state);

    void    UseAlternateArea(bool useAlt);

*/

  private:

    QString m_name;
    bool    m_debug;
    QRect   m_area;

/*
    QRect   m_area;
    int     numb_layers;
    int     m_context;
    int     m_order;

    QMap<QString, UIType *> typeList;
    vector<UIType *> *allTypes;

*/

};


#endif
