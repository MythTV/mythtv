/*
	themeddialog.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include <iostream>
using namespace std;

#include "myththemeddialog.h"
#include "mythfontproperties.h"
#include "mythuitext.h"
#include "mythmainwindow.h"
#include "mythxmlparser.h"
#include "mythcontainer.h"

class MythUIThemedDialogPrivate
{
  public:
  
    MythUIThemedDialogPrivate(MythUIThemedDialog *l_owner,
                              const QString &screen_name,
                              const QString &theme_file,
                              const QString &theme_dir,
                              float lwmult,
                              float lhmult);
    ~MythUIThemedDialogPrivate();


  private:

    void                        loadScreen();
    void                        parseContainer(QDomElement &e);
  
    QString                     m_screen_name;
    QString                     m_theme_file;
    QString                     m_theme_dir;
    float                       wmult, hmult;
    MythXMLParser               m_xml_parser;
    QDomElement                 m_xml_data;
    QPtrList<MythUIContainer>   m_containers;
    MythUIThemedDialog          *owner;
};




MythUIThemedDialog::MythUIThemedDialog(MythScreenStack *parent, 
                                       const QString &screen_name,
                                       const QString &theme_file,
                                       const QString &theme_dir) 
         : MythScreenType(parent, screen_name)
{

    d = new MythUIThemedDialogPrivate(this, 
                                      screen_name, 
                                      theme_file, 
                                      theme_dir,
                                      wmult, 
                                      hmult);

}

bool MythUIThemedDialog::keyPressEvent(QKeyEvent *e)
{
    
    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (unsigned int i = 0; i < actions.size() && !handled; i++)
        {
            QString action = actions[i];
            handled = true;

            if (action == "ESCAPE")
            {
                m_ScreenStack->PopScreen();
            }
            else
            {
                handled = false;
            }
        }
    }

    return handled;
}

MythUIThemedDialog::~MythUIThemedDialog()
{
    if(d)
    {
        delete d;
        d = NULL;
    }
}

/*
---------------------------------------------------------------------
*/


MythUIThemedDialogPrivate::MythUIThemedDialogPrivate(
                                       MythUIThemedDialog *l_owner, 
                                       const QString &screen_name,
                                       const QString &theme_file,
                                       const QString &theme_dir,
                                       float lwmult,
                                       float lhmult
                                                    )
{
    owner = l_owner;
    
    m_screen_name = screen_name;
    m_theme_file = theme_file;
    m_theme_dir = theme_dir;
    
    wmult = lwmult;
    hmult = lhmult;
    
    m_xml_parser.SetWMult(wmult);
    m_xml_parser.SetHMult(hmult);
    m_xml_parser.setOwner(owner);
    m_xml_parser.setTheme(theme_file, theme_dir);
    
    //
    //  Try and fill our m_xml_data member variable with the xml data
    //  related to this screen for the given theme
    //

    if (!m_xml_parser.LoadTheme(m_xml_data, m_screen_name))
    {
        cerr << "myththemeddialog.o: Couldn't find a screen/window "
             << "description called \""
             << m_screen_name
             << "\" in a theme file with a basename of \""
             << m_theme_file
             << "\""
             << endl;
        exit(-31);
    }
    
    //
    //  Okey dokey, we have filled our m_xml_data variable with stuff to
    //  display. Time to create the widgets and whatnot that the xml data
    //  says we should.
    //

    loadScreen();

}

void MythUIThemedDialogPrivate::loadScreen()
{

    //
    //  Parse all the child elements in the theme
    //

    for (
            QDomNode child = m_xml_data.firstChild(); 
            !child.isNull();
            child = child.nextSibling()
        )
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                m_xml_parser.parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            /*
            else if (e.tagName() == "popup")
            {
                parsePopup(e);
            }
            */
            else
            {
                cerr << "myththemeddialog.o: I don't understand this "
                     << "DOM Element: \"" 
                     << e.tagName() 
                     << "\""
                     << endl;
                exit(-32);
            }
        }
    }
}

void MythUIThemedDialogPrivate::parseContainer(QDomElement &element)
{

    //
    //  Have the xml parser deal with the containers but hold a pointer to
    //  each of them so that we can iterate over them later.
    //

    MythUIContainer *new_container = m_xml_parser.parseContainer(element);
    if(new_container)
    {
        m_containers.append(new_container);    
    }
}



MythUIThemedDialogPrivate::~MythUIThemedDialogPrivate()
{
}


