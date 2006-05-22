#include <qapplication.h>

#include "test1.h"

#include "mythuiimage.h"
#include "mythuitext.h"
#include "mythmainwindow.h"
#include "mythlistbutton.h"
#include "mythdialogbox.h"
#include "mythfontproperties.h"
#include "mythcontext.h"
#include "myththemedmenu.h"

TestScreen1::TestScreen1(MythScreenStack *parent, const char *name)
           : MythScreenType(parent, name)
{
    MythFontProperties fontProp1;
    fontProp1.SetFace(CreateFont("Arial", 48, QFont::Bold));
    fontProp1.SetColor(QColor(Qt::white));
    fontProp1.SetShadow(true, NormPoint(QPoint(4, 4)), QColor(Qt::black), 64);

    QRect mainRect = GetMythMainWindow()->GetUIScreenRect();
    MythUIText *main = new MythUIText("Welcome to MythTV", fontProp1, mainRect,
                                      mainRect, this, "welcome");
    main->SetJustification(Qt::AlignCenter);

    MythFontProperties fontProp;
    fontProp.SetFace(CreateFont("Arial", 14));
    fontProp.SetColor(QColor(Qt::white));
    fontProp.SetShadow(true, NormPoint(QPoint(1, 1)), QColor(Qt::black), 64);

    QRect textRect = NormRect(QRect(0, mainRect.height() * 2 / 3, 
                                    mainRect.width(), mainRect.height() / 3));
    MythUIText *text = new MythUIText("[ Press any key to continue. ]",
                                      fontProp, textRect, textRect, this,
                                      "label");
    text->SetJustification(Qt::AlignCenter);
    text->CycleColor(Qt::white, "#F7862B", 200);
}

bool TestScreen1::keyPressEvent(QKeyEvent *e)
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
                QApplication::exit();
            }
            else
            {
                Launch1();
            }
        }
    }

    return true;
}

void TestScreen1::customEvent(QCustomEvent *e)
{
    if (e->type() == kMythDialogBoxCompletionEventType)
    {
        DialogCompletionEvent *dce;
        dce = reinterpret_cast<DialogCompletionEvent *>(e);

        QString id = dce->GetId();
        int result = dce->GetResult();

        if (id == "FIRST_DIALOG")
        {
            if (result == 0)
                Launch2();
            else if (result == 1)
                LaunchMenu();
        }
        else if (id == "SECOND_DIALOG")
        {
            if (result == 0)
                Launch3();
            else
                Launch1();
        }
        else if (id == "THIRD_DIALOG")
        {
            if (result == 0)
                Launch4();
            else
                Launch2();
        }
        else if (id == "FOURTH_DIALOG")
        {
            if (result == 0)
                Launch5();
            else
                Launch3();
        }
        else if (id == "FIFTH_DIALOG")
        {
            if (result == 0)
                Launch6();
            else
                Launch4();
        }
        else if (id == "SIXTH_DIALOG")
        {
            if (result == 0)
                Launch7();
            else
                Launch5();
        }
        else if (id == "SEVENTH_DIALOG")
        {
            if (result == 0)
                Launch8();
            else
                Launch6();
        }
        else if (id == "FINAL_DIALOG")
        {
            if (result != 0)
                Launch7();
        }
    }
}

void TestScreen1::LaunchMenu(void)
{
    QString themename = gContext->GetSetting("Theme", "blue");
    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {
        printf("no theme!\n");
        return;
    }

    MythThemedMenu *menu = new MythThemedMenu(themedir.local8Bit(), 
                                              "mainmenu.xml", m_ScreenStack, 
                                              "mainmenu");
    if (menu->foundTheme())
    {
        menu->setKillable();
        m_ScreenStack->AddScreen(menu);
    }
    else
    {
        delete menu;
    }
}

void TestScreen1::Launch1(void)
{
    QString diagText = "Hi!\nThis is a test of the new rendering backend for "
                       "MythTV.\nHave fun or something.";
    MythDialogBox *diag = new MythDialogBox(diagText, m_ScreenStack, "startdialog");
    diag->Create();
    diag->SetReturnEvent(this, "FIRST_DIALOG");
    diag->AddButton("Ok, let's continue this insanity");
    diag->AddButton("Let's see the menu (no return)");
    diag->AddButton("No, I'm scared, I want to go home");

    m_ScreenStack->AddScreen(diag);
}

void TestScreen1::Launch2(void)
{
    QString diagText = "In this new rendering backend, multiple drawing "
                       "methods can be implemented.  For instance, OpenGL and "
                       "Qt (Xlib / XRENDER) paint engines have been written. ";
    MythDialogBox *diag = new MythDialogBox(diagText, m_ScreenStack, "2dialog");
    diag->Create();
    diag->SetReturnEvent(this, "SECOND_DIALOG");
    diag->AddButton("Ok, but what can it do?");

    m_ScreenStack->AddScreen(diag);
}

