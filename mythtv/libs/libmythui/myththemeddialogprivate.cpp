/*
	themeddialogprivate.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
*/

#include "myththemeddialogprivate.h"

MythUIThemedDialogPrivate::MythUIThemedDialogPrivate(
                                       MythUIThemedDialog *l_owner, 
                                       const QString &screen_name,
                                       const QString &theme_file,
                                       const QString &theme_dir
                                                    )
{
    m_owner = l_owner;
    
    m_screen_name = screen_name;
    m_theme_file = theme_file;
    m_theme_dir = theme_dir;
    
    m_xml_parser.setOwner(m_owner, this);
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

void MythUIThemedDialogPrivate::addWidgetToMap(MythUIType *new_widget)
{
    if(m_flat_widget_map[new_widget->name()])
    {
        cerr << "myththemeddialog.o: a dialog called \""
             << m_screen_name
             << "\" has multiple widgets called \""
             << new_widget->name()
             << "\", which is not a good idea."
             << endl;
    }

    m_flat_widget_map.insert(new_widget->name(), new_widget);
}

MythUITree* MythUIThemedDialogPrivate::getMythUITree(const QString &widget_name)
{
    MythUIType *possible_return =  m_flat_widget_map[widget_name];

    if(possible_return)
    {
            MythUITree *actual_return = NULL;
            if ( (actual_return = dynamic_cast<MythUITree*>(possible_return)) )
            {
                return actual_return;
            }
    }
    return NULL;
}

MythUIThemedDialogPrivate::~MythUIThemedDialogPrivate()
{
}


