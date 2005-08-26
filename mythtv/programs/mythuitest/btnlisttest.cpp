#ifndef BTNLISTTEST_CPP
#define BTNLISTTEST_CPP

#include "btnlisttest.h"
#include "mythmainwindow.h"
#include "mythlistbutton.h"
#include "mythcontext.h"
#include "mythfontproperties.h"

#include <qapplication.h>

TestWindow::TestWindow(MythScreenStack *parent)
          : MythScreenType(parent, "dialogtest")
{

    listWidth = GetMythMainWindow()->GetUIScreenRect().width() - 2*pad;

    this->setupHList();
    this->hbuttons->SetActive(true);
    this->focused = this->hbuttons;

    new MythListButtonItem(hbuttons, "Hello");
    new MythListButtonItem(hbuttons, "Goodbye");  
    new MythListButtonItem(hbuttons, "Really Really Really Really Long");
    new MythListButtonItem(hbuttons, "A B C D");

    this->setupVList();
    new MythListButtonItem(vbuttons, "Hello");
    new MythListButtonItem(vbuttons, "Goodbye");  
    new MythListButtonItem(vbuttons, "Really Really Really Really Long");
    new MythListButtonItem(vbuttons, "A B C D");
}

TestWindow::~TestWindow(void)
{
}

void TestWindow::setupVList(void)
{

    /* place below the first list */
    QRect r(pad, listHeight + 2*pad, listWidth, listHeight);
    cout << "(" << r.x() << ", " << r.y() << ")\t" << r.width() << " x " 
	 << r.height() << endl;

    vbuttons = new MythListButton(this, "vlist", r, true, true);

    MythFontProperties fontProp;
    fontProp.face = CreateFont("Arial", 24, QFont::Bold);
    fontProp.color = QColor(Qt::white);
    fontProp.hasShadow = true;
    fontProp.shadowOffset = NormPoint(QPoint(4, 4));
    fontProp.shadowColor = QColor(Qt::black);
    fontProp.shadowAlpha = 64;

    vbuttons->SetFontActive(fontProp);

    fontProp.color = Qt::darkGray;

    vbuttons->SetFontInactive(fontProp);
    vbuttons->SetSpacing(NormX(10));
    vbuttons->SetMargin(NormX(6));
    vbuttons->SetDrawFromBottom(true);
    vbuttons->SetTextFlags(Qt::AlignCenter);
}

void TestWindow::setupHList(void)
{


    QRect prect = GetMythMainWindow()->GetUIScreenRect();
    QRect r(pad, pad, listWidth, listHeight);

    hbuttons = new MythHorizListButton(this,"hlist",r,true,true, 3);

    MythFontProperties fontProp;
    fontProp.face = CreateFont("Arial", 24, QFont::Bold);
    fontProp.color = QColor(Qt::white);
    fontProp.hasShadow = true;
    fontProp.shadowOffset = NormPoint(QPoint(4, 4));
    fontProp.shadowColor = QColor(Qt::black);
    fontProp.shadowAlpha = 64;

    hbuttons->SetFontActive(fontProp);

    fontProp.color = Qt::darkGray;

    hbuttons->SetFontInactive(fontProp);
    hbuttons->SetSpacing(NormX(10));
    hbuttons->SetMargin(NormX(6));
    hbuttons->SetDrawFromBottom(true);
}

void TestWindow::toggleActive(void)
{
    focused->SetActive(false);
    focused = (focused == vbuttons) ? hbuttons : vbuttons;
    focused->SetActive(true);
}

bool TestWindow::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    if (GetMythMainWindow()->TranslateKeyPress("qt", e, actions))
    {
        for (size_t i = 0; (i < actions.size()) && !handled; i++)
        {
            QString action = actions[i];
            if (action == "ESCAPE")
                QApplication::exit();
            else if (action == "SELECT")
                toggleActive();
            else if (action == "UP")
            {
                if (focused == vbuttons)
                    focused->MoveUp();
            }
            else if (action == "DOWN")
            {
                if (focused == vbuttons)
                    focused->MoveDown();
            }
            else if (action == "LEFT")
            {
                if (focused == hbuttons)
                    focused->MoveUp();
            }
            else if (action == "RIGHT")
            {
                if (focused == hbuttons)
                    focused->MoveDown();
            }
        }
    }

    return true;
}


#endif /* BTNLISTTEST_CPP */