void TestScreen1::Launch3(void)
{
    QString diagText = "Well, for instance, the OpenGL engine can do "
                       "arbitrary alpha blending.  Notice the transition "
                       "between dialogs - the old one fades out while "
                       "the new one fades in.  The MythTV logo in the "
                       "lower left is also continually changing its "
                       "transparency.";
    MythDialogBox *diag = new MythDialogBox(diagText, m_ScreenStack, "3dialog");
    diag->Create();
    diag->SetReturnEvent(this, "THIRD_DIALOG");
    diag->AddButton("Neat, but what else?");

    m_ScreenStack->AddScreen(diag);
}

void TestScreen1::Launch4(void)
{
    QString diagText = "Well, how about movement?";
    MythDialogBox *diag = new MythDialogBox(diagText, m_ScreenStack, "4dialog");
    diag->Create();
    diag->SetReturnEvent(this, "FOURTH_DIALOG");
    diag->AddButton("Anything else?");

    TestMove *tm = new TestMove(diag, "move");
    tm->SetPosition(NormPoint(QPoint(0, 150)));

    m_ScreenStack->AddScreen(diag);
}

void TestScreen1::Launch5(void)
{
    QString diagText = "Animation is also supported.. (when using OpenGL)";
    MythDialogBox *diag = new MythDialogBox(diagText, m_ScreenStack, "5dialog");
    diag->Create();
    diag->SetReturnEvent(this, "FIFTH_DIALOG");
    diag->AddButton("Sweet.");

    MythUIImage *aniwait = new MythUIImage("images/watch%1.png", 1, 16,
                                           30, diag, "ani");
    aniwait->SetPosition(NormPoint(QPoint(350, 275)));
    aniwait->Load();

    m_ScreenStack->AddScreen(diag);
}

void TestScreen1::Launch6(void)
{
    QString diagText = "All eye candy can be disabled to handle "
                       "slower systems and drawing methods, and all effects "
                       "can be combined easily. ";
    MythDialogBox *diag = new MythDialogBox(diagText, m_ScreenStack, "6dialog");
    diag->Create();
    diag->SetReturnEvent(this, "SIXTH_DIALOG");
    diag->AddButton("Ok, sounds great.");

    m_ScreenStack->AddScreen(diag);
}

void TestScreen1::Launch7(void)
{
    QString diagText = "The idea behind the new UI backend is not to "
                       "overwhelm the user with too much eyecandy, "
                       "but to enable subtle effects that add to the "
                       "overall user experience.";
    MythDialogBox *diag = new MythDialogBox(diagText, m_ScreenStack, "7dialog");
    diag->Create();
    diag->SetReturnEvent(this, "SEVENTH_DIALOG");
    diag->AddButton("Ok.");

    m_ScreenStack->AddScreen(diag);
}

void TestScreen1::Launch8(void)
{
    QString diagText = "That concludes this short tour of the new MythTV "
                       "UI backend.";
    MythDialogBox *diag = new MythDialogBox(diagText, m_ScreenStack, "8dialog");
    diag->Create();
    diag->SetReturnEvent(this, "FINAL_DIALOG");
    diag->AddButton("Good-bye");

    for (int i = 0; i < 20; i++)
        diag->AddButton(QString("Test text item #%1").arg(i));
    
    m_ScreenStack->AddScreen(diag);
}

////////////////////////////////////////////////////////////////////////////

TestMove::TestMove(MythUIType *parent, const char *name)
        : MythUIType(parent, name)
{
    MythUIImage *image;

    image = new MythUIImage("images/tv.png", this, "tv image");
    image->SetPosition(NormPoint(QPoint(30, 30)));
    image->Load();

    QFont fontFace = CreateFont("Arial", 28, QFont::Bold);

    MythFontProperties fontProp;
    fontProp.SetFace(fontFace);
    fontProp.SetColor(QColor(qRgba(255, 255, 255, 255)));

    new MythUIText("Hello!", fontProp,
                   NormRect(QRect(0, 0, 200, 200)),
                   NormRect(QRect(0, 0, 200, 200)),
                   this, "text test");

    MoveTo(NormPoint(QPoint(500, 0)), NormPoint(QPoint(1, 0)));
    connect(this, SIGNAL(FinishedMoving()), SLOT(moveDone()));

    position = 0;
}

void TestMove::moveDone()
{
    switch (position)
    {
        case 0: MoveTo(NormPoint(QPoint(0, 0)), NormPoint(QPoint(-1, 0))); break;
        case 1: MoveTo(NormPoint(QPoint(500, 0)), NormPoint(QPoint(1, 0))); break;
        default: break;
    }

    position++;
    if (position > 1)
        position = 0;
}

