#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qapplication.h>

#include "dialogbox.h"
#include "settings.h"

extern Settings *globalsettings;

DialogBox::DialogBox(const QString &text, const char *checkboxtext,
                     QWidget *parent, const char *name)
         : QDialog(parent, name)
{
    int screenheight = QApplication::desktop()->height();
    int screenwidth = QApplication::desktop()->width();

    if (globalsettings->GetNumSetting("GuiWidth") > 0)
        screenwidth = globalsettings->GetNumSetting("GuiWidth");
    if (globalsettings->GetNumSetting("GuiHeight") > 0)
        screenheight = globalsettings->GetNumSetting("GuiHeight");

    float wmult = screenwidth / 800.0;
    float hmult = screenheight / 600.0;

    setGeometry(0, 0, screenwidth, screenheight);
    setFixedSize(QSize(screenwidth, screenheight));

    setFont(QFont("Arial", (int)(16 * hmult), QFont::Bold));
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
