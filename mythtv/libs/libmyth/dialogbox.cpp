#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qapplication.h>

#include "dialogbox.h"
#include "mythcontext.h"

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

    setFont(QFont("Arial", (int)(context->GetBigFontSize() * hmult), 
            QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QLabel *maintext = new QLabel(text, this);
    maintext->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);

    box = new QVBoxLayout(this, (int)(20 * wmult));

    box->addWidget(maintext, 1);

    checkbox = NULL;
    if (checkboxtext)
    {
        checkbox = new QCheckBox(checkboxtext, this);
        box->addWidget(checkbox, 0);
    }

    buttongroup = new QButtonGroup(0);
  
    if (checkbox)
        buttongroup->insert(checkbox);
    connect(buttongroup, SIGNAL(clicked(int)), this, SLOT(buttonPressed(int)));
}

void DialogBox::AddButton(const QString &title)
{
    QPushButton *button = new QPushButton(title, this);
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
