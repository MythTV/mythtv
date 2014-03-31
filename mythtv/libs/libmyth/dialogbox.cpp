#include <iostream>
using namespace std;

#include "dialogbox.h"
#include "mythwidgets.h"

#ifdef _WIN32
#undef DialogBox
#endif

#include <QButtonGroup>
#include <QBoxLayout>

DialogBox::DialogBox(MythMainWindow *parent, const QString &text, 
                     const char *checkboxtext,
                     const char *name)
         : MythDialog(parent, name)
{
    MythLabel *maintext = new MythLabel(text, this);
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

    buttongroup = new QButtonGroup();
  
    if (checkbox)
        buttongroup->addButton(checkbox, -2);
    connect(buttongroup, SIGNAL(buttonClicked(int)),
            this,        SLOT(  buttonPressed(int)));
}

void DialogBox::AddButton(const QString &title)
{
    MythPushButton *button = new MythPushButton(title, this);

    if (buttongroup->buttons().empty() ||
        (checkbox && buttongroup->buttons().size() == 1))
    {
        button->setFocus();
    }

    int id = buttongroup->buttons().size();
    id = (checkbox) ? id - 1 : id;
    buttongroup->addButton(button, id);

    box->addWidget(button, 0);
}

void DialogBox::buttonPressed(int which)
{
    if (buttongroup->button(which) != checkbox)
        AcceptItem(which);
}
