/*
	xmlparser.cpp

	Copyright (c) 2004 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Based on old libmyth xml parser, which was (C) .... uhm ... dunno?
	
*/

#include <cmath>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qfile.h>

#include "mythcontext.h"
#include "mythxmlparser.h"
#include "mythmainwindow.h"
#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythuitree.h"
#include "myththemeddialogprivate.h"

MythXMLParser::MythXMLParser(void)
{
    wmult = 800;
    hmult = 600;
    m_owner = NULL;
    m_private_owner = NULL;
    
    m_themedir     = "";
    m_themedir_alt = "";

    m_basedir      = gContext->GetShareDir() + "/themes/default/";
    m_basedir_alt  = gContext->GetShareDir() + "/themes/default/images/";
}

void MythXMLParser::setTheme(const QString &theme_file, const QString &theme_dir)
{
    m_themedir     = theme_dir;
    m_themedir_alt = QString("%1/images").arg(theme_dir);

    m_themedir     = m_themedir.local8Bit();
    m_themedir_alt = m_themedir_alt.local8Bit();
    
    m_theme_file = theme_file.local8Bit();
}


MythXMLParser::~MythXMLParser()
{
/*
    vector<LayerSet *>::iterator i = allTypes->begin();
    for (; i != allTypes->end(); i++)
    {
        LayerSet *type = (*i);
        if (type)
            delete type;
    }
    delete allTypes;
*/
}



bool MythXMLParser::LoadTheme(QDomElement &ele, QString screen_name)
{

    QDomDocument doc;
    QFile f;
    
    if(m_themedir.length() < 1)
    {
        cerr << "mythxmlparser.o: LoadTheme() called, but no theme directory "
             << "is set. Did something forget to call "
             << "MythXMLParser::setTheme() ?"
             << endl;
        f.setName(QString("%1/%2").arg(m_basedir).arg(m_theme_file));
    }
    else
    {
        f.setName(QString("%1/%2").arg(m_themedir).arg(m_theme_file));
    }
     
    if (!f.open(IO_ReadOnly))
    {
        f.setName(QString("%1/%2").arg(m_basedir).arg(m_theme_file));
        if (!f.open(IO_ReadOnly))
        {
            cerr << "mythxmlparser.o: can't open: " << f.name() << endl;
            return false;
        }
    }

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
    {
        cerr << "mythxmlparser.o: error parsing \"" 
             << f.name() 
             << "\" at line " << errorLine << ", column " << errorColumn 
             << " (error message was \""
             << errorMsg
             << "\")"
             << endl;
        f.close();
        return false;
    }

    f.close();

    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "screen")
            {
                QString name = e.attribute("name", "");
                if (name.isNull() || name.isEmpty())
                {
                    cerr << "mythxmlparser.o: screen needs a name"
                         << endl;
                    return false;
                }

                if (name == screen_name)
                {
                    ele = e;
                    return true;
                }
            }
            else
            {
                cerr << "mythxmlparser.o: unknown element \"" 
                     << e.tagName() 
                     << "\""
                     << endl;
                return false;
            }
        }
        n = n.nextSibling();
    }

    return false;
}


void MythXMLParser::parseFont(QDomElement &element)
{
    //
    //  Default values for any font to be created
    //

    QString name;
    QString face;
    QString bold;
    int     size = -1;
    QPoint  shadowOffset = QPoint(0, 0);
    QString color = "#ffffff";
    QString shadow_color = "#000000";
    int     outline_size = -1;
    QString outline_color = "#000000";

    //
    //  Make sure the font has name
    //

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "mythxmlparser.o: a font needs a name"
             << endl;
        return;
    }

    //
    //  Make sure the font has a face defined
    //

    face = element.attribute("face", "");
    if (face.isNull() || face.isEmpty())
    {
        cerr << "mythxmlparser.o: a font called \""
             << name 
             << "\" does not have a face declared, I "
             << " am arbitrarily making it Arial"
             << endl;
        face = "Arial";
    }


    //
    //  Cycle through any other attributes of the font as defined in the xml
    //  data
    //

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "size")
            {
                size = getFirstText(info).toInt();
            }
            else if (info.tagName() == "color")
            {
                color = getFirstText(info);
            }
            else if (info.tagName() == "shadowcolor"   || 
                     info.tagName() == "shadow_color"  ||
                     info.tagName() == "shadowcolour"  ||
                     info.tagName() == "shadow_colour" )
            {
                shadow_color = getFirstText(info);
            }
            else if (info.tagName() == "shadowposition" || 
                     info.tagName() == "shadow_position")
            {
                shadowOffset = parsePoint(getFirstText(info));
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
            }
            else if (info.tagName() == "bold")
            {
                bold = getFirstText(info);
            }
            else if (info.tagName() == "outline")
            {
                outline_size = getFirstText(info).toInt();
            }
            else if (info.tagName() == "outlinecolor"  || 
                     info.tagName() == "outline_color" ||
                     info.tagName() == "outlinecolour" ||
                     info.tagName() == "outline_colour")
            {
                outline_color = getFirstText(info);
            }
            else
            {
                cerr << "mythxmlparser.o: Unknown tag \"" 
                     << info.tagName() 
                     << "\" in font called \""
                     << name
                     << "\""
                     << endl;
                return;
            }
        }
    }

    MythFontProperties *testFont = GetFont(name);
    if (testFont)
    {
        cerr << "mythxmlparser.o: already have a font called \"" 
             << name 
             << "\" (second instance being ignored)"
             << endl;
        return;
    }

    if (size < 0)
    {
        cerr << "mythxmlparser.o: Font called \""
             << name
             << "\" has a bad size (<0), I am arbitrarily "
             << "resizing it to 12"
             << endl;
        size = 12;
    }

    size = (int)ceil(size * hmult);

    //
    //  This calls global function CreatFont() to get the font at the right
    //  size no matter what the DPI of the display we're running on
    //

    QFont temp = GetMythMainWindow()->CreateFont(face, size);

    if (bold.lower() == "yes" ||
        bold.lower() == "true")
    {
        temp.setBold(true);
    }

    QColor foreColor(color);
    QColor shadowColor(shadow_color);

    MythFontProperties newFont;
    newFont.face = temp;
    newFont.color = foreColor;
    newFont.shadowColor = shadowColor;
    newFont.shadowOffset = shadowOffset;
    
    if (outline_size > 0)
    {
        newFont.hasOutline = true;
        newFont.outlineSize = outline_size;
        newFont.outlineColor = outline_color;
    }

    fontMap[name] = newFont;
}


MythFontProperties *MythXMLParser::GetFont(const QString &text)
{
    MythFontProperties *return_value = NULL;
    if (fontMap.contains(text))
    {
        return_value = &fontMap[text];
    }

    return return_value;
}

QString MythXMLParser::getFirstText(QDomElement &element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}


void MythXMLParser::normalizeRect(QRect &rect)
{
    rect.setWidth((int)(rect.width() * wmult));
    rect.setHeight((int)(rect.height() * hmult));
    rect.moveTopLeft(QPoint((int)(rect.x() * wmult),
                            (int)(rect.y() * hmult)));
}

QPoint MythXMLParser::parsePoint(QString text)
{
    int x, y;
    QPoint retval(0, 0);
    if (sscanf(text.data(), "%d,%d", &x, &y) == 2)
        retval = QPoint(x, y);
    return retval;
}

QRect MythXMLParser::parseRect(QString text)
{
    int x, y, w, h;
    QRect retval(0, 0, 0, 0);
    if (sscanf(text.data(), "%d,%d,%d,%d", &x, &y, &w, &h) == 4)
    {
        retval = QRect(x, y, w, h);
    }
    return retval;
}

