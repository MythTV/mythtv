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
