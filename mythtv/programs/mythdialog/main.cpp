#include <qapplication.h>

#include "libmyth/dialogbox.h"
#include "libmyth/mythcontext.h"

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    if (argc < 3)
    {
        printf("Not enough args to run with\n");
        return 0;
    }

    MythContext *context = new MythContext();

    context->LoadQtConfig();

    DialogBox diag(context, argv[1]);
    a.setMainWidget(&diag);

    for (int i = 2; i < argc; i++)
    {
        diag.AddButton(argv[i]);
    }

    diag.Show();

    int result = diag.exec();

    delete context;

    return (result);
}