MythUIContainer* MythXMLParser::parseContainer(QDomElement &element)
{
    bool debugging = false;

    if(!m_owner)
    {
        cerr << "mythxmlparser.o: you can't call parseContainer() "
             << "without setting an owner"
             << endl;
        return NULL;
    }

    QString debug = "";

    QString name = element.attribute("name", "");

    if (name.isNull() || name.isEmpty())
    {
        cerr << "mythxmlparser.o: a container has no name, which is "
             << "not allowed. Container in question will be ignored."
             << endl;
        return NULL;
    }

    QString area_string = element.attribute("area", "");
    if (area_string.isNull() || area_string.isEmpty())
    {
        cerr << "mythxmlparser.o: a container called \""
             << name
             << "\" has no area, which is "
             << "not allowed. Container in question will be ignored."
             << endl;
        return NULL;
    }
    
    QRect area = parseRect(area_string);
    normalizeRect(area);

    //
    //  Check for debugging being on
    //

    QString debug_string = element.attribute("debug", "");
    if (debug_string.length() > 0) 
    {
        if (debug_string.lower() == "yes" ||
            debug_string.lower() == "true")
        {
            debugging = true;
        }
    }
    
    debug_string = element.attribute("debugging", "");
    if (debug_string.length() > 0) 
    {
        if (debug_string.lower() == "yes" ||
            debug_string.lower() == "true")
        {
            debugging = true;
        }
    }

    MythUIContainer *new_container = getContainer(name);
    if (new_container)
    {
        cerr << "mythxmlparser.o: a container called \""
             << name
             << "\" already exists, second instance is "
             << "being ignored"
             << endl;
        return NULL;
    }

    new_container = new MythUIContainer(m_owner, name);
    new_container->setArea(area);
    m_containers[name] = new_container;
    m_private_owner->addWidgetToMap(new_container);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "debug")
            {
                debug = getFirstText(info);
                if (debug.lower() == "yes" ||
                    debug.lower() == "true")
                {
                    new_container->setDebug(true);
                }
            }
            else if (info.tagName() == "image")
            {
                parseImage(new_container, info);
            }
            else if (info.tagName() == "text"     ||
                     info.tagName() == "textarea" ||
                     info.tagName() == "text_area" )
            {
                parseTextArea(new_container, info);
            }
            else if (info.tagName() == "tree"     ||
                     info.tagName() == "treeview" ||
                     info.tagName() == "tree_view" )
            {
                parseTree(new_container, info);
            }
            else
            {
                cerr << "mythxmlparser.o: container called \""
                     << name
                     << "\" contains an unrecognized tag of \""
                     << info.tagName() 
                     << "\""
                     << endl;
            }
        }
    }
    
    
    /*
    
            if (info.tagName() == "debug")
            {
                debug = getFirstText(info);
                if (debug.lower() == "yes")
                    container->SetDebug(true);
            }
            else if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "image")
            {
                parseImage(container, info);
            }
            else if (info.tagName() == "animatedimage")
            {
                parseAnimatedImage(container, info);
            }
            else if (info.tagName() == "repeatedimage")
            {
                parseRepeatedImage(container, info);
            }
            else if (info.tagName() == "listarea")
            {
                parseListArea(container, info);
            }
            else if (info.tagName() == "listbtnarea")
            {
                parseListBtnArea(container, info);
            }
            else if (info.tagName() == "listtreearea")
            {
                parseListTreeArea(container, info);
            }
            else if (info.tagName() == "textarea")
            {
                parseTextArea(container, info);
            }
            else if (info.tagName() == "multitextarea")
            {
                parseMultiTextArea(container, info);
            }
            else if (info.tagName() == "statusbar")
            {
                parseStatusBar(container, info);
            }
            else if (info.tagName() == "managedtreelist")
            {
                parseManagedTreeList(container, info);
            }
            else if (info.tagName() == "pushbutton")
            {
                parsePushButton(container, info);
            }
            else if (info.tagName() == "textbutton")
            {
                parseTextButton(container, info);
            }
            else if (info.tagName() == "checkbox")
            {
                parseCheckBox(container, info);
            }
            else if (info.tagName() == "selector")
            {
                parseSelector(container, info);
            }
            else if (info.tagName() == "blackhole")
            {
                parseBlackHole(container, info);
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
                container->SetAreaRect(area);
            }
            else if (info.tagName() == "bar")
            {
                parseBar(container, info);
            }
            else if (info.tagName() == "keyboard")
            {
                parseKeyboard(container, info);
            }
            else if (info.tagName() == "guidegrid")
            {
                parseGuideGrid(container, info);
            }
            else
            {
                cerr << "Unknown container child: " << info.tagName() << endl;
                return;
            }
        }
    */

    new_container->setDebug(debugging);
    return new_container;
}

MythUIContainer *MythXMLParser::getContainer(const QString &a_name)
{
    MythUIContainer *return_value = NULL;
    if (m_containers.contains(a_name))
    {
        return_value = m_containers[a_name];
    }
    return return_value;
}

void MythXMLParser::parseImage(MythUIContainer *container, QDomElement &element)
{
    //
    //  Set some defaults for the image type
    //

    QPoint position = QPoint(0, 0);

    //
    //  Check that the image tag has a name and filename
    //

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "mythxmlparser.o: image tag with no name is being ignored"
             << endl;
        return;
    }

    QString file_name = element.attribute("filename", "");
    if (file_name.isNull() || file_name.isEmpty())
    {
        file_name = element.attribute("file_name", "");
        if (file_name.isNull() || file_name.isEmpty())
        {
            file_name = element.attribute("file", "");
            if (file_name.isNull() || file_name.isEmpty())
            {
                cerr << "mythxmlparser.o: image tag with no file|"
                     << "filename|file_name tag is being ignored"
                     << endl;
                return;
            }
        }
    }

    //
    //  Parse any attributes of the image
    //
    
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "position")
            {
                position = parsePoint(getFirstText(info));
                position.setX((int)(position.x() * wmult));
                position.setY((int)(position.y() * hmult));
            }
            else
            {
                cerr << "mythxmlparser.o: ignoring unknown tag called \""
                     << info.tagName()
                     << "\" in image with name of name"
                     << endl;
            }
        }
    }


    //
    //  See if we can find this image somewhere.
    //
    
    QFile f(QString("%1/%2").arg(m_themedir).arg(file_name));
    if  (!f.open(IO_ReadOnly))
    {
        f.setName(QString("%1/%2").arg(m_themedir_alt).arg(file_name));    
        if (!f.open(IO_ReadOnly))
        {
            f.setName(QString("%1/%2").arg(m_basedir).arg(file_name));
            if (!f.open(IO_ReadOnly))
            {
                f.setName(QString("%1/%2").arg(m_basedir_alt).arg(file_name));
                if (!f.open(IO_ReadOnly))
                {
                    cerr << "mythxmlparser.o: Could not find an image file "
                         << "called \""
                         << file_name
                         << "\" in "
                         << m_themedir
                         << ", "
                         << m_themedir_alt
                         << ", "
                         << m_basedir
                         << ", or "
                         << m_basedir_alt
                         << endl;
                    return;
                }
            }
        }
    }
    
    //
    //  If we made it through the above, then we found the file
    //

    MythUIImage *new_image = new MythUIImage(f.name(), container, name);
    f.close();
    new_image->Load();
    m_private_owner->addWidgetToMap(new_image);
    
//    position += container->getPosition();
    new_image->SetPosition(position);

}


void MythXMLParser::parseTextArea(MythUIContainer *container, QDomElement &element)
{
    QRect area = QRect(0,0,container->getArea().width(), container->getArea().height());
    QRect alt_area = QRect(0, 0, 0, 0);
    QPoint shadowOffset = QPoint(0, 0);
    QString cutdown = "";
    QString value = "";
    QString multiline = "";
    QString align = "";

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "mythxmlparser.o: text|textarea tag with no name is being ignored"
             << endl;
        return;
    }

    QString font_name = element.attribute("font", "");
    if (font_name.isNull() || font_name.isEmpty())
    {
        cerr << "mythxmlparser.o: text|textarea tag with no font is being ignored"
             << endl;
        return;
    }

    MythFontProperties *font = GetFont(font_name);
    if (!font)
    {
        cerr << "mythxmlparser.o: Unknown font called \""
             << font_name
             << "\" in text|textarea called \""
             << name
             << "\""
             << endl;
        return;
    }


    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "value")
            {
                if ((value.isNull() || value.isEmpty()) &&
                    info.attribute("lang","") == "")
                {
                    value = qApp->translate("ThemeUI", getFirstText(info));
                }
                else if (info.attribute("lang","").lower() ==
                         gContext->GetLanguage())
                {
                    value = getFirstText(info);
                }
            }
            else if (info.tagName() == "altarea" ||
                     info.tagName() == "alt_area")
            {
                alt_area = parseRect(getFirstText(info));
                normalizeRect(alt_area);
            }
            else if (info.tagName() == "cutdown" ||
                     info.tagName() == "cut_down")
            {
                cutdown = getFirstText(info);
            }
            else if (info.tagName() == "multiline" ||
                     info.tagName() == "multi_line")
            {
                multiline = getFirstText(info);
            }
            else if (info.tagName() == "align" ||
                     info.tagName() == "alignment")
            {
                align = getFirstText(info);
            }
            else
            {
                cerr << "mythxmlparser.o: Unknown tag called \""
                     << info.tagName() 
                     << "\" in text|textarea called \""
                     << name
                     << "\""
                     << endl;
                return;
            }

        }
    }

    //area.moveBy(container->getPosition().x(), container->getPosition().y());
    //alt_area.moveBy(container->getPosition().x(), container->getPosition().y());

    MythUIText *new_text = new MythUIText(value, *font, area, 
                                        alt_area, container, name);

    if (multiline.lower() == "yes" ||
        multiline.lower() == "true")
    {
        new_text->SetJustification(Qt::WordBreak);
    }

    if (cutdown.lower() == "no" ||
        cutdown.lower() == "false")
    {
        new_text->SetCutDown(false);
    }

    if (!align.isNull() && !align.isEmpty())
    {
        int jst = (Qt::AlignTop | Qt::AlignLeft);
        if (multiline.lower() == "yes" ||
            multiline.lower() == "true")
        {
            jst = Qt::WordBreak;
        }
        if (align.lower() == "center")
            new_text->SetJustification(jst | Qt::AlignCenter);
        else if (align.lower() == "right")
            new_text->SetJustification(jst | Qt::AlignRight);
        else if (align.lower() == "allcenter")
            new_text->SetJustification(jst | Qt::AlignHCenter | Qt::AlignVCenter);
        else if (align.lower() == "vcenter")
            new_text->SetJustification(jst | Qt::AlignVCenter);
        else if (align.lower() == "hcenter")
            new_text->SetJustification(jst | Qt::AlignHCenter);
    }
    m_private_owner->addWidgetToMap(new_text);
}

