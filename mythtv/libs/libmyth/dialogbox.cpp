#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qapplication.h>

#include "dialogbox.h"
#include "mythcontext.h"
#include "mythwidgets.h"

DialogBox::DialogBox(MythContext *context, const QString &text, 
                     const char *checkboxtext,
                     QWidget *parent, const char *name)
         : QDialog(parent, name)
{
    int screenheight = 0, screenwidth = 0;
    float wmult = 0, hmult = 0;

    context->GetScreenSettings(screenwidth, wmult, screenheight, hmult);

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(context->GetMediumFontSize() * hmult), 
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    context->ThemeWidget(this);

    QLabel *maintext = new QLabel(text, this);
    maintext->setBackgroundOrigin(WindowOrigin);
    maintext->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);

    box = new QVBoxLayout(this, (int)(20 * wmult));

    box->addWidget(maintext, 1);

    checkbox = NULL;
    if (checkboxtext)
    {
        checkbox = new QCheckBox(checkboxtext, this);
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
    MyPushButton *button = new MyPushButton(title, this);
    button->setBackgroundMode(X11ParentRelative);

    buttongroup->insert(button);

    box->addWidget(button, 0);
}

void DialogBox::Show()
{
    showFullScreen();
    raise();
    setActiveWindow();
}

void DialogBox::buttonPressed(int which)
{
    if (buttongroup->find(which) == checkbox)
    {
    }
    else
        done(which+1);
}
