#include <qcursor.h>
#include <qdialog.h>
#include <qdir.h>
#include <qvbox.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qobjectlist.h>
#include <qdir.h>
#include <qregexp.h>
#include <qaccel.h>
#include <qfocusdata.h>

#include <iostream>
using namespace std;

#ifdef USE_LIRC
#include <pthread.h>
#include "lirc.h"
#include "lircevent.h"
#endif

#include "uitypes.h"
#include "xmlparse.h"
#include "mythdialogs.h"
#include "lcddevice.h"

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
    ignore_lirc_keys = false;
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
    gContext->GetScreenSettings(xbase, screenwidth, wmult,
                                ybase, screenheight, hmult);
    setGeometry(xbase, ybase, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(gContext->GetMediumFont());
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

void MythMainWindow::customEvent(QCustomEvent *ce)
{
    if (ce->type() == kExternalKeycodeEventType)
    {
        ExternalKeycodeEvent *eke = (ExternalKeycodeEvent *)ce;
        int keycode = eke->getKeycode();

        QKeyEvent key(QEvent::KeyPress, keycode, 0, Qt::NoButton);

        QObject *key_target = getTarget(key);

        QApplication::sendEvent(key_target, &key);
    }
#ifdef USE_LIRC
    else if (ce->type() == kLircKeycodeEventType && !ignore_lirc_keys) 
    {
        LircKeycodeEvent *lke = (LircKeycodeEvent *)ce;
        int keycode = lke->getKeycode();

        if (keycode) 
        {
            int mod = keycode & MODIFIER_MASK;
            int k = keycode & ~MODIFIER_MASK; /* trim off the mod */
            int ascii = 0;
            QString text;

            if (k & UNICODE_ACCEL)
            {
                QChar c(k & ~UNICODE_ACCEL);
                ascii = c.latin1();
                text = QString(c);
            }

            QKeyEvent key(lke->isKeyDown() ? QEvent::KeyPress :
                          QEvent::KeyRelease, k, ascii, mod, text);

            QObject *key_target = getTarget(key);

            QApplication::sendEvent(key_target, &key);
        }
        else
        {
            cerr << "LircClient warning: attempt to convert '"
                 << lke->getLircText() << "' to a key sequence failed. Fix"
                                           " your key mappings.\n";
        }
    }
    else if (ce->type() == kLircMuteEventType)
    {
        LircMuteEvent *lme = (LircMuteEvent *)ce;
        ignore_lirc_keys = lme->eventsMuted();
    }
#endif
}

QObject *MythMainWindow::getTarget(QKeyEvent &key)
{
    QObject *key_target = NULL;

    key_target = QWidget::keyboardGrabber();

    if (!key_target)
    {
        QWidget *focus_widget = qApp->focusWidget();
        if (focus_widget && focus_widget->isEnabled())
        {
            key_target = focus_widget;

            // Yes this is special code for handling the
            // the escape key.
            if (key.key() == Key_Escape && focus_widget->topLevelWidget())
                key_target = focus_widget->topLevelWidget();
        }
    }

    if (!key_target)
        key_target = this;

    return key_target;
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

    gContext->GetScreenSettings(xbase, screenwidth, wmult,
                                ybase, screenheight, hmult);

    defaultBigFont = gContext->GetBigFont();
    defaultMediumFont = gContext->GetMediumFont();
    defaultSmallFont = gContext->GetSmallFont();

    setFont(defaultMediumFont);
    setCursor(QCursor(Qt::BlankCursor));

    if (setsize)
    {
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
#ifndef QWS
    WFlags flags = getWFlags();
    flags |= WRepaintNoErase;
    setWFlags(flags);
#endif
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
    float wmult, hmult;

    gContext->GetScreenSettings(wmult, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setPalette(parent->palette());
    setFont(parent->font());
    setCursor(QCursor(Qt::BlankCursor));

    hpadding = 110;
    wpadding = 80;

    vbox = new QVBoxLayout(this, (int)(10 * hmult));
}

MythPopupBox::MythPopupBox(MythMainWindow *parent, bool graphicPopup,
                           QColor popupForeground, QColor popupBackground,
                           QColor popupHighlight, const char *name)
            : MythDialog(parent, name, false)
{
    float wmult, hmult;

    gContext->GetScreenSettings(wmult, hmult);

    setLineWidth(3);
    setMidLineWidth(3);
    setFrameShape(QFrame::Panel);
    setFrameShadow(QFrame::Raised);
    setFrameStyle(QFrame::Box | QFrame::Plain);
    setPalette(parent->palette());
    setFont(parent->font());
    setCursor(QCursor(Qt::BlankCursor));

    hpadding = 110;
    wpadding = 80;

    vbox = new QVBoxLayout(this, (int)(10 * hmult));

    if (!graphicPopup)
        setPaletteBackgroundColor(popupBackground);
    else
        gContext->ThemeWidget(this);
    setPaletteForegroundColor(popupHighlight);

    popupForegroundColor = popupForeground;
}

bool MythPopupBox::focusNextPrevChild(bool next)
{
    QFocusData *focusList = focusData();
    QObjectList *objList = queryList(NULL,NULL,false,true);

    QWidget *startingPoint = focusList->home();
    QWidget *candidate = NULL;

    QWidget *w = (next) ? focusList->prev() : focusList->next();

    int countdown = focusList->count();

    do
    {
        if (w && w != startingPoint && !w->focusProxy() && 
            w->isVisibleTo(this) && w->isEnabled() &&
            (objList->find((QObject *)w) != -1))
        {
            candidate = w;
        }

        w = (next) ? focusList->prev() : focusList->next();
    }
    while (w && !(candidate && w == startingPoint) && (countdown-- > 0));

    if (!candidate)
        return false;

    candidate->setFocus();
    return true;
}

void MythPopupBox::addWidget(QWidget *widget, bool setAppearance)
{
    if (setAppearance == true)
    {
         widget->setPalette(palette());
         widget->setFont(font());
    }

    if (widget->isA("QLabel"))
    {
        widget->setBackgroundOrigin(ParentOrigin);
        widget->setPaletteForegroundColor(popupForegroundColor);
    }

    vbox->addWidget(widget);
}

QLabel *MythPopupBox::addLabel(QString caption, LabelSize size, bool wrap)
{
    QLabel *label = new QLabel(caption, this);
    switch (size)
    {
        case Large: label->setFont(defaultBigFont); break;
        case Medium: label->setFont(defaultMediumFont); break;
        case Small: label->setFont(defaultSmallFont); break;
    }

    label->setMaximumWidth((int)m_parent->width() / 2);
    if (wrap)
        label->setAlignment(Qt::WordBreak | Qt::AlignLeft);

    addWidget(label, false);
    return label;
}

QButton *MythPopupBox::addButton(QString caption, QObject *target, 
                                 const char *slot)
{
    if (!target)
    {
        target = this;
        slot = SLOT(defaultButtonPressedHandler());
    }

    MythPushButton *button = new MythPushButton(caption, this);
    m_parent->connect(button, SIGNAL(pressed()), target, slot);
    addWidget(button, false);
    return button;
}

void MythPopupBox::ShowPopup(QObject *target, const char *slot)
{
    const QObjectList *objlist = children();
    QObjectListIt it(*objlist);
    QObject *objs;

    while ((objs = it.current()) != 0)
    {
        ++it;
        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            widget->adjustSize();
        }
    }

    polish();

    int x = 0, y = 0, maxw = 0, poph = 0;

    it = QObjectListIt(*objlist);
    while ((objs = it.current()) != 0)
    {
        ++it;
        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            poph += widget->height();
            if (widget->width() > maxw)
                maxw = widget->width();
        }
    }

    poph += (int)(hpadding * hmult);
    setMinimumHeight(poph);

    maxw += (int)(wpadding * wmult);

    int width = (int)(800 * wmult);
    int height = (int)(600 * hmult);

    if (parentWidget())
    {
        width = parentWidget()->width();
        height = parentWidget()->height();
    }

    x = (int)(width / 2) - (int)(maxw / 2);
    y = (int)(height / 2) - (int)(poph / 2);

    setFixedSize(maxw, poph);
    setGeometry(x, y, maxw, poph);

    if (target && slot)
    {
        QAccel *popaccel = new QAccel(this);
        popaccel->connectItem(popaccel->insertItem(m_parent->Key_Escape),
                              target, slot);
    }

    Show();
}

int MythPopupBox::ExecPopup(QObject *target, const char *slot)
{
    if (!target)
        ShowPopup(this, SLOT(defaultExitHandler()));
    else
        ShowPopup(target, slot);

    return exec();
}

void MythPopupBox::defaultButtonPressedHandler(void)
{
    const QObjectList *objlist = children();
    QObjectListIt it(*objlist);
    QObject *objs;
    int i = 0;
    while ((objs = it.current()) != 0)
    {
        ++it;
        if (objs->isWidgetType())
        {
            QWidget *widget = (QWidget *)objs;
            if (widget->hasFocus())
                break;
            i++;
        }
    }
    done(i);
}

void MythPopupBox::defaultExitHandler()
{
    done(-1);
}

void MythPopupBox::showOkPopup(MythMainWindow *parent, QString title,
                               QString message)
{
    MythPopupBox popup(parent, title);
    popup.addLabel(message, Medium, true);
    QButton *okButton = popup.addButton(tr("OK"));
    okButton->setFocus();
    popup.ExecPopup();
}

bool MythPopupBox::showOkCancelPopup(MythMainWindow *parent, QString title,
                                     QString message, bool focusOk)
{
    MythPopupBox popup(parent, title);
    popup.addLabel(message, Medium, true);
    QButton *okButton = popup.addButton(tr("OK"));
    QButton *cancelButton = popup.addButton(tr("Cancel"));
    if (focusOk)
        okButton->setFocus();
    else
        cancelButton->setFocus();

    return (popup.ExecPopup() == 1);
}

MythProgressDialog::MythProgressDialog(const QString &message, int totalSteps)
                  : MythDialog(gContext->GetMainWindow(), "progress", false)
{
    int screenwidth, screenheight;
    float wmult, hmult;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setFont(gContext->GetMediumFont());
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

    LCD *lcddev = gContext->GetLCDDevice();

    if (lcddev)
    {
        textItems = new QPtrList<LCDTextItem>;
        textItems->setAutoDelete(true);

        textItems->clear();
        textItems->append(new LCDTextItem(1, ALIGN_CENTERED, message, "Generic",
                          false));
        lcddev->switchToGeneric(textItems);
    }
    else 
        textItems = NULL;

    show();

    qApp->processEvents();
}

void MythProgressDialog::Close(void)
{
    accept();

    if (textItems)
    {
        LCD *lcddev = gContext->GetLCDDevice();
        lcddev->switchToNothing();
        lcddev->switchToTime();
        delete textItems;
    }
}

void MythProgressDialog::setProgress(int curprogress)
{
    float fProgress = (float)curprogress / (steps * 1000.0);
    LCD *lcddev = gContext->GetLCDDevice();
    if (lcddev)
        lcddev->setGenericProgress(fProgress);
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
                                   QString theme_filename, const char* name,
                                   bool setsize)
                : MythDialog(parent, name, setsize)
{
    setNoErase();
    context = -1;
    my_containers.clear();
    widget_with_current_focus = NULL;

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

    buildFocusList();

    updateBackground();
    initForeground();
}

bool MythThemedDialog::buildFocusList()
{
    //
    //  Build a list of widgets that will take focus
    //

    focus_taking_widgets.clear();


    //  Loop over containers
    LayerSet *looper;
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
    if(focus_taking_widgets.count() > 0)
    {
        return true;
    }
    return false;
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
            cout << "A container called \"" << looper->GetName() 
                 << "\" said its area is "
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

    QPtrListIterator<UIType> an_it(focus_taking_widgets);
    UIType *looper;

    while ((looper = an_it.current()) != 0)
    {
        if(looper->canTakeFocus())
        {
            widget_with_current_focus = looper;
            widget_with_current_focus->takeFocus();
            return true;
        }
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
            if (reached_current && looper->canTakeFocus())
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
            if (reached_current && looper->canTakeFocus())
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
            an_it.toLast();
            while ((looper = an_it.current()) != 0)
            {
                if(looper->canTakeFocus())
                {
                    widget_with_current_focus->looseFocus();
                    widget_with_current_focus = looper;
                    widget_with_current_focus->takeFocus();
                    return true;
                }
                --an_it;
            }
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

UIType* MythThemedDialog::getCurrentFocusWidget()
{
    if(widget_with_current_focus)
    {
        return widget_with_current_focus;
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

UICheckBoxType* MythThemedDialog::getUICheckBoxType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UICheckBoxType *hunted;
            if( (hunted = dynamic_cast<UICheckBoxType*>(hunter)) )
            {
                return hunted;
            }
        }
        ++an_it;
    }
    return NULL;
}

UISelectorType* MythThemedDialog::getUISelectorType(const QString &name)
{
    QPtrListIterator<LayerSet> an_it(my_containers);
    LayerSet *looper;

    while( (looper = an_it.current()) != 0)
    {
        UIType *hunter = looper->GetType(name);
        if(hunter)
        {
            UISelectorType *hunted;
            if( (hunted = dynamic_cast<UISelectorType*>(hunter)) )
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

/*
---------------------------------------------------------------------
*/

MythPasswordDialog::MythPasswordDialog(QString message,
                                       bool *success,
                                       QString target,
                                       MythMainWindow *parent, 
                                       const char *name, 
                                       bool)
                   :MythDialog(parent, name, false)
{
    success_flag = success;
    target_text = target;

    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    this->setGeometry((screenwidth - 250 ) / 2,
                      (screenheight - 50 ) / 2,
                      300,50);
    QFrame *outside_border = new QFrame(this);
    outside_border->setGeometry(0,0,300,50);
    outside_border->setFrameStyle(QFrame::Panel | QFrame::Raised );
    outside_border->setLineWidth(4);
    QLabel *message_label = new QLabel(message, this);
    message_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    message_label->setGeometry(15,10,130,30);
    message_label->setBackgroundOrigin(ParentOrigin);
    password_editor = new MythLineEdit(this);
    password_editor->setEchoMode(QLineEdit::Password);
    password_editor->setGeometry(150,10,135,30);
    password_editor->setBackgroundOrigin(ParentOrigin);
    connect(password_editor, SIGNAL(textChanged(const QString &)),
            this, SLOT(checkPassword(const QString &)));

    this->setActiveWindow();
    password_editor->setFocus();
}

void MythPasswordDialog::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Key_Escape)
    {
        MythDialog::keyPressEvent(e);
    }
}


void MythPasswordDialog::checkPassword(const QString &the_text)
{
    if (the_text == target_text)
    {
        *success_flag = true;
        done(0);
    }
    else
    {
        //  Oh to beep 
    }
}

MythPasswordDialog::~MythPasswordDialog()
{
}

/*
---------------------------------------------------------------------
 */

Myth2ButtonDialog::Myth2ButtonDialog(MythMainWindow *parent, QString title1,
                                     QString message, QString button1msg,
                                     QString button2msg, int defvalue)
                 : MythDialog(parent, title1, false)
{
    gContext->GetScreenSettings(screenwidth, wmult, screenheight, hmult);
    QFont bigFont = gContext->GetBigFont();
    QFont medFont = gContext->GetMediumFont();

    popup = new MythPopupBox(gContext->GetMainWindow(), "2 button dialog");
    popup->setFrameStyle( QFrame::Box | QFrame::Plain );
    gContext->ThemeWidget(popup);
    QLabel *msg = new QLabel(tr(message), popup);
    msg->setBackgroundOrigin(ParentOrigin);
    QLabel *filler1 = new QLabel("", popup);
    filler1->setBackgroundOrigin(ParentOrigin);

    QLabel *title = new QLabel(title1, popup);
    title->setBackgroundOrigin(ParentOrigin);
    title->setFont(bigFont);
    title->setMaximumWidth((int)(width() / 2));

    QLabel *filler2 = new QLabel("", popup);
    filler2->setBackgroundOrigin(ParentOrigin);

    but1 = new MythPushButton(tr(button1msg), popup);
    but2 = new MythPushButton(tr(button2msg), popup);

    popup->addWidget(msg, false);
    popup->addWidget(filler1, false);
    popup->addWidget(title, false);
    popup->addWidget(filler2, false);
    popup->addWidget(but1, false);
    popup->addWidget(but2, false);

    if (defvalue == 1)
        but1->setFocus();
    else
        but2->setFocus();

    msg->adjustSize();
    filler1->adjustSize();
    filler2->adjustSize();
    title->adjustSize();
    but1->adjustSize();
    but2->adjustSize();

    popup->polish();

    int x, y, maxw, poph;
    poph = msg->height() + filler1->height() +
           title->height() + but1->height() + but2->height() +
           (int)(110 * hmult);
    popup->setMinimumHeight(poph);
    maxw = 0;

    if (title->width() > maxw)
        maxw = title->width();
    if (but1->width() > maxw)
        maxw = but1->width();
    if (but2->width() > maxw)
        maxw = but2->width();

    maxw += (int)(80 * wmult);

    x = (int)(screenwidth / 2) - (int)(maxw / 2);
    y = (int)(screenheight / 2) - (int)(poph / 2);

    popup->setFixedSize(maxw, poph);
    popup->setGeometry(x, y, maxw, poph);
//    popup->setFocusPolicy(StrongFocus);
    popup->Show();
    grabKeyboard();
}

void Myth2ButtonDialog::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) 
    {
        case Key_Escape:
            done(0);
            break;
        case Key_Enter:
        case Key_Return:
        case Key_Space:
            if (but1->hasFocus())
                done(1);
            else
                done(2);
            break;
        case Key_Up:
        case Key_Down:
            if (but1->hasFocus())
                but2->setFocus();
            else
                but1->setFocus();
            break;
    }
}

/*
---------------------------------------------------------------------
*/

MythImageFileDialog::MythImageFileDialog(QString *result,
                                         QString top_directory,
                                         MythMainWindow *parent, 
                                         QString window_name,
                                         QString theme_filename,
                                         const char *name, 
                                         bool setsize)
                   :MythThemedDialog(parent, 
                                     window_name, 
                                     theme_filename,
                                     name,
                                     setsize)
{
    selected_file = result;
    initial_node = NULL;
    
    //
    //  MythImageFileDialog's expect there to be certain
    //  elements in the theme, or they will fail.
    //

    //
    //  For example, we use the size of the background
    //  pixmap to define the geometry of the dialog. If
    //  the pixmap ain't there, we need to fail.
    //
    

    UIImageType *file_browser_background = getUIImageType("file_browser_background");
    if(file_browser_background)
    {
        QPixmap background = file_browser_background->GetImage();
        
        this->setFixedSize(QSize(background.width(), background.height()));
        this->move((screenwidth - background.width()) / 2,
                   (screenheight - background.height()) / 2);
    }
    else
    {
        cerr << "myhdialogs.o: Could not find a UIImageType called file_browser_background in your theme." << endl;
        exit(0);
    }

    //
    //  Make a nice border
    //

    this->setFrameStyle(QFrame::Panel | QFrame::Raised );
    this->setLineWidth(4);


    //
    //  Find the managed tree list that handles browsing.
    //  Fail if we can't find it.
    //

    file_browser = getUIManagedTreeListType("file_browser");
    if(file_browser)
    {
        file_browser->calculateScreenArea();
        file_browser->showWholeTree(true);
        connect(file_browser, SIGNAL(nodeSelected(int, IntVector*)),
                this, SLOT(handleTreeListSelection(int, IntVector*)));
        connect(file_browser, SIGNAL(nodeEntered(int, IntVector*)),
                this, SLOT(handleTreeListEntered(int, IntVector*)));
    }
    else
    {
        cerr << "mythdialogs.o: Could not find a UIManagedTreeListType called file_browser in your theme." << endl;
        exit(0);
    }    
    
    //
    //  Find the UIImageType for previewing
    //  No image_box, no preview.
    //
    
    image_box = getUIImageType("image_box");
    if(image_box)
    {
        image_box->calculateScreenArea();
    }
    
    image_files.clear();
    buildTree(top_directory);

    file_browser->assignTreeData(root_parent);
    if(initial_node)
    {
        file_browser->setCurrentNode(initial_node);
    }
    file_browser->enter();
    file_browser->refresh();
    
}

void MythImageFileDialog::keyPressEvent(QKeyEvent *e)
{
    switch(e->key())
    {
        case Key_Up:       file_browser->moveUp();             break;
        case Key_Down:     file_browser->moveDown();           break;
        case Key_Left:     file_browser->popUp();              break;
        case Key_Right:    file_browser->pushDown();           break;
        case Key_PageUp:   file_browser->pageUp();             break;
        case Key_PageDown: file_browser->pageDown();           break;
        
        case Key_Space:
        case Key_Enter:
        case Key_Return:   file_browser->select();             break;
        
        default:           MythThemedDialog::keyPressEvent(e); break;
    }
}

void MythImageFileDialog::buildTree(QString starting_where)
{
    buildFileList(starting_where);
    
    root_parent = new GenericTree("Image Files root", -1, false);
    file_root = root_parent->addNode("Image Files", -1, false);

    //
    //  Go through the files and build a tree
    //    
    for(uint i = 0; i < image_files.count(); ++i)
    {
        bool make_active = false;
        QString file_string = *(image_files.at(i));
        if(file_string == *selected_file)
        {
            make_active = true;
        }
        QString prefix = gContext->GetSetting("VideoStartupDir");
        if(prefix.length() < 1)
        {
            cerr << "mythdialogs.o: Seems unlikely that this is going to work" << endl;
        }
        file_string.remove(0, prefix.length());
        QStringList list(QStringList::split("/", file_string));
        GenericTree *where_to_add;
        where_to_add = file_root;
        int a_counter = 0;
        QStringList::Iterator an_it = list.begin();
        for( ; an_it != list.end(); ++an_it)
        {
            if(a_counter + 1 >= (int) list.count())
            {
                QString title = (*an_it);
                GenericTree *added_node = where_to_add->addNode(title.section(".",0,0), i, true);
                if(make_active)
                {
                    initial_node = added_node;
                }
            }
            else
            {
                QString dirname = *an_it + "/";
                GenericTree *sub_node;
                sub_node = where_to_add->getChildByName(dirname);
                if(!sub_node)
                {
                    sub_node = where_to_add->addNode(dirname, -1, false);
                }
                where_to_add = sub_node;
            }
            ++a_counter;
        }
    }
    if(file_root->childCount() < 1)
    {
        //
        //  Nothing survived the requirements
        //
        file_root->addNode("No files found", -1, false);
    }
}

void MythImageFileDialog::buildFileList(QString directory)
{
    QStringList imageExtensions = QImage::inputFormatList();

    QDir d(directory);
       
    if (!d.exists())
        return;

    const QFileInfoList *list = d.entryInfoList();

    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;
    QRegExp r;
    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." ||
            fi->fileName() == ".." )
        {
            continue;
        }
            
        if(fi->isDir())
        {
            buildFileList(fi->absFilePath());
        }
        else
        {
            r.setPattern("^" + fi->extension() + "$");
            r.setCaseSensitive(false);
            QStringList result = imageExtensions.grep(r);
            if(!result.isEmpty())
            {
                image_files.append(fi->absFilePath());
            }
            else
            {
                r.setPattern("^" + fi->extension());
                r.setCaseSensitive(false);
                QStringList other_result = imageExtensions.grep(r);
                if(!result.isEmpty())
                {
                    image_files.append(fi->absFilePath());
                }
            }
        }
    }                                                                                                                                                                                                                                                               
}

void MythImageFileDialog::handleTreeListEntered(int type, IntVector*)
{
    if(image_box)
    {
        if(type > -1)
        {
            image_box->SetImage(image_files[type]);
        }
        else
        {
            image_box->SetImage("");
        }
        image_box->LoadImage();
    }
}

void MythImageFileDialog::handleTreeListSelection(int type, IntVector*)
{
    if(type > -1)
    {
        *selected_file = image_files[type];
        done(0);
    }   
}

MythImageFileDialog::~MythImageFileDialog()
{
    if(root_parent)
    {
        root_parent->deleteAllChildren();
        delete root_parent;
        root_parent = NULL;
    }
}