void MythXMLParser::parseTree(MythUIContainer *container, QDomElement &element)
{

    //
    //  Defaults.
    //
    
    MythUITreeType tree_type = MUTT_vertical;
    int numb_columns = 0;
    int numb_rows = 0;
    bool debugging = false;

    //
    //  Create a default font if we need one
    //

    MythFontProperties *default_font = GetFont("myth_xml_parser_default_tree_font");
    if(!default_font)
    {
        QFont temp_font = GetMythMainWindow()->CreateFont("Arial", 12);
        default_font = new MythFontProperties();
        
    }



    //
    //  Area defaults to container area
    //

    QRect area = QRect(0,0,container->getArea().width(), container->getArea().height());

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "mythxmlparser.o: tree tag with no name is being ignored"
             << endl;
        return;
    }
    
    
    //
    //  User can also specify area by saying <tree area="0,0,800,600">
    //

    QString possible_area = element.attribute("area", "");
    if (possible_area.length() > 0)
    {
        area = parseRect(possible_area);
        normalizeRect(area);
    }
    
    //
    //  Figure out the type of tree
    //
    
    QString possible_type = element.attribute("type", "");
    if (possible_type.length() > 0)
    {
        possible_type = possible_type.lower();
        if (possible_type == "vertical")
        {
            tree_type = MUTT_vertical;
        }
        else if (possible_type == "horizontal")
        {
            tree_type = MUTT_horizontal;
        }
        else if (possible_type == "folders" || possible_type == "folder")
        {
            tree_type = MUTT_folder;
        }
    }
    
    //
    //  Get column/row counts
    //
    
    QString possible_columns = element.attribute("columns", "");
    if (possible_columns.length() > 0)
    {
        numb_columns = possible_columns.toInt();
    }
    
    QString possible_rows = element.attribute("rows", "");
    if (possible_rows.length() > 0)
    {
        numb_rows = possible_rows.toInt();
    }

    //
    //  Check for debugging being on
    //

    QString debug_string = element.attribute("debug", "");
    if (debug_string.length() > 0) 
    {
        if (debug_string.lower() == "yes" ||
            debug_string.lower() == "true")
        {
            debugging = true;
        }
    }
    
    debug_string = element.attribute("debugging", "");
    if (debug_string.length() > 0) 
    {
        if (debug_string.lower() == "yes" ||
            debug_string.lower() == "true")
        {
            debugging = true;
        }
    }
    


    //
    //  Do some sanity checking
    //
    
    if(tree_type == MUTT_vertical)
    {
        if(numb_columns < 1)
        {
            cerr << "mythxmlparser.o: vertical type tree with "
                 << "no columns makes no sense"
                 << endl;
        }
        if(numb_rows > 0)
        {
            cerr << "mythxmlparser.o: vertical type tree with "
                 << "rows specified makes no sense"
                 << endl;
        }
        
    }
    else if(tree_type == MUTT_horizontal)
    {
        // etc.
    }
    

    //
    //  Create the tree and prepare to receive sub-elements (by telling it
    //  its type)
    //

    MythUITree *new_tree = new MythUITree(area, container, name);
    new_tree->preInit(tree_type, numb_columns, numb_rows); 
    
    //
    //  Parse sub elements
    //
    
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        {
            //
            //  User can _also_ specify area by saying
            //  <area>0,0,800,600</area>
            //

            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "column")
            {
                parseTreeColumn(new_tree, info);
            }
            else if (info.tagName() == "font")
            {
                MythFontProperties *new_font = GetFont(getFirstText(info));
                if (!new_font)
                {
                    cerr << "mythxmlparser.o: Unknown font called \""
                         << getFirstText(info)
                         << "\" in tree called \""
                         << name
                         << "\""
                         << endl;
                }
             }
             else
             {
                    cerr << "mythxmlparser.o: Unknown tag called \""
                     << info.tagName() 
                     << "\" in tree called \""
                     << name
                     << "\""
                     << endl;
                     return;
            }

        }
    }

    if(debugging)
    {
        new_tree->setDebug(true);
    }
    m_private_owner->addWidgetToMap(new_tree);
}


void MythXMLParser::parseTreeColumn(MythUITree *new_tree, QDomElement &element)
{
    //
    //  Set defaults
    //

    QRect area = QRect(0,0,0,0);
    bool debugging = false;

    //
    //  Find the number identifier for this column
    //
    
    QString number_tag = element.attribute("number", "");
    int number = number_tag.toInt();

    //
    //  Check for debugging being on
    //

    QString debug_string = element.attribute("debug", "");
    if (debug_string.length() > 0) 
    {
        if (debug_string.lower() == "yes" ||
            debug_string.lower() == "true")
        {
            debugging = true;
        }
    }
    
    debug_string = element.attribute("debugging", "");
    if (debug_string.length() > 0) 
    {
        if (debug_string.lower() == "yes" ||
            debug_string.lower() == "true")
        {
            debugging = true;
        }
    }
    
    //
    //  Parse any child tags of the column
    //

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else
            {
                cerr << "mythxmlparser.o: Unknown tag called \""
                     << info.tagName() 
                     << "\" in column description of tree called \""
                     << new_tree->name()
                     << "\""
                     << endl;
                return;
            }

        }
    }

    new_tree->addColumn(number, area, debugging);
}


