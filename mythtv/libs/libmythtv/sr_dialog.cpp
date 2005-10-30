#include "scheduledrecording.h"
#include "sr_root.h"
#include "mythcontext.h"
#include "sr_dialog.h"

RecOptDialog::RecOptDialog(ScheduledRecording* sr, MythMainWindow *parent, const char *name)
            : MythDialog(parent, name), listMenu(this, "listMenu")
{
    schedRec = sr;
    program = sr->getProgramInfo();


    theme = new XMLParse();
    theme->SetWMult(wmult);
    theme->SetHMult(hmult);

    if (!theme->LoadTheme(xmldata, "recording_options"))
    {

        MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Missing Element"),
                                  tr("The theme you are using does not contain a 'recording_options' "
                                     "element.  Please contact the theme creator and ask if they could "
                                     "please update it.<br><br>The next screen will be empty.  "
                                     "Press EXIT to return to the menu."));
        return;
    }

    LoadWindow(xmldata);

    listMenu.init(theme, "selector", "menu_list", listRect);

    rootGroup = sr->getRootGroup();
    rootGroup->setParentList(&listMenu);
    listMenu.setCurGroup(rootGroup);

    setNoErase();
    allowEvents = true;
    allowUpdates = true;
    updateBackground();
}

RecOptDialog::~RecOptDialog()
{

}

void RecOptDialog::constructList(void)
{


}


void RecOptDialog::LoadWindow(QDomElement &element)
{
    QString name;
    int context;
    QRect area;

    for (QDomNode child = element.firstChild(); !child.isNull(); child = child.nextSibling())
    {
        QDomElement e = child.toElement();

        if (!e.isNull())
        {
            if (e.tagName() == "font")
                theme->parseFont(e);
            else if (e.tagName() == "container")
            {
                theme->parseContainer(e, name, context, area);
                if (name.lower() == "program_info")
                    infoRect = area;
                else if (name == "selector")
                    listRect = area;
            }
            else
            {
                MythPopupBox::showOkPopup(gContext->GetMainWindow(), tr("Unknown Element"),
                                          QString(tr("The theme you are using contains an "
                                          "unknown element ('%1').  It will be ignored")).arg(e.tagName()));
            }
        }
    }
}

void RecOptDialog::paintEvent(QPaintEvent *e)
{
    if (!allowUpdates)
    {
        updateAll = true;
        return;
    }

    QRect r = e->rect();
    QPainter p(this);

    if (updateAll || r.intersects(infoRect))
        updateInfo(&p);

    listMenu.paintEvent(r, &p, updateAll);


}

void RecOptDialog::keyPressEvent(QKeyEvent *e)
{
    if (!allowEvents)
        return;

    allowEvents = false;
    bool handled = false;

    QStringList actions;
    gContext->GetMainWindow()->TranslateKeyPress("TV Frontend", e, actions);


    for (unsigned int i = 0; i < actions.size() && !handled; i++)
    {
        QString action = actions[i];
        handled = true;
        if (action == "ESCAPE")
        {
            if (!listMenu.goBack())
                done(MythDialog::Rejected );
        }
        else if (!listMenu.getLocked())
        {
            if (action == "UP")
                listMenu.cursorUp(false);
            else if (action == "DOWN")
                listMenu.cursorDown(false);
            else if (action == "PAGEUP")
                listMenu.cursorUp(true);
            else if (action == "PAGEDOWN")
                listMenu.cursorDown(true);
            else if (action == "SELECT")
                listMenu.select();
            else if (action == "LEFT")
                listMenu.cursorLeft(false);
            else if (action == "RIGHT")
                listMenu.cursorRight(false);
            else if (action == "PAGELEFT")
                listMenu.cursorLeft(true);
            else if (action == "PAGERIGHT")
                listMenu.cursorRight(true);
            else
                handled = false;
        }
    }

    if (!handled)
        MythDialog::keyPressEvent(e);

    allowEvents = true;
}

void RecOptDialog::updateInfo(QPainter *p)
{

    LayerSet *container = theme->GetSet("program_info");

    if (container)
    {

        if (infoMap.isEmpty())
        {
            if (schedRec)
                schedRec->ToMap(infoMap);
            else
                // this should NEVER happen
                return;
        }

        QRect pr = infoRect;
        QPixmap pix(pr.size());
        pix.fill(this, pr.topLeft());
        QPainter tmp(&pix);


        container->ClearAllText();
        container->SetText(infoMap);

        container->Draw(&tmp, 4, 0);
        container->Draw(&tmp, 5, 0);
        container->Draw(&tmp, 6, 0);
        container->Draw(&tmp, 7, 0);
        container->Draw(&tmp, 8, 0);

        tmp.end();
        p->drawPixmap(pr.topLeft(), pix);
    }


}


void RecOptDialog::updateBackground(void)
{
    QPixmap bground(size());
    bground.fill(this, 0, 0);

    QPainter tmp(&bground);

    LayerSet *container = theme->GetSet("background");
        container->Draw(&tmp, 0, 0);

    tmp.end();
    myBackground = bground;

    setPaletteBackgroundPixmap(myBackground);
}
