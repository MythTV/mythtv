#include <qcursor.h>
#include <qdialog.h>
#include <qdir.h>
#include <qvbox.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qobjectlist.h>

#include <iostream>
using namespace std;

#ifdef USE_LIRC
#include <pthread.h>
#include "lirc.h"
#endif

#include "uitypes.h"
#include "xmlparse.h"
#include "mythdialogs.h"

#ifdef USE_LIRC
static void *SpawnLirc(void *param)
{
    MythMainWindow *main_window = (MythMainWindow *)param;
    QString config_file = QDir::homeDirPath() + "/.mythtv/lircrc";
    QString program("mythtv");
    LircClient *cl = new LircClient(main_window);
    if (!cl->Init(config_file, program))
        cl->Process();

    return NULL;
}
#endif

MythMainWindow::MythMainWindow(QWidget *parent, const char *name, bool modal)
              : QDialog(parent, name, modal)
{
    Init();
#ifdef USE_LIRC
    pthread_t lirc_tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&lirc_tid, &attr, SpawnLirc, this);
#endif
}

void MythMainWindow::Init(void)
{
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int x = 0, y = 0, w = 0, h = 0;
#ifndef QWS
    GetMythTVGeometry(qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);
#endif

    int xoff = gContext->GetNumSetting("GuiOffsetX", 0);
    int yoff = gContext->GetNumSetting("GuiOffsetY", 0);

    x += xoff;
    y += yoff;

    setGeometry(x, y, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(gContext->GetMediumFontSize() * hmult),
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    gContext->ThemeWidget(this);

    Show();
}

void MythMainWindow::Show(void)
{
    if (gContext->GetNumSetting("RunFrontendInWindow", 0))
        show();
    else
        showFullScreen();
    setActiveWindow();
}

void MythMainWindow::attach(QWidget *child)
{
    widgetList.push_back(child);
    child->raise();
    child->setFocus();
}

void MythMainWindow::detach(QWidget *child)
{
    if (widgetList.back() != child)
    {
        cerr << "Not removing top most widget, error\n";
        return;
    }

    widgetList.pop_back();
    QWidget *current = currentWidget();

    if (current)
        current->setFocus();
}

QWidget *MythMainWindow::currentWidget(void)
{
    if (widgetList.size() > 0)
        return widgetList.back();
    return NULL;
}

void MythMainWindow::keyPressEvent(QKeyEvent *e)
{
    QWidget *current = currentWidget();
    if (current)
        qApp->notify(current, e);
    else
        QDialog::keyPressEvent(e);
}

MythDialog::MythDialog(MythMainWindow *parent, const char *name, bool setsize)
          : QFrame(parent, name)
{
    rescode = 0;

    if (!parent)
    {
        cerr << "Trying to create a dialog without a parent.\n";
        return;
    }

    in_loop = false;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int x = 0, y = 0, w = 0, h = 0;
#ifndef QWS
    GetMythTVGeometry(qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);
#endif

    setFont(QFont("Arial", (int)(gContext->GetMediumFontSize() * hmult),
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    if (setsize)
    {
        setGeometry(x, y, screenwidth, screenheight);
        setFixedSize(QSize(screenwidth, screenheight));
        gContext->ThemeWidget(this);
    }

    parent->attach(this);
    m_parent = parent;
}

MythDialog::~MythDialog()
{
    m_parent->detach(this);
}

void MythDialog::setNoErase(void)
{
    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
    setWFlags(flags);
}

void MythDialog::Show(void)
{
    show();
}

void MythDialog::done(int r)
{
    hide();
    setResult(r);
    close();
}

void MythDialog::accept()
{
    done(Accepted);
}

void MythDialog::reject()
{
    done(Rejected);
}

int MythDialog::exec()
{
    if (in_loop) 
    {
        qWarning("MythDialog::exec: Recursive call detected.");
        return -1;
    }

    setResult(0);

    Show();

    in_loop = TRUE;
    qApp->enter_loop();

    int res = result();

    return res;
}

void MythDialog::hide()
{
    if (isHidden())
        return;

    // Reimplemented to exit a modal when the dialog is hidden.
    QWidget::hide();
    if (in_loop)  
    {
        in_loop = FALSE;
        qApp->exit_loop();
    }
}

void MythDialog::keyPressEvent( QKeyEvent *e )
{
    //   Calls reject() if Escape is pressed. Simulates a button
    //   click for the default button if Enter is pressed. Move focus
    //   for the arrow keys. Ignore the rest.
    if ( e->state() == 0 || ( e->state() & Keypad && e->key() == Key_Enter ) ) {
        switch ( e->key() ) {
        case Key_Escape:
            reject();
            break;
        case Key_Up:
        case Key_Left:
            if ( focusWidget() &&
                 ( focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                   focusWidget()->focusPolicy() == QWidget::WheelFocus ) ) {
                break;
            }
            // call ours, since c++ blocks us from calling the one
            // belonging to focusWidget().
            focusNextPrevChild( FALSE );
            break;
        case Key_Down:
        case Key_Right:
            if ( focusWidget() &&
                 ( focusWidget()->focusPolicy() == QWidget::StrongFocus ||
                   focusWidget()->focusPolicy() == QWidget::WheelFocus ) ) {
                break;
            }
            focusNextPrevChild( TRUE );
            break;
        default:
            return;
        }
    }
}

MythPopupBox::MythPopupBox(MythMainWindow *parent, const char *name)
            : MythDialog(parent, name, false)
{
    int w, h;
    float wmult, hmult;

    gContext->GetScreenSettings(w, wmult, h, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setPalette(parent->palette());
    setFont(parent->font());
    setCursor(QCursor(Qt::BlankCursor));

    vbox = new QVBoxLayout(this, (int)(10 * hmult));
}

void MythPopupBox::addWidget(QWidget *widget, bool setAppearance)
{
    if (setAppearance == true)
    {
         widget->setPalette(palette());
         widget->setFont(font());
    }
    vbox->addWidget(widget);
}

MythProgressDialog::MythProgressDialog(const QString &message, int totalSteps)
                  : MythDialog(gContext->GetMainWindow(), "progress", false)
{
    int screenwidth, screenheight;
    float wmult, hmult;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    int x = 0, y = 0, w = 0, h = 0;
#ifndef QWS
    GetMythTVGeometry(qt_xdisplay(), qt_xscreen(), &x, &y, &w, &h);
#endif

    setFont(QFont("Arial", (int)(gContext->GetMediumFontSize() * hmult),
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    gContext->ThemeWidget(this);

    int yoff = screenheight / 3;
    int xoff = screenwidth / 10;
    setGeometry(xoff, yoff, screenwidth - xoff * 2, yoff);
    setFixedSize(QSize(screenwidth - xoff * 2, yoff));

    QVBoxLayout *lay = new QVBoxLayout(this, 0);

    QVBox *vbox = new QVBox(this);
    lay->addWidget(vbox);

    vbox->setLineWidth(3);
    vbox->setMidLineWidth(3);
    vbox->setFrameShape(QFrame::Panel);
    vbox->setFrameShadow(QFrame::Raised);
    vbox->setMargin((int)(15 * wmult));

    QLabel *msglabel = new QLabel(vbox);
    msglabel->setBackgroundOrigin(ParentOrigin);
    msglabel->setText(message);

    progress = new QProgressBar(totalSteps, vbox);
    progress->setBackgroundOrigin(ParentOrigin);
    progress->setProgress(0);

    steps = totalSteps / 1000;
    if (steps == 0)
        steps = 1;

    gContext->LCDswitchToChannel(message);

    show();

    qApp->processEvents();
}

void MythProgressDialog::Close(void)
{
    accept();
    gContext->LCDswitchToTime();
}

void MythProgressDialog::setProgress(int curprogress)
{
    gContext->LCDsetChannelProgress( (curprogress + 0.0) / (steps * 1000.0));
    progress->setProgress(curprogress);
    if (curprogress % steps == 0)
        qApp->processEvents();
}

void MythProgressDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Key_Escape)
        ;
    else
        MythDialog::keyPressEvent(e);
}

MythThemedDialog::MythThemedDialog(MythMainWindow *parent, QString window_name,
                                   QString theme_filename, const char* name)
                : MythDialog(parent, name)
{
    setNoErase();
    context = -1;
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
    while ((looper = an_it.current()) != 0)
    {
        //  Loop over UITypes within each container
        vector<UIType *> *all_ui_type_objects = looper->getAllTypes();
        vector<UIType *>::iterator i = all_ui_type_objects->begin();
        for (; i != all_ui_type_objects->end(); i++)
        {
            UIType *type = (*i);
            connect(type, SIGNAL(requestUpdate()), this, 
                    SLOT(updateForeground()));
            connect(type, SIGNAL(requestUpdate(const QRect &)), this, 
                    SLOT(updateForeground(const QRect &)));
        }
        ++an_it;
    }

    //
    //  Build a list of widgets that will take focus
    //

    //  Loop over containers
    QPtrListIterator<LayerSet> another_it(my_containers);
    while ((looper = another_it.current()) != 0)
    {
        //  Loop over UITypes within each container
        vector<UIType *> *all_ui_type_objects = looper->getAllTypes();
        vector<UIType *>::iterator i = all_ui_type_objects->begin();
        for (; i != all_ui_type_objects->end(); i++)
        {
            UIType *type = (*i);
            if (type->canTakeFocus())
            {
                focus_taking_widgets.append(type);
            }
        }
        ++another_it;
    }

    updateBackground();
    initForeground();
}

MythThemedDialog::~MythThemedDialog()
{
    delete theme;
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
                cerr << "dialogbox.o: I don't understand this DOM Element:" 
                     << e.tagName() << endl;
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
    int a_context;
    theme->parseContainer(element, name, a_context, area);
    if (name.length() < 1)
    {
        cerr << "dialogbox.o: I told an object to parse a container and it "
                "didn't give me a name back\n";
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
    cerr << "I don't know how to handle popops yet (I'm going to try and "
            "just ignore it)\n";
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

    //
    //  *IFF* there is a background, draw it
    //
    if (container)
    {
        container->Draw(&tmp, 0, context);
        tmp.end();
    }

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

    QPainter whole_dialog_painter(&my_foreground);

    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while ((looper = an_it.current()) != 0)
    {
        QRect container_area = looper->GetAreaRect();

        //
        //  Only paint if the container's area is valid
        //  and it intersects with whatever Qt told us
        //  needed to be repainted
        //

        if (container_area.isValid() &&
            r.intersects(container_area) &&
            looper->GetName().lower() != "background")
        {
            //
            //  Debugging
            //
            /*
            cout << "A container called \"" << looper->GetName() << "\" said its
 area is "
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
            offscreen_painter.drawPixmap(0, 0, my_background, 
                                         container_area.left(), 
                                         container_area.top());

            //
            //  Loop over the draworder layers

            for (int i = 0; i <= looper->getLayers(); i++)
            {
                looper->Draw(&offscreen_painter, i, context);
            }

            //
            //  If it did in fact paint something (empty
            //  container?) then tell it we're done
            //
            if (offscreen_painter.isActive())
            {
                offscreen_painter.end();
                whole_dialog_painter.drawPixmap(container_area.topLeft(), 
                                                container_picture);
            }

        }
        ++an_it;
    }

    if (whole_dialog_painter.isActive())
    {
        whole_dialog_painter.end();
    }

    update(r);
}

void MythThemedDialog::paintEvent(QPaintEvent *e)
{
    bitBlt(this, e->rect().left(), e->rect().top(),
           &my_foreground, e->rect().left(), e->rect().top(),
           e->rect().width(), e->rect().height());

    MythDialog::paintEvent(e);
}

bool MythThemedDialog::assignFirstFocus()
{
    if (widget_with_current_focus)
    {
        widget_with_current_focus->looseFocus();
    }

    if (focus_taking_widgets.count() > 0)
    {
        widget_with_current_focus = focus_taking_widgets.first();
        widget_with_current_focus->takeFocus();
        return true;
    }

    return false;
}

bool MythThemedDialog::nextPrevWidgetFocus(bool up_or_down)
{
    if (up_or_down)
    {
        bool reached_current = false;
        QPtrListIterator<UIType> an_it(focus_taking_widgets);
        UIType *looper;

        while ((looper = an_it.current()) != 0)
        {
            if (reached_current)
            {
                widget_with_current_focus->looseFocus();
                widget_with_current_focus = looper;
                widget_with_current_focus->takeFocus();
                return true;
            }

            if (looper == widget_with_current_focus)
            {
                reached_current= true;
            }
            ++an_it;
        }

        if (assignFirstFocus())
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

        while ((looper = an_it.current()) != 0)
        {
            if (reached_current)
            {
                widget_with_current_focus->looseFocus();
                widget_with_current_focus = looper;
                widget_with_current_focus->takeFocus();
                return true;
            }

            if (looper == widget_with_current_focus)
            {
                reached_current= true;
            }
            --an_it;
        }

        if (reached_current)
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
    if (widget_with_current_focus)
    {
        widget_with_current_focus->activate();
    }
    else
    {
        cerr << "dialogbox.o: Something asked me activate the current widget, "
                "but there is no current widget\n";
    }
}

UIType* MythThemedDialog::getUIObject(const QString &name)
{
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

    while ((looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if (hunter)
            return hunter;
        ++an_it;
    }

    return NULL;
}

UIManagedTreeListType* MythThemedDialog::getUIManagedTreeListType(const QString &name)
{
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
    return NULL;
}

UITextType* MythThemedDialog::getUITextType(const QString &name)
{
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
    return NULL;
}

UIPushButtonType* MythThemedDialog::getUIPushButtonType(const QString &name)
{
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
    return NULL;
}

UITextButtonType* MythThemedDialog::getUITextButtonType(const QString &name)
{
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
    return NULL;
}

UIRepeatedImageType* MythThemedDialog::getUIRepeatedImageType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UIRepeatedImageType *hunted;
            if( (hunted = dynamic_cast<UIRepeatedImageType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIBlackHoleType* MythThemedDialog::getUIBlackHoleType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UIBlackHoleType *hunted;
            if( (hunted = dynamic_cast<UIBlackHoleType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIImageType* MythThemedDialog::getUIImageType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UIImageType *hunted;
            if( (hunted = dynamic_cast<UIImageType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UIStatusBarType* MythThemedDialog::getUIStatusBarType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UIStatusBarType *hunted;
            if( (hunted = dynamic_cast<UIStatusBarType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

