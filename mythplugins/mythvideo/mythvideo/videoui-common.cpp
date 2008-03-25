#include <mythtv/uitypes.h>

#include "metadata.h"
#include "videoutils.h"

void ShowCastDialog(MythMainWindow *wparent, Metadata &item)
{
    MythPopupBox *castbox = new MythPopupBox(wparent);

    MythListBox *mlb = new MythListBox(castbox);

    mlb->insertStringList(GetCastList(item));
    castbox->addWidget(mlb, true);

    QAbstractButton *okButton = castbox->addButton(QObject::tr("Ok"));
    okButton->setFocus();

    castbox->ExecPopup();
    castbox->deleteLater();
}
