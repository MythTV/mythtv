#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>

#include "menubox.h"

MenuBox::MenuBox(const char *text, QWidget *parent = 0, 
                 const char *name = 0)
       : QDialog(parent, name)
{
    setGeometry(0, 0, 800, 600);
    setFixedWidth(800);
    setFixedHeight(600);

    setFont(QFont("Arial", 40, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QLabel *maintext = new QLabel(text, this);
    maintext->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    maintext->setFont(QFont("Arial", 40, QFont::Bold));

    box = new QVBoxLayout(this, 20);

    box->addWidget(maintext, 0);

    buttongroup = new QButtonGroup(0);

    setFont(QFont("Arial", 16, QFont::Bold));

    connect(buttongroup, SIGNAL(clicked(int)), this, SLOT(buttonPressed(int)));
}

void MenuBox::AddButton(const char *title)
{
    QPushButton *button = new QPushButton(title, this);
    buttongroup->insert(button);

    box->addWidget(button, 2);
}

void MenuBox::Show()
{
    showFullScreen();
}

void MenuBox::buttonPressed(int which)
{
    done(which);
}
