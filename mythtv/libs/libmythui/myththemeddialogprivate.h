#ifndef THEMEDDIALOGPRIVATE_H_
#define THEMEDDIALOGPRIVATE_H_
/*
	myththemeddialogprivate.h

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <iostream>
using namespace std;

#include <qdict.h>

#include "myththemeddialog.h"
#include "mythfontproperties.h"
#include "mythuitext.h"
#include "mythmainwindow.h"
#include "mythxmlparser.h"
#include "mythuicontainer.h"
#include "mythuitree.h"

class MythUIThemedDialogPrivate
{
  public:
  
    MythUIThemedDialogPrivate(MythUIThemedDialog *l_owner,
                              const QString &screen_name,
                              const QString &theme_file,
                              const QString &theme_dir);
    
    void addWidgetToMap(MythUIType *new_widget);

    //
    //  Actual method to get pointers to named widgets
    //
    
    MythUITree* getMythUITree(const QString &widget_name);

    ~MythUIThemedDialogPrivate();


  private:

    void                        loadScreen();
    void                        parseContainer(QDomElement &e);
  
    QString                     m_screen_name;
    QString                     m_theme_file;
    QString                     m_theme_dir;
    MythXMLParser               m_xml_parser;
    QDomElement                 m_xml_data;
    QPtrList<MythUIContainer>   m_containers;
    MythUIThemedDialog         *m_owner;
    QDict<MythUIType>           m_flat_widget_map;
};

#endif