/*

void XMLParse::parseAnimatedImage(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Animated Image needs a name\n";
        exit(-46);
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "Animated Image needs an order\n";
        exit(-47);
    }

    QString filename = "";
    QPoint pos = QPoint(0, 0);

    QPoint scale = QPoint(-1, -1);
    QPoint skipin = QPoint(0, 0);
    QString interval, startinterval, imagecount;

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "staticsize")
            {
                scale = parsePoint(getFirstText(info));
            }
            else if (info.tagName() == "skipin")
            {
                skipin = parsePoint(getFirstText(info));
                skipin.setX((int)(skipin.x() * wmult));
                skipin.setY((int)(skipin.y() * hmult));
            }
            else if (info.tagName() == "interval")
            {
                interval = getFirstText(info);
            }
            else if (info.tagName() == "startinterval")
            {
                startinterval = getFirstText(info);
            }
            else if (info.tagName() == "imagecount")
            {
                imagecount = getFirstText(info);
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in image\n";
                exit(-49);
            }
        }
    }

    UIAnimatedImageType *image = new UIAnimatedImageType(name, filename, imagecount.toInt(),
        interval.toInt(), startinterval.toInt(), order.toInt(), pos);
    image->SetScreen(wmult, hmult);
    if (scale.x() != -1 || scale.y() != -1)
        image->SetSize(scale.x(), scale.y());
    image->SetSkip(skipin.x(), skipin.y());
    QString flex = element.attribute("fleximage", "");
    if (!flex.isNull() && !flex.isEmpty())
    {
        if (flex.lower() == "yes")
            image->SetFlex(true);
        else
            image->SetFlex(false);
    }

    //image->LoadImage();
    if (context != -1)
    {
        image->SetContext(context);
    }
    image->SetParent(container);
    container->AddType(image);
    container->bumpUpLayers(order.toInt());
    
   
}

void XMLParse::parseRepeatedImage(LayerSet *container, QDomElement &element)
{
    int orientation = 0;
    int context = -1;
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Repeated Image needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "Repeated Image needs an order\n";
        return;
    }

    QString filename = "";
    QPoint pos = QPoint(0, 0);

    QPoint scale = QPoint(-1, -1);
    QPoint skipin = QPoint(0, 0);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "filename")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "staticsize")
            {
                scale = parsePoint(getFirstText(info));
            }
            else if (info.tagName() == "skipin")
            {
                skipin = parsePoint(getFirstText(info));
                skipin.setX((int)(skipin.x() * wmult));
                skipin.setY((int)(skipin.y() * hmult));
            }
            else if (info.tagName() == "orientation")
            {
                QString orient_string = getFirstText(info).lower();
                if (orient_string == "lefttoright")
                {
                    orientation = 0;
                }
                if (orient_string == "righttoleft")
                {
                    orientation = 1;
                }
                if (orient_string == "bottomtotop")
                {
                    orientation = 2;
                }
                if (orient_string == "toptobottom")
                {
                    orientation = 3;
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in repeated image\n";
                return;
            }
        }
    }

    UIRepeatedImageType *image = new UIRepeatedImageType(name, filename, order.toInt(), pos);
    image->SetScreen(wmult, hmult);
    if (scale.x() != -1 || scale.y() != -1)
        image->SetSize(scale.x(), scale.y());
    image->SetSkip(skipin.x(), skipin.y());
    QString flex = element.attribute("fleximage", "");
    if (!flex.isNull() && !flex.isEmpty())
    {
        if (flex.lower() == "yes")
            image->SetFlex(true);
        else
            image->SetFlex(false);
    }

    image->LoadImage();
    if (context != -1)
    {
        image->SetContext(context);
    }
    image->setOrientation(orientation);
    image->SetParent(container);
    container->AddType(image);
    container->bumpUpLayers(order.toInt());
}

void XMLParse::parseGuideGrid(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString align = "";
    QString font = "";
    QString color = "";
    QString seltype = "";
    QString selcolor = "";
    QString reccolor = "";
    QString concolor = "";
    QRect area;
    QPoint textoff = QPoint(0, 0);
    bool cutdown = true;
    bool multiline = false;
    QMap<QString, QString> catColors;
    QMap<int, QString> recImgs;
    QMap<int, QString> arrows;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Guide needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "Guide needs an order\n";
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "solidcolor")
            {
                color = getFirstText(info);
                catColors["none"] = color;
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "align")
            {
                align = getFirstText(info);
            }
            else if (info.tagName() == "cutdown")
            {
                if (getFirstText(info).lower() == "no")
                   cutdown = false;
            }
            else if (info.tagName() == "textoffset")
            {
                textoff = parsePoint(getFirstText(info));
                textoff.setX((int)(textoff.x() * wmult));
                textoff.setY((int)(textoff.y() * hmult));
            }
            else if (info.tagName() == "recordingcolor")
            {
                reccolor = getFirstText(info);
            }
            else if (info.tagName() == "conflictingcolor")
            {
                concolor = getFirstText(info);
            }
            else if (info.tagName() == "multiline")
            {
                if (getFirstText(info).lower() == "yes")
                   multiline = true;
            }
            else if (info.tagName() == "selector")
            {
                QString typ = "";
                QString col = "";
                typ = info.attribute("type");
                col = info.attribute("color");

                selcolor = col;
                seltype = typ;
            }
            else if (info.tagName() == "recordstatus")
            {
                QString typ = "";
                QString img = "";
                int inttype = 0;
                typ = info.attribute("type");
                img = info.attribute("image");

                if (typ == "SingleRecord")
                    inttype = 1;
                else if (typ == "TimeslotRecord")
                    inttype = 2;
                else if (typ == "ChannelRecord")
                    inttype = 3;
                else if (typ == "AllRecord")
                    inttype = 4;
                else if (typ == "WeekslotRecord")
                    inttype = 5;
                else if (typ == "FindOneRecord")
                    inttype = 6;
                else if (typ == "OverrideRecord")
                    inttype = 7;

                recImgs[inttype] = img;
            }
            else if (info.tagName() == "arrow")
            {
                QString dir = "";
                QString imag = "";
                dir = info.attribute("direction");
                imag = info.attribute("image");

                if (dir == "left")
                    arrows[0] = imag;
                else
                    arrows[1] = imag;
            }
            else if (info.tagName() == "catcolor")
            {
                QString cat = "";
                QString col = "";
                cat = info.attribute("category");
                col = info.attribute("color");

                catColors[cat] = col;
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in bar\n";
                return;
            }
        }
    }
    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in guidegrid: " << name << endl;
        return;
    }

    UIGuideType *guide = new UIGuideType(name, order.toInt());
    guide->SetScreen(wmult, hmult);
    guide->SetFont(testfont);
    guide->SetSolidColor(color);
    guide->SetCutDown(cutdown);
    guide->SetArea(area);
    guide->SetCategoryColors(catColors);
    guide->SetTextOffset(textoff);
    if (concolor == "")
        concolor = reccolor;
    guide->SetRecordingColors(reccolor, concolor);
    guide->SetSelectorColor(selcolor);
    for (int i = 1; i <= 7; i++)
        guide->LoadImage(i, recImgs[i]);
    if (seltype.lower() == "box")
        guide->SetSelectorType(1);
    else
        guide->SetSelectorType(2); // solid

    guide->SetArrow(0, arrows[0]);
    guide->SetArrow(1, arrows[1]);

    int jst = Qt::AlignLeft | Qt::AlignTop;
    if (multiline == true)
        jst = Qt::WordBreak;

    if (!align.isNull() && !align.isEmpty())
    {
        if (align.lower() == "center")
            guide->SetJustification(Qt::AlignCenter | jst);
        else if (align.lower() == "right")
            guide->SetJustification(Qt::AlignRight | jst);
        else if (align.lower() == "allcenter")
            guide->SetJustification(Qt::AlignHCenter | Qt::AlignVCenter | jst);
        else if (align.lower() == "vcenter")
            guide->SetJustification(Qt::AlignVCenter | jst);
        else if (align.lower() == "hcenter")
            guide->SetJustification(Qt::AlignHCenter | jst);
    }
    else
        guide->SetJustification(jst);

    align = "";

    if (context != -1)
    {
        guide->SetContext(context);
    }
    container->AddType(guide);
}

void XMLParse::parseBar(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString align = "";
    QString orientation = "horizontal";
    QString filename = "";
    QString font = "";
    QPoint textoff = QPoint(0, 0);
    QPoint iconoff = QPoint(0, 0);
    QPoint iconsize = QPoint(0, 0);
    QRect area;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Bar needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "Bar needs an order\n";
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "orientation")
            {
                orientation = getFirstText(info);
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "imagefile")
            {
                filename = getFirstText(info);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "align")
            {
                align = getFirstText(info);
            }
            else if (info.tagName() == "textoffset")
            {
                textoff = parsePoint(getFirstText(info));
                textoff.setX((int)(textoff.x() * wmult));
                textoff.setY((int)(textoff.y() * hmult));
            }
            else if (info.tagName() == "iconoffset")
            {
                iconoff = parsePoint(getFirstText(info));
                iconoff.setX((int)(iconoff.x() * wmult));
                iconoff.setY((int)(iconoff.y() * hmult));
            }
            else if (info.tagName() == "iconsize")
            {
                iconsize = parsePoint(getFirstText(info));
                iconsize.setX((int)(iconsize.x() * wmult));
                iconsize.setY((int)(iconsize.y() * hmult));
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in bar\n";
                return;
            }
        }
    }
    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in bar: " << name << endl;
        return;
    }

    UIBarType *bar = new UIBarType(name, filename, order.toInt(), area);
    bar->SetScreen(wmult, hmult);
    bar->SetFont(testfont);
    bar->SetTextOffset(textoff);
    bar->SetIconOffset(iconoff);
    bar->SetIconSize(iconsize);
    if (orientation == "horizontal")
       bar->SetOrientation(1);
    else if (orientation == "vertical")
       bar->SetOrientation(2);

    if (!align.isNull() && !align.isEmpty())
    {
        if (align.lower() == "center")
            bar->SetJustification(Qt::AlignCenter);
        else if (align.lower() == "right")
            bar->SetJustification(Qt::AlignRight);
        else if (align.lower() == "allcenter")
            bar->SetJustification(Qt::AlignHCenter | Qt::AlignVCenter);
        else if (align.lower() == "vcenter")
            bar->SetJustification(Qt::AlignVCenter);
        else if (align.lower() == "hcenter")
            bar->SetJustification(Qt::AlignHCenter);

    }
    align = "";

    if (context != -1)
    {
        bar->SetContext(context);
    }
    container->AddType(bar);
}



void XMLParse::parseMultiTextArea(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QRect area = QRect(0, 0, 0, 0);
    QRect altArea = QRect(0, 0, 0, 0);
    QPoint shadowOffset = QPoint(0, 0);
    QString font = "";
    QString cutdown = "";
    QString value = "";
    QString statictext = "";
    QString multiline = "";
    int padding = -1;
    int drop_delay = -1;
    int drop_pause = -1;
    int scroll_delay = -1;
    int scroll_pause = -1;
    int draworder = 0;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Multitext area needs a name\n";
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() && layerNum.isEmpty())
    {
        cerr << "Multitext area needs a draworder\n";
        return;
    }
    draworder = layerNum.toInt();

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "padding")
            {
                padding = getFirstText(info).toInt();
            }
            else if (info.tagName() == "dropdelay")
            {
                drop_delay = getFirstText(info).toInt();
            }
            else if (info.tagName() == "droppause")
            {
                drop_pause = getFirstText(info).toInt();
            }
            else if (info.tagName() == "scrolldelay")
            {
                scroll_delay = getFirstText(info).toInt();
            }
            else if (info.tagName() == "scrollpause")
            {
                scroll_pause = getFirstText(info).toInt();
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "altarea")
            {
                altArea = parseRect(getFirstText(info));
                normalizeRect(altArea);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "cutdown")
            {
                cutdown = getFirstText(info);
            }
            else if (info.tagName() == "shadow")
            {
                shadowOffset = parsePoint(getFirstText(info));
                shadowOffset.setX((int)(shadowOffset.x() * wmult));
                shadowOffset.setY((int)(shadowOffset.y() * hmult));
            }
            else
            {
                cerr << "Unknown tag in multitext area: "
                     << info.tagName()
                     << endl;
                return;
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in multitextarea: " << name << endl;
        return;
    }

    UIMultiTextType *multitext = new UIMultiTextType(name, testfont, draworder,
                                      area, altArea);
    multitext->SetScreen(wmult, hmult);
    if (context != -1)
    {
        multitext->SetContext(context);
    }

    if (padding > -1)
    {
        multitext->setMessageSpacePadding(padding);
    }
    if (drop_delay > -1)
    {
        multitext->setDropTimingLength(drop_delay);
    }
    if (drop_pause > -1)
    {
        multitext->setDropTimingPause(drop_pause);
    }
    if (scroll_delay > -1)
    {
        multitext->setScrollTimingLength(scroll_delay);
    }
    if (scroll_pause > -1)
    {
        multitext->setScrollTimingPause(scroll_pause);
    }

    multitext->SetParent(container);
    multitext->calculateScreenArea();
    container->AddType(multitext);
}

void XMLParse::parseListArea(LayerSet *container, QDomElement &element)
{
    int context = -1;
    int item_cnt = 0;
    QRect area = QRect(0, 0, 0, 0);
    QString force_color = "";
    QString act_font = "", in_font = "";
    QString statictext = "";
    int padding = 0;
    int draworder = 0;
    QMap<int, int> columnWidths;
    QMap<int, int> columnContexts;
    QMap<QString, QString> fontFunctions;
    QMap<QString, fontProp> theFonts;
    int colCnt = -1;

    QRect fill_select_area = QRect(0, 0, 0, 0);
    QColor fill_select_color = QColor(255, 255, 255);
    int fill_type = -1;

    QPoint uparrow_loc;
    QPoint dnarrow_loc;
    QPoint select_loc;
    QPoint rightarrow_loc;
    QPoint leftarrow_loc;

    QPixmap *uparrow_img = NULL;
    QPixmap *dnarrow_img = NULL;
    QPixmap *select_img = NULL;
    QPixmap *right_img = NULL;
    QPixmap *left_img = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "List area needs a name\n";
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() && layerNum.isEmpty())
    {
        cerr << "List area needs a draworder\n";
        return;
    }
    draworder = layerNum.toInt();

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "activefont")
            {
                act_font = getFirstText(info);
            }
            else if (info.tagName() == "inactivefont")
            {
                in_font = getFirstText(info);
            }
            else if (info.tagName() == "items")
            {
                item_cnt = getFirstText(info).toInt();
            }
            else if (info.tagName() == "columnpadding")
            {
                padding = getFirstText(info).toInt();
            }
            else if (info.tagName() == "fcnfont")
            {
                QString fontname = "";
                QString fontfcn = "";

                fontname = info.attribute("name", "");
                fontfcn = info.attribute("function", "");

                if (fontname.isNull() || fontname.isEmpty())
                {
                    cerr << "FcnFont needs a name\n";
                    return;
                }

                if (fontfcn.isNull() || fontfcn.isEmpty())
                {
                    cerr << "FcnFont needs a function\n";
                    return;
                }
                fontFunctions[fontfcn] = fontname;
            }
            else if (info.tagName() == "fill")
            {
                QString fillfcn = "";
                QString fillcolor = "";
                QString fillarea = "";
                QString filltype = "";

                fillfcn = info.attribute("function", "");
                fillcolor = info.attribute("color", "#ffffff");
                fillarea = info.attribute("area", "");
                filltype = info.attribute("type", "");

                if (fillfcn.isNull() || fillfcn.isEmpty())
                {
                    cerr << "Fill needs a function\n";
                    return;
                }
                if (fillcolor.isNull() || fillcolor.isEmpty())
                {
                    cerr << "Fill needs a color\n";
                    return;
                }
                if (filltype == "5050")
                    fill_type = 1;
                if (fill_type == -1)
                    fill_type = 1;

                if (fillarea.isNull() || fillarea.isEmpty())
                {
                    fill_select_area = area;
                }
                else
                {
                    fill_select_area = parseRect(fillarea);
                    normalizeRect(fill_select_area);
                }
                fill_select_color = QColor(fillcolor);

            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString imgpoint = "";
                QString imgfile = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image needs a function\n";
                    return;
                }

                imgfile = info.attribute("filename", "");
                if (imgfile.isNull() || imgfile.isEmpty())
                {
                    cerr << "Image needs a filename\n";
                    return;
                }



                imgpoint = info.attribute("location", "");
                if (imgpoint.isNull() && imgpoint.isEmpty())
                {
                    cerr << "Image needs a location\n";
                    return;
                }

                if (imgname.lower() == "selectionbar")
                {
                    select_img = gContext->LoadScalePixmap(imgfile);
                    select_loc = parsePoint(imgpoint);
                    select_loc.setX((int)(select_loc.x() * wmult));
                    select_loc.setY((int)(select_loc.y() * hmult));
                }
                if (imgname.lower() == "uparrow")
                {
                    uparrow_img = gContext->LoadScalePixmap(imgfile);
                    uparrow_loc = parsePoint(imgpoint);
                    uparrow_loc.setX((int)(uparrow_loc.x() * wmult));
                    uparrow_loc.setY((int)(uparrow_loc.y() * hmult));
                }
                if (imgname.lower() == "downarrow")
                {
                    dnarrow_img = gContext->LoadScalePixmap(imgfile);
                    dnarrow_loc = parsePoint(imgpoint);
                    dnarrow_loc.setX((int)(dnarrow_loc.x() * wmult));
                    dnarrow_loc.setY((int)(dnarrow_loc.y() * hmult));
                }
                if (imgname.lower() == "leftarrow")
                {
                    left_img = gContext->LoadScalePixmap(imgfile);
                    leftarrow_loc = parsePoint(imgpoint);
                    leftarrow_loc.setX((int)(leftarrow_loc.x() * wmult));
                    leftarrow_loc.setY((int)(leftarrow_loc.y() * hmult));

                }
                else if (imgname.lower() == "rightarrow")
                {
                    right_img = gContext->LoadScalePixmap(imgfile);
                    rightarrow_loc = parsePoint(imgpoint);
                    rightarrow_loc.setX((int)(rightarrow_loc.x() * wmult));
                    rightarrow_loc.setY((int)(rightarrow_loc.y() * hmult));
                }

            }
            else if (info.tagName() == "column")
            {
                QString colnum = "";
                QString colwidth = "";
                QString colcontext = "";
                colnum = info.attribute("number", "");
                if (colnum.isNull() || colnum.isEmpty())
                {
                    cerr << "Column needs a number\n";
                    return;
                }

                colwidth = info.attribute("width", "");
                if (colwidth.isNull() && colwidth.isEmpty())
                {
                    cerr << "Column needs a width\n";
                    return;
                }

                colcontext = info.attribute("context", "");
                if (!colcontext.isNull() && !colcontext.isEmpty())
                {
                    columnContexts[colnum.toInt()] = colcontext.toInt();
                }
                else
                    columnContexts[colnum.toInt()] = -1;

                columnWidths[colnum.toInt()] = (int)(wmult*colwidth.toInt());
                if (colnum.toInt() > colCnt)
                    colCnt = colnum.toInt();
            }
            else
            {
                cerr << "Unknown tag in listarea: " << info.tagName() << endl;
                return;
            }
        }
    }

    UIListType *list = new UIListType(name, area, draworder);
    list->SetCount(item_cnt);
    list->SetScreen(wmult, hmult);
    list->SetColumnPad(padding);
    if (select_img)
    {
        list->SetImageSelection(*select_img, select_loc);
        delete select_img;
    }

    if (uparrow_img)
    {
        list->SetImageUpArrow(*uparrow_img, uparrow_loc);
        delete uparrow_img;
    }

    if (dnarrow_img)
    {
        list->SetImageDownArrow(*dnarrow_img, dnarrow_loc);
        delete dnarrow_img;
    }

    if (right_img)
    {
        list->SetImageRightArrow(*right_img, rightarrow_loc);
        delete right_img;
    }

    if (left_img)
    {
        list->SetImageLeftArrow(*left_img, leftarrow_loc);
        delete left_img;
    }

    typedef QMap<QString,QString> fontdata;
    fontdata::Iterator it;
    for ( it = fontFunctions.begin(); it != fontFunctions.end(); ++it )
    {
        fontProp *testFont = GetFont(it.data());
        if (testFont)
            theFonts[it.data()] = *testFont;
    }
    if (theFonts.size() > 0)
        list->SetFonts(fontFunctions, theFonts);
    if (fill_type != -1)
        list->SetFill(fill_select_area, fill_select_color, fill_type);
    if (context != -1)
    {
        list->SetContext(context);
    }

    for (int i = 0; i <= colCnt; i++)
    {
         if (columnWidths[i] > 0)
            list->SetColumnWidth(i, columnWidths[i]);
        if (columnContexts[i] != -1)
            list->SetColumnContext(i, columnContexts[i]);
        else
            list->SetColumnContext(i, context);
    }

    if (colCnt == -1)
        list->SetColumnContext(1, -1);

    container->AddType(list);

}

void XMLParse::parseStatusBar(LayerSet *container, QDomElement &element)
{
    int context = -1;
    int orientation = 0;
    int imgFillSpace = 0;
    QPixmap *imgFiller = NULL;
    QPixmap *imgContainer = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "StatusBar needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "StatusBar needs an order\n";
        return;
    }

    QPoint pos = QPoint(0, 0);

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "container")
            {
                QString confile = getFirstText(info);
                QString flex = info.attribute("fleximage", "");
                if (!flex.isNull() && !flex.isEmpty())
                {
                    if (flex.lower() == "yes")
                    {
                        if (usetrans == 1)
                            confile = "trans-" + confile;
                        else
                            confile = "solid-" + confile;

                        imgContainer = gContext->LoadScalePixmap(confile);
                    }
                    else
                        imgContainer = gContext->LoadScalePixmap(confile);
                }
                else
                {
                    imgContainer = gContext->LoadScalePixmap(confile);
                }
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "fill")
            {
                QString fillfile = getFirstText(info);
                imgFillSpace = info.attribute("whitespace", "").toInt();

                QString flex = info.attribute("fleximage", "");
                if (!flex.isNull() && !flex.isEmpty())
                {
                    if (flex.lower() == "yes")
                    {
                        if (usetrans == 1)
                            fillfile = "trans-" + fillfile;
                        else
                            fillfile = "solid-" + fillfile;
                        imgFiller = gContext->LoadScalePixmap(fillfile);
                     }
                     else
                        imgFiller = gContext->LoadScalePixmap(fillfile);
                }
                else
                {
                    imgFiller = gContext->LoadScalePixmap(fillfile);
                }
            }
            else if (info.tagName() == "orientation")
            {
                QString orient_string = getFirstText(info).lower();
                if (orient_string == "lefttoright")
                {
                    orientation = 0;
                }
                if (orient_string == "righttoleft")
                {
                    orientation = 1;
                }
                if (orient_string == "bottomtotop")
                {
                    orientation = 2;
                }
                if (orient_string == "toptobottom")
                {
                    orientation = 3;
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in statusbar\n";
                return;
            }
        }
    }

    UIStatusBarType *sb = new UIStatusBarType(name, pos, order.toInt());
    sb->SetScreen(wmult, hmult);
    sb->SetFiller(imgFillSpace);
    if (imgContainer)
    {
        sb->SetContainerImage(*imgContainer);
        delete imgContainer;
    }
    if (imgFiller)
    {
        sb->SetFillerImage(*imgFiller);
        delete imgFiller;
    }
    if (context != -1)
    {
        sb->SetContext(context);
    }
    sb->SetParent(container);
    sb->calculateScreenArea();
    sb->setOrientation(orientation);
    container->AddType(sb);
    container->bumpUpLayers(order.toInt());
}

void XMLParse::parseManagedTreeList(LayerSet *container, QDomElement &element)
{
    QRect area;
    QRect binarea;
    int bins = 1;
    int context = -1;

    QPixmap *uparrow_img = NULL;
    QPixmap *downarrow_img = NULL;
    QPixmap *leftarrow_img = NULL;
    QPixmap *rightarrow_img = NULL;
    QPixmap *select_img = NULL;

    //
    //  A Map to store the geometry of
    //  the bins as we parse
    //

    typedef QMap<int, QRect> CornerMap;
    CornerMap bin_corners;
    bin_corners.clear();

    //
    //  Some maps to store fonts as we parse
    //

    QMap<QString, QString> fontFunctions;
    QMap<QString, fontProp> theFonts;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "ManagedTreeList needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "ManagedTreeList needs an order\n";
        return;
    }

    QString bins_string = element.attribute("bins", "");
    if (bins_string.toInt() > 0)
    {
        bins = bins_string.toInt();
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image needs a function\n";
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    cerr << "Image needs a filename\n";
                    return;
                }

                if (info.tagName() == "context")
                {
                    context = getFirstText(info).toInt();
                }

                if (imgname.lower() == "selectionbar")
                {
                    select_img = gContext->LoadScalePixmap(file);
                    if (!select_img)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else if (imgname.lower() == "uparrow")
                {
                    uparrow_img = gContext->LoadScalePixmap(file);
                    if (!uparrow_img)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else if (imgname.lower() == "downarrow")
                {
                    downarrow_img = gContext->LoadScalePixmap(file);
                    if (!downarrow_img)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else if (imgname.lower() == "leftarrow")
                {
                    leftarrow_img = gContext->LoadScalePixmap(file);
                    if (!leftarrow_img)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else if (imgname.lower() == "rightarrow")
                {
                    rightarrow_img = gContext->LoadScalePixmap(file);
                    if (!rightarrow_img)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                else
                {
                    cerr << "xmlparse.o: I don't know what to do with an image tag who's function is " << imgname << endl;
                }
            }
            else if (info.tagName() == "bin")
            {
                QString whichbin_string = info.attribute("number", "");
                int whichbin = whichbin_string.toInt();
                if (whichbin < 1)
                {
                    cerr << "xmlparse.o: Bad setting for bin number in bin tag" << endl;
                    return;
                }
                if (whichbin > bins + 1)
                {
                    cerr << "xmlparse.o: Attempt to set bin with a reference larger than number of bins" << endl;
                    return;
                }
                for (QDomNode child = info.firstChild(); !child.isNull();
                child = child.nextSibling())
                {
                    QDomElement info = child.toElement();
                    if (!info.isNull())
                    {
                        if (info.tagName() == "area")
                        {
                            binarea = parseRect(getFirstText(info));
                            normalizeRect(binarea);
                            bin_corners[whichbin] = binarea;
                        }
                        else if (info.tagName() == "fcnfont")
                        {
                            QString fontname = "";
                            QString fontfcn = "";

                            fontname = info.attribute("name", "");
                            fontfcn = info.attribute("function", "");

                            if (fontname.isNull() || fontname.isEmpty())
                            {
                                cerr << "FcnFont needs a name\n";
                                return;
                            }

                            if (fontfcn.isNull() || fontfcn.isEmpty())
                            {
                                cerr << "FcnFont needs a function\n";
                                return;
                            }
                            QString a_string = QString("bin%1-%2").arg(whichbin).arg(fontfcn);
                            fontFunctions[a_string] = fontname;
                        }
                        else
                        {
                            cerr << "Unknown tag in bin: " << info.tagName() << endl;
                            return;
                        }
                    }
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in ManagedTreeList\n";
                return;
            }
        }
    }


    UIManagedTreeListType *mtl = new UIManagedTreeListType(name);
    mtl->SetScreen(wmult, hmult);
    mtl->setArea(area);
    mtl->setBins(bins);
    mtl->setBinAreas(bin_corners);
    mtl->SetOrder(order.toInt());
    mtl->SetParent(container);
    mtl->SetContext(context);

    //
    //  Perform moegreen/jdanner font magic
    //

    typedef QMap<QString,QString> fontdata;
    fontdata::Iterator it;
    for ( it = fontFunctions.begin(); it != fontFunctions.end(); ++it )
    {
        fontProp *testFont = GetFont(it.data());
        if (testFont)
            theFonts[it.data()] = *testFont;
    }

    if (theFonts.size() > 0)
        mtl->setFonts(fontFunctions, theFonts);

    if (select_img)
    {
        mtl->setHighlightImage(*select_img);
        delete select_img;
    }

    if (!uparrow_img)
        uparrow_img = new QPixmap();
    if (!downarrow_img)
        downarrow_img = new QPixmap();
    if (!leftarrow_img)
        leftarrow_img = new QPixmap();
    if (!rightarrow_img)
        rightarrow_img = new QPixmap();

    mtl->setArrowImages(*uparrow_img, *downarrow_img, *leftarrow_img,
                        *rightarrow_img);

    delete uparrow_img;
    delete downarrow_img;
    delete leftarrow_img;
    delete rightarrow_img;

    mtl->makeHighlights();
    mtl->calculateScreenArea();
    container->AddType(mtl);
}

void XMLParse::parsePushButton(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QPoint pos = QPoint(0, 0);
    QPixmap *image_on = NULL;
    QPixmap *image_off = NULL;
    QPixmap *image_pushed = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "PushButton needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "PushButton needs an order\n";
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image in a push button needs a function\n";
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    cerr << "Image in a push button needs a filename\n";
                    return;
                }

                if (imgname.lower() == "on")
                {
                    image_on = gContext->LoadScalePixmap(file);
                    if (!image_on)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "off")
                {
                    image_off = gContext->LoadScalePixmap(file);
                    if (!image_off)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "pushed")
                {
                    image_pushed = gContext->LoadScalePixmap(file);
                    if (!image_pushed)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in PushButton\n";
                return;
            }
        }
    }

    if (!image_on)
        image_on = new QPixmap();
    if (!image_off)
        image_off = new QPixmap();
    if (!image_pushed)
        image_pushed = new QPixmap();

    UIPushButtonType *pbt = new UIPushButtonType(name, *image_on, *image_off,
                                                 *image_pushed);

    delete image_on;
    delete image_off;
    delete image_pushed;

    pbt->SetScreen(wmult, hmult);
    pbt->setPosition(pos);
    pbt->SetOrder(order.toInt());
    if (context != -1)
    {
        pbt->SetContext(context);
    }
    pbt->SetParent(container);
    pbt->calculateScreenArea();
    container->AddType(pbt);
}

void XMLParse::parseTextButton(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString font = "";
    QPoint pos = QPoint(0, 0);
    QPixmap *image_on = NULL;
    QPixmap *image_off = NULL;
    QPixmap *image_pushed = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "TextButton needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "TextButton needs an order\n";
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image in a text button needs a function\n";
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    cerr << "Image in a text button needs a filename\n";
                    return;
                }

                if (imgname.lower() == "on")
                {
                    image_on = gContext->LoadScalePixmap(file);
                    if (!image_on)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "off")
                {
                    image_off = gContext->LoadScalePixmap(file);
                    if (!image_off)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "pushed")
                {
                    image_pushed = gContext->LoadScalePixmap(file);

                    if (!image_pushed)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in TextButton\n";
                return;
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in textbutton: " << name << endl;
        return;
    }

    if (!image_on)
        image_on = new QPixmap();
    if (!image_off)
        image_off = new QPixmap();
    if (!image_pushed)
        image_pushed = new QPixmap();

    UITextButtonType *tbt = new UITextButtonType(name, *image_on, *image_off,
                                                 *image_pushed);

    delete image_on;
    delete image_off;
    delete image_pushed;

    tbt->SetScreen(wmult, hmult);
    tbt->setPosition(pos);
    tbt->setFont(testfont);
    tbt->SetOrder(order.toInt());
    if (context != -1)
    {
        tbt->SetContext(context);
    }
    tbt->SetParent(container);
    tbt->calculateScreenArea();
    container->AddType(tbt);
}

void XMLParse::parseCheckBox(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QPoint pos = QPoint(0, 0);
    QPixmap *image_checked = NULL;
    QPixmap *image_unchecked = NULL;
    QPixmap *image_checked_high = NULL;
    QPixmap *image_unchecked_high = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "CheckBox needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "CheckBox needs an order\n";
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "position")
            {
                pos = parsePoint(getFirstText(info));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image in a CheckBox needs a function\n";
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    cerr << "Image in a CheckBox needs a filename\n";
                    return;
                }

                if (imgname.lower() == "checked")
                {
                    image_checked = gContext->LoadScalePixmap(file);
                    if (!image_checked)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "unchecked")
                {
                    image_unchecked = gContext->LoadScalePixmap(file);
                    if (!image_unchecked)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "checked_high")
                {
                    image_checked_high = gContext->LoadScalePixmap(file);
                    if (!image_checked_high)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "unchecked_high")
                {
                    image_unchecked_high = gContext->LoadScalePixmap(file);
                    if (!image_unchecked_high)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in CheckBox\n";
                return;
            }
        }
    }

    if (!image_checked)
        image_checked = new QPixmap();
    if (!image_unchecked)
        image_unchecked = new QPixmap();
    if (!image_checked_high)
        image_checked_high = new QPixmap();
    if (!image_unchecked_high)
        image_unchecked_high = new QPixmap();

    UICheckBoxType *cbt = new UICheckBoxType(name,
                                             *image_checked, *image_unchecked,
                                             *image_checked_high, *image_unchecked_high);

    delete image_checked;
    delete image_unchecked;
    delete image_checked_high;
    delete image_unchecked_high;

    cbt->SetScreen(wmult, hmult);
    cbt->setPosition(pos);
    cbt->SetOrder(order.toInt());
    if (context != -1)
    {
        cbt->SetContext(context);
    }
    cbt->SetParent(container);
    cbt->calculateScreenArea();
    container->AddType(cbt);
}

void XMLParse::parseSelector(LayerSet *container, QDomElement &element)
{
    int context = -1;
    QString font = "";
    QRect area;
    QPixmap *image_on = NULL;
    QPixmap *image_off = NULL;
    QPixmap *image_pushed = NULL;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "Selector needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "Selector needs an order\n";
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "context")
            {
                context = getFirstText(info).toInt();
            }
            else if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "font")
            {
                font = getFirstText(info);
            }
            else if (info.tagName() == "image")
            {
                QString imgname = "";
                QString file = "";

                imgname = info.attribute("function", "");
                if (imgname.isNull() || imgname.isEmpty())
                {
                    cerr << "Image in a selector needs a function\n";
                    return;
                }

                file = info.attribute("filename", "");
                if (file.isNull() || file.isEmpty())
                {
                    cerr << "Image in a selector needs a filename\n";
                    return;
                }

                if (imgname.lower() == "on")
                {
                    image_on = gContext->LoadScalePixmap(file);
                    if (!image_on)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "off")
                {
                    image_off = gContext->LoadScalePixmap(file);
                    if (!image_off)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
                if (imgname.lower() == "pushed")
                {
                    image_pushed = gContext->LoadScalePixmap(file);

                    if (!image_pushed)
                    {
                        cerr << "xmparse.o: I can't find a file called " << file << endl ;
                    }
                }
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in Selector\n";
                return;
            }
        }
    }

    fontProp *testfont = GetFont(font);
    if (!testfont)
    {
        cerr << "Unknown font: " << font << " in Selector: " << name << endl;
        return;
    }

    if (!image_on)
        image_on = new QPixmap();
    if (!image_off)
        image_off = new QPixmap();
    if (!image_pushed)
        image_pushed = new QPixmap();

    UISelectorType *st = new UISelectorType(name, *image_on, *image_off,
                                                 *image_pushed, area);

    delete image_on;
    delete image_off;
    delete image_pushed;

    st->SetScreen(wmult, hmult);
    st->setPosition(area.topLeft());
    st->setFont(testfont);
    st->SetOrder(order.toInt());
    if (context != -1)
    {
        st->SetContext(context);
    }
    st->SetParent(container);
    st->calculateScreenArea();
    container->AddType(st);
}


void XMLParse::parseBlackHole(LayerSet *container, QDomElement &element)
{
    QRect area;

    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "BlackHole needs a name\n";
        return;
    }

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else
            {
                cerr << "Unknown: " << info.tagName() << " in Black Hole\n";
                return;
            }
        }
    }


    UIBlackHoleType *bh = new UIBlackHoleType(name);
    bh->SetScreen(wmult, hmult);
    bh->setArea(area);
    bh->SetParent(container);
    bh->calculateScreenArea();
    container->AddType(bh);
}

void XMLParse::parseListBtnArea(LayerSet *container, QDomElement &element)
{
    QRect   area = QRect(0,0,0,0);
    QString fontActive;
    QString fontInactive;
    bool    showArrow = true;
    bool    showScrollArrows = false;
    int     draworder = 0;
    QColor  grUnselectedBeg(Qt::black);
    QColor  grUnselectedEnd(80,80,80);
    uint    grUnselectedAlpha(100);
    QColor  grSelectedBeg(82,202,56);
    QColor  grSelectedEnd(52,152,56);
    uint    grSelectedAlpha(255);
    int     spacing = 2;
    int     margin = 3;

    QString name = element.attribute("name", "");
    if (name.isEmpty()) {
        std::cerr << "ListBtn area needs a name" << std::endl;
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() || layerNum.isEmpty())
    {
        cerr << "ListBtn area needs a draworder\n";
        return;
    }

    draworder = layerNum.toInt();
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling()) {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "fcnfont")
            {
                QString fontName = info.attribute("name", "");
                QString fontFcn  = info.attribute("function", "");

                if (fontFcn.lower() == "active")
                    fontActive = fontName;
                else if (fontFcn.lower() == "inactive")
                    fontInactive = fontFcn;
                else {
                    std::cerr << "Unknown font function for listbtn area: "
                              << fontFcn
                              << std::endl;
                    return;
                }
            }
            else if (info.tagName() == "showarrow") {
                if (getFirstText(info).lower() == "no")
                    showArrow = false;
            }
            else if (info.tagName() == "showscrollarrows") {
                if (getFirstText(info).lower() == "yes")
                    showScrollArrows = true;
            }
            else if (info.tagName() == "gradient") {

                if (info.attribute("type","").lower() == "selected") {
                    grSelectedBeg = QColor(info.attribute("start"));
                    grSelectedEnd = QColor(info.attribute("end"));
                    grSelectedAlpha = info.attribute("alpha","255").toUInt();
                }
                else if (info.attribute("type","").lower() == "unselected") {
                    grUnselectedBeg = QColor(info.attribute("start"));
                    grUnselectedEnd = QColor(info.attribute("end"));
                    grUnselectedAlpha = info.attribute("alpha","100").toUInt();
                }
                else {
                    std::cerr << "Unknown type for gradient in listbtn area"
                              << std::endl;
                    return;
                }

                if (!grSelectedBeg.isValid() || !grSelectedEnd.isValid() ||
                    !grUnselectedBeg.isValid() || !grUnselectedEnd.isValid()) {
                    std::cerr << "Unknown color for gradient in listbtn area"
                              << std::endl;
                    return;
                }

                if (grSelectedAlpha > 255 || grUnselectedAlpha > 255) {
                    std::cerr << "Incorrect alpha for gradient in listbtn area"
                              << std::endl;
                    return;
                }
            }
            else if (info.tagName() == "spacing") {
                spacing = getFirstText(info).toInt();
            }
            else if (info.tagName() == "margin") {
                margin = getFirstText(info).toInt();
            }
            else
            {
                std::cerr << "Unknown tag in listbtn area: "
                          << info.tagName() << endl;
                return;
            }

        }
    }

    fontProp *fpActive = GetFont(fontActive);
    if (!fpActive)
    {
        cerr << "Unknown font: " << fontActive
             << " in listbtn area: " << name << endl;
        return;
    }

    fontProp *fpInactive = GetFont(fontInactive);
    if (!fpInactive)
    {
        cerr << "Unknown font: " << fontInactive
             << " in listbtn area: " << name << endl;
        return;
    }


    UIListBtnType *l = new UIListBtnType(name, area, draworder, showArrow,
                                         showScrollArrows);
    l->SetScreen(wmult, hmult);
    l->SetFontActive(fpActive);
    l->SetFontInactive(fpInactive);
    l->SetItemRegColor(grUnselectedBeg, grUnselectedEnd, grUnselectedAlpha);
    l->SetItemSelColor(grSelectedBeg, grSelectedEnd, grSelectedAlpha);
    l->SetSpacing((int)(spacing*hmult));
    l->SetMargin((int)(margin*wmult));
    l->SetParent(container);

    container->AddType(l);
    container->bumpUpLayers(0);
}

void XMLParse::parseListTreeArea(LayerSet *container, QDomElement &element)
{
    QRect   area = QRect(0,0,0,0);
    QRect   listsize = QRect(0,0,0,0);
    int     leveloffset = 0;
    QString fontActive;
    QString fontInactive;
    bool    showArrow = true;
    bool    showScrollArrows = false;
    int     draworder = 0;
    QColor  grUnselectedBeg(Qt::black);
    QColor  grUnselectedEnd(80,80,80);
    uint    grUnselectedAlpha(100);
    QColor  grSelectedBeg(82,202,56);
    QColor  grSelectedEnd(52,152,56);
    uint    grSelectedAlpha(255);
    int     spacing = 2;
    int     margin = 3;

    QString name = element.attribute("name", "");
    if (name.isEmpty()) {
        std::cerr << "ListBtn area needs a name" << std::endl;
        return;
    }

    QString layerNum = element.attribute("draworder", "");
    if (layerNum.isNull() || layerNum.isEmpty())
    {
        cerr << "ListBtn area needs a draworder\n";
        return;
    }

    draworder = layerNum.toInt();
    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling()) {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "area")
            {
                area = parseRect(getFirstText(info));
                normalizeRect(area);
            }
            else if (info.tagName() == "listsize")
            {
                listsize = parseRect(getFirstText(info));
                normalizeRect(listsize);
            }
            else if (info.tagName() == "leveloffset")
            {
                leveloffset = getFirstText(info).toInt();
            }
            else if (info.tagName() == "fcnfont")
            {
                QString fontName = info.attribute("name", "");
                QString fontFcn  = info.attribute("function", "");

                if (fontFcn.lower() == "active")
                    fontActive = fontName;
                else if (fontFcn.lower() == "inactive")
                    fontInactive = fontFcn;
                else {
                    std::cerr << "Unknown font function for listbtn area: "
                              << fontFcn
                              << std::endl;
                    return;
                }
            }
            else if (info.tagName() == "showarrow") {
                if (getFirstText(info).lower() == "no")
                    showArrow = false;
            }
            else if (info.tagName() == "showscrollarrows") {
                if (getFirstText(info).lower() == "yes")
                    showScrollArrows = true;
            }
            else if (info.tagName() == "gradient") {

                if (info.attribute("type","").lower() == "selected") {
                    grSelectedBeg = QColor(info.attribute("start"));
                    grSelectedEnd = QColor(info.attribute("end"));
                    grSelectedAlpha = info.attribute("alpha","255").toUInt();
                }
                else if (info.attribute("type","").lower() == "unselected") {
                    grUnselectedBeg = QColor(info.attribute("start"));
                    grUnselectedEnd = QColor(info.attribute("end"));
                    grUnselectedAlpha = info.attribute("alpha","100").toUInt();
                }
                else {
                    std::cerr << "Unknown type for gradient in listbtn area"
                              << std::endl;
                    return;
                }

                if (!grSelectedBeg.isValid() || !grSelectedEnd.isValid() ||
                    !grUnselectedBeg.isValid() || !grUnselectedEnd.isValid()) {
                    std::cerr << "Unknown color for gradient in listbtn area"
                              << std::endl;
                    return;
                }

                if (grSelectedAlpha > 255 || grUnselectedAlpha > 255) {
                    std::cerr << "Incorrect alpha for gradient in listbtn area"
                              << std::endl;
                    return;
                }
            }
            else if (info.tagName() == "spacing") {
                spacing = getFirstText(info).toInt();
            }
            else if (info.tagName() == "margin") {
                margin = getFirstText(info).toInt();
            }
            else
            {
                std::cerr << "Unknown tag in listbtn area: "
                          << info.tagName() << endl;
                return;
            }

        }
    }

    fontProp *fpActive = GetFont(fontActive);
    if (!fpActive)
    {
        cerr << "Unknown font: " << fontActive
             << " in listbtn area: " << name << endl;
        return;
    }

    fontProp *fpInactive = GetFont(fontInactive);
    if (!fpInactive)
    {
        cerr << "Unknown font: " << fontInactive
             << " in listbtn area: " << name << endl;
        return;
    }


    UIListTreeType *l = new UIListTreeType(name, area, listsize, leveloffset,
                                           draworder);

    l->SetScreen(wmult, hmult);
    l->SetFontActive(fpActive);
    l->SetFontInactive(fpInactive);
    l->SetItemRegColor(grUnselectedBeg, grUnselectedEnd, grUnselectedAlpha);
    l->SetItemSelColor(grSelectedBeg, grSelectedEnd, grSelectedAlpha);
    l->SetSpacing((int)(spacing*hmult));
    l->SetMargin((int)(margin*wmult));
    l->SetParent(container);
    l->calculateScreenArea();

    container->AddType(l);
    container->bumpUpLayers(0);
}

void XMLParse::parseKey(LayerSet *container, QDomElement &element)
{
    QString name, action, order, notclicked;
    QString clicked, chars, font, subtitle;
    QPoint pos;

    notclicked = clicked = subtitle = chars = font = "";
    pos = QPoint(0, 0);

    name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "key needs a name\n";
        return;
    }

    action = element.attribute("action", "");
    if (action.isNull() || action.isEmpty())
    {
        cerr << "key needs an action\n";
        return;
    }

    order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "keyboard needs an order\n";
        return;
    }

    LayerSet *c = new LayerSet("_internal");

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "notclickedimg")
            {
                notclicked = getFirstText(e);
            }
            else if (e.tagName() == "clickedimg")
            {
                clicked = getFirstText(e);
            }
            else if (e.tagName() == "subtitleimg")
            {
                subtitle = getFirstText(e);
            }
            else if (e.tagName() == "position")
            {
                pos = parsePoint(getFirstText(e));
                pos.setX((int)(pos.x() * wmult));
                pos.setY((int)(pos.y() * hmult));

            }
            else if (e.tagName() == "chars")
            {
                chars = getFirstText(e);
            }
            else if (e.tagName() == "textarea")
            {
                parseTextArea(c, e);
            }
            else if (e.tagName() == "font")
            {
                font = getFirstText(e);
            }
            else
            {
                cerr << "Unknown: " << e.tagName() << " in key\n";
                return;
            }
        }
    }

    vector<UIType *>::iterator i = c->getAllTypes()->begin();
    for (; i != c->getAllTypes()->end(); i++)
    {
        UIType *type = (*i);
        if (UITextType *text = dynamic_cast<UITextType*>(type))
        {
            QRect rect = text->DisplayArea();
            rect.moveTopLeft(rect.topLeft() + pos);
            text->SetDisplayArea(rect);
        }
    }

    UIKeyType *key = new UIKeyType(name);
    key->SetContainer(c);
    key->SetOrder(order.toInt());
    key->SetAction(action);
    key->SetClicked(clicked);
    key->SetNotClicked(notclicked);
    key->SetSubtitle(subtitle);
    key->SetChars(chars);
    key->SetFont(font);
    key->SetPosition(pos);
    container->AddType(key);
}

void XMLParse::parseKeyboard(LayerSet *container, QDomElement &element)
{
    QString name = element.attribute("name", "");
    if (name.isNull() || name.isEmpty())
    {
        cerr << "keyboard needs a name\n";
        return;
    }

    QString order = element.attribute("draworder", "");
    if (order.isNull() || order.isEmpty())
    {
        cerr << "keyboard needs an order\n";
        return;
    }

    LayerSet *c = new LayerSet("_internal");

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "image")
            {
                parseImage(c, e);
            }
            else if (e.tagName() == "key")
            {
                parseKey(c, e);
            }
            else
            {
                cerr << "Unknown: " << e.tagName() << " in keyboard\n";
                return;
            }
        }
    }

    UIKeyboardType *kbd = new UIKeyboardType(name, order.toInt());
    kbd->SetContainer(c);
    container->AddType(kbd);

    vector<UIType *>::iterator i = c->getAllTypes()->begin();
    for (; i != c->getAllTypes()->end(); i++)
    {
        UIType *type = (*i);
        if (UIKeyType *keyt = dynamic_cast<UIKeyType*>(type))
        {
            kbd->AddKey(keyt->GetAction(), keyt);
        }
    }
}
*/
