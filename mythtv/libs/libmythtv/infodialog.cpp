#include "infodialog.h"

InfoDialog::InfoDialog(QWidget *parent, const char *name)
          : QDialog(parent, name, TRUE)
{
    setGeometry(0, 0, 800, 600);
    setFixedWidth(800); 
    setFixedHeight(600);
    showFullScreen();
}
