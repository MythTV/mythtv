#include <iostream>
using namespace std;

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
                : MythDialog(parent, name, Qt::WRepaintNoErase)
{
    context = 0;
    my_containers.clear();
    widget_with_current_focus = NULL;
    focus_taking_widgets.clear();
    
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

    //
    //  Auto-connect signals we know about
    //
    

    //  Loop over containers
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;
    while( (looper = an_it.current()) != 0)
    {
        //  Loop over UITypes within each container
        vector<UIType *> *all_ui_type_objects = looper->getAllTypes();
        vector<UIType *>::iterator i = all_ui_type_objects->begin();
        for (; i != all_ui_type_objects->end(); i++)
        {
            UIType *type = (*i);
            connect(type, SIGNAL(requestUpdate()), this, SLOT(updateForeground()));            
            connect(type, SIGNAL(requestUpdate(const QRect &)), this, SLOT(updateForeground(const QRect &)));
        }
        ++an_it;
    }
    
    //
    //  Build a list of widgets that will take focus
    //
    

    //  Loop over containers
    QPtrListIterator<LayerSet> another_it(my_containers);
    while( (looper = another_it.current()) != 0)
    {
        //  Loop over UITypes within each container
        vector<UIType *> *all_ui_type_objects = looper->getAllTypes();
        vector<UIType *>::iterator i = all_ui_type_objects->begin();
        for (; i != all_ui_type_objects->end(); i++)
        {
            UIType *type = (*i);
            if(type->canTakeFocus())
            {
                focus_taking_widgets.append(type);
            }
        }
        ++another_it;
    }
    

    updateBackground();
    initForeground();
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


void MythThemedDialog::initForeground()
{
    my_foreground = my_background;
    updateForeground();
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

void MythThemedDialog::updateForeground()
{
    QRect r = this->geometry();
    updateForeground(r);
}

void MythThemedDialog::updateForeground(const QRect &r)
{

    
    //
    //  Debugging
    //

    /*
    cout << "I am updating the foreground from " 
         << r.left()
         << ","
         << r.top()
         << " to "
         << r.left() + r.width()
         << ","
         << r.top() + r.height()
         << endl;
    */
    
    //
    //  We paint offscreen onto a pixmap
    //  and then BitBlt it over
    //

//    my_foreground = my_background;
//    my_foreground.fill(this, 0, 0);
//    my_foreground = my_background;
    QPainter whole_dialog_painter(&my_foreground);
    
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
        
        if(container_area.isValid() && 
           r.intersects(container_area) &&
           looper->GetName().lower() != "background")
        {
            //
            //  Debugging
            //
            /*
            cout << "A container called \"" << looper->GetName() << "\" said its area is " 
                 << container_area.left()
                 << ","
                 << container_area.top()
                 << " to "
                 << container_area.left() + container_area.width()
                 << ","
                 << container_area.top() + container_area.height()
                 << endl;
            */
            
            QPixmap container_picture(container_area.size());
            QPainter offscreen_painter(&container_picture);
            offscreen_painter.drawPixmap(0, 0, my_background, container_area.left(), container_area.top());


            // 
            //  is 9 layers hardcoded somewhere?
            //  the container should probably be keeping
            //  track of how many layers it has on it's own
            //
            
            for(int i = 0; i < 9; i++)
            {
                looper->Draw(&offscreen_painter, i, -1);
            }

            //  
            //  If it did in fact paint something (empty
            //  container?) then tell it we're done
            //
            if(offscreen_painter.isActive())
            {
                offscreen_painter.end();
                whole_dialog_painter.drawPixmap(container_area.topLeft(), container_picture);
            }
            
        }
        ++an_it;
    }

    if(whole_dialog_painter.isActive())
    {
        whole_dialog_painter.end();
    }
    update(r);
}

void MythThemedDialog::paintEvent(QPaintEvent *e)
{
    MythDialog::paintEvent(e);

    bitBlt( this, 
            e->rect().left(), e->rect().top(), 
            &my_foreground,
            e->rect().left(), e->rect().top(), 
            e->rect().width(), e->rect().height()
            );
}


bool MythThemedDialog::assignFirstFocus()
{
    if(widget_with_current_focus)
    {
        widget_with_current_focus->looseFocus();
    }
    if(focus_taking_widgets.count() > 0)
    {
        widget_with_current_focus = focus_taking_widgets.first();
        widget_with_current_focus->takeFocus();
        return true;
    }
    return false;
}

bool MythThemedDialog::nextPrevWidgetFocus(bool up_or_down)
{
    if(up_or_down)
    {
        bool reached_current = false;
        QPtrListIterator<UIType> an_it(focus_taking_widgets);
        UIType *looper;

        while( (looper = an_it.current()) != 0)
        {
            if(reached_current)
            {
                widget_with_current_focus->looseFocus();
                widget_with_current_focus = looper;
                widget_with_current_focus->takeFocus();
                return true;    
            }
            if(looper == widget_with_current_focus)
            {
                reached_current= true;
            }
            ++an_it;
        }
    
        if(assignFirstFocus())
        {
            return true;
        }
        return false;
    }
    else
    {
        bool reached_current = false;
        QPtrListIterator<UIType> an_it(focus_taking_widgets);
        an_it.toLast();
        UIType *looper;

        while( (looper = an_it.current()) != 0)
        {
            if(reached_current)
            {
                widget_with_current_focus->looseFocus();
                widget_with_current_focus = looper;
                widget_with_current_focus->takeFocus();
                return true;    
            }
            if(looper == widget_with_current_focus)
            {
                reached_current= true;
            }
            --an_it;
        }
        
        if(reached_current)
        {
            widget_with_current_focus->looseFocus();
            widget_with_current_focus = focus_taking_widgets.last();
            widget_with_current_focus->takeFocus();
            return true;
        }
        return false;
    }
    return false;
}

void MythThemedDialog::activateCurrent()
{
    if(widget_with_current_focus)
    {
        widget_with_current_focus->activate();
    }
    else
    {
        cerr << "dialogbox.o: Something asked me activate the current widget, but there is no current widget" << endl;
    }
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

UIManagedTreeListType* MythThemedDialog::getUIManagedTreeListType(QString name)
{

    UIManagedTreeListType* oops = NULL;
    
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UIManagedTreeListType *hunted;
            if( (hunted = dynamic_cast<UIManagedTreeListType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }

    return oops;
}

UITextType* MythThemedDialog::getUITextType(QString name)
{

    UITextType* oops = NULL;
    
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UITextType *hunted;
            if( (hunted = dynamic_cast<UITextType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }

    return oops;
}

UIPushButtonType* MythThemedDialog::getUIPushButtonType(QString name)
{

    UIPushButtonType* oops = NULL;
    
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UIPushButtonType *hunted;
            if( (hunted = dynamic_cast<UIPushButtonType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }

    return oops;
}

UITextButtonType* MythThemedDialog::getUITextButtonType(QString name)
{

    UITextButtonType* oops = NULL;
    
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UITextButtonType *hunted;
            if( (hunted = dynamic_cast<UITextButtonType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }

    return oops;
}

