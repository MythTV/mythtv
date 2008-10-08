#include <iostream>
using namespace std;

#include "dialogbox.h"
#include "mythcontext.h"
#include "mythwidgets.h"

#ifdef USING_MINGW
#undef DialogBox
#endif

#include <Q3ButtonGroup>
#include <QBoxLayout>

DialogBox::DialogBox(MythMainWindow *parent, const QString &text, 
                     const char *checkboxtext,
                     const char *name)
         : MythDialog(parent, name)
{
    QLabel *maintext = new QLabel(text, this);
    maintext->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    maintext->setWordWrap(true);

    box = new QVBoxLayout(this);
    box->setContentsMargins((int)(60 * wmult),(int)(60 * wmult),
                           (int)(60 * wmult),(int)(60 * wmult));
    box->setSpacing(0);
    box->addWidget(maintext, 1);

    checkbox = NULL;
    if (checkboxtext)
    {
        checkbox = new MythCheckBox(this);
        checkbox->setText(checkboxtext);
        box->addWidget(checkbox, 0);
    }

    buttongroup = new Q3ButtonGroup(0);
  
    if (checkbox)
        buttongroup->insert(checkbox);
    connect(buttongroup, SIGNAL(clicked(int)), this, SLOT(buttonPressed(int)));
}

void DialogBox::AddButton(const QString &title)
{
    MythPushButton *button = new MythPushButton(title, this);

    if (buttongroup->count() == 0 ||
        (checkbox && buttongroup->count() == 1))
    {
        button->setFocus();
    }

    buttongroup->insert(button);

    box->addWidget(button, 0);
}

void DialogBox::buttonPressed(int which)
{
    if (buttongroup->find(which) != checkbox)
        AcceptItem(which);
}
