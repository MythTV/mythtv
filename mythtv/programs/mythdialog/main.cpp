#include <qapplication.h>

#include "dialogbox.h"

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    if (argc < 3)
    {
        printf("Not enough args to run with\n");
        return 0;
    }

    DialogBox *diag = new DialogBox(argv[1]);
    a.setMainWidget(diag);

    for (int i = 2; i < argc; i++)
    {
        diag->AddButton(argv[i]);
    }

    diag->Show();

    int result = diag->exec();

    printf("result was %d\n", result);

    return (result);
}
