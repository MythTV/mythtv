#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qapplication.h>

#include "dialogbox.h"
#include "mythcontext.h"
#include "mythwidgets.h"

DialogBox::DialogBox(const QString &text, const char *checkboxtext,
                     QWidget *parent, const char *name)
         : MythDialog(parent, name)
{
    QLabel *maintext = new QLabel(text, this);
    maintext->setBackgroundOrigin(WindowOrigin);
    maintext->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);

    box = new QVBoxLayout(this, (int)(20 * wmult));

    box->addWidget(maintext, 1);

    checkbox = NULL;
    if (checkboxtext)
    {
        checkbox = new MythCheckBox(this);
        checkbox->setText(checkboxtext); 
        checkbox->setBackgroundOrigin(WindowOrigin);
        box->addWidget(checkbox, 0);
    }

    buttongroup = new QButtonGroup(0);
  
    if (checkbox)
        buttongroup->insert(checkbox);
    connect(buttongroup, SIGNAL(clicked(int)), this, SLOT(buttonPressed(int)));
}

void DialogBox::AddButton(const QString &title)
{
    MythPushButton *button = new MythPushButton(title, this);
    buttongroup->insert(button);

    box->addWidget(button, 0);
}

void DialogBox::buttonPressed(int which)
{
    if (buttongroup->find(which) == checkbox)
    {
    }
    else
        done(which+1);
}

/*
==========================================================================
*/

MythThemedDialog::MythThemedDialog(QString window_name,
                                   QString theme_filename,
                                   QWidget *parent,
                                   const char* name)
                : MythDialog(parent, name)
{
    context = 0;
    my_containers.clear();
    
    //
    //  Load the theme. Crap out if we can't find it.
    //
    
    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);
    if(!theme->LoadTheme(xmldata, window_name, theme_filename))
    {
        cerr << "dialogbox.o: Couldn't find your theme. I'm outta here" << endl;
        exit(0);
    }
    loadWindow(xmldata);
    updateBackground();
}

void MythThemedDialog::loadWindow(QDomElement &element)
{
    //
    //  Parse all the child elements in the theme
    //

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement e = child.toElement();
        if (!e.isNull())
        {
            if (e.tagName() == "font")
            {
                theme->parseFont(e);
            }
            else if (e.tagName() == "container")
            {
                parseContainer(e);
            }
            else if (e.tagName() == "popup")
            {
                parsePopup(e);
            }
            else
            {
                cerr << "dialogbox.o: I don't understand this DOM Element:" << e.tagName() << endl;
                exit(0);
            }
        }
    }

}

void MythThemedDialog::parseContainer(QDomElement &element)
{
    //
    //  Have the them object parse the containers
    //  but hold a pointer to each of them so 
    //  that we can iterate over them later
    //

    QRect area;
    QString name;
    int context;
    theme->parseContainer(element, name, context, area);
    if(name.length() < 1)
    {
        cerr << "dialogbox.o: I told a them object to parse a container and it didn't give me a name back" << endl;
        exit(0);
    }

    LayerSet *container_reference = theme->GetSet(name);
    my_containers.append(container_reference);
}

void MythThemedDialog::parseFont(QDomElement &element)
{
    //
    //  this is just here so you can re-implement the virtual    
    //  function and do something special if you like
    //

    theme->parseFont(element);
}

void MythThemedDialog::parsePopup(QDomElement &element)
{
    //
    //  theme doesn't know how to do this yet
    //
    element = element;
    cerr << "I don't know how to handle popops yet (I'm going to try and just ignore it)" << endl;
}

void MythThemedDialog::updateBackground()
{
    //
    //  Draw the background pixmap once
    // 

    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    //
    //  Ask the container holding anything
    //  to do with the background to draw
    //  itself on a pixmap
    //

    LayerSet *container = theme->GetSet("background");
    container->Draw(&tmp, 0, context);
    tmp.end();

    //
    //  Copy that pixmap to the permanent one
    //  and tell Qt about it
    //

    my_background = bground;
    setPaletteBackgroundPixmap(my_background);
}

void MythThemedDialog::paintEvent(QPaintEvent *e)
{
    //
    //  repaint whatever Qt tells us needs to 
    //  be painted over again
    //

    QRect r = e->rect();
    QPainter p(this);
    
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        QRect   container_area = looper->GetAreaRect();

        //
        //  Only paint if the container's area is valid
        //  and it intersects with whatever Qt told us
        //  needed to be repainted
        //
        
        if(container_area.isValid() && r.intersects(container_area))
        {
            QPixmap container_picture(container_area.size());
            container_picture.fill(this, container_area.topLeft());
            QPainter offscreen_painter(&container_picture);

            // 
            //  is 9 layers hardcoded somewhere?
            //  the container should probably be keeping
            //  track of how many layers it has on it's own
            //
            
            for(int i = 0; i < 9; i++)
            {
                looper->Draw(&offscreen_painter, i, context);
            }

            //  
            //  If it did in fact paint something (empty
            //  container?) then tell it we're done
            //
            if(offscreen_painter.isActive())
            {
                offscreen_painter.end();
            }
            
            //
            //  Ok, the pixmap is done, paint it onto
            //  the actual window (ie. "this")
            //

            p.drawPixmap(container_area.topLeft(), container_picture);
        }
        ++an_it;
    }
                    

    //
    //  Whatever else may be on this widget
    //  should get painted as well
    //
    MythDialog::paintEvent(e);
}

UIType* MythThemedDialog::getUIObject(QString name)
{

    UIType* oops = NULL;
    
    //
    //  Try and find the UIType target referenced
    //  by "name".
    //
    //  At some point, it would be nice to speed
    //  this up with a map across all instantiated
    //  UIType objects "owned" by this dialog
    //
    
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            return hunter;
        }
        ++an_it;
    }

    return oops;
}
