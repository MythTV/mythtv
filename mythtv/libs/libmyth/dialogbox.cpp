#include <qlayout.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <qcursor.h>

#include "dialogbox.h"

DialogBox::DialogBox(const QString &text, QWidget *parent = 0, 
                     const char *name = 0)
         : QDialog(parent, name)
{
    setGeometry(0, 0, 800, 600);
    setFixedWidth(800);
    setFixedHeight(600);

    setFont(QFont("Arial", 16, QFont::Bold));
    setCursor(QCursor(Qt::BlankCursor));

    QLabel *maintext = new QLabel(text, this);
    maintext->setAlignment(Qt::WordBreak | Qt::AlignLeft | Qt::AlignTop);

    box = new QVBoxLayout(this, 20);

    box->addWidget(maintext, 1);

    buttongroup = new QButtonGroup(0);

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
}

void DialogBox::buttonPressed(int which)
{
    done(which);
}
