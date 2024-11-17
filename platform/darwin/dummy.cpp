// main.cpp
#include <QApplication>
#include <QLabel>

int main(int argc, char** argv)
{
    QApplication app(argc,argv);
    QLabel lbl("Hello");
    lbl.show();
    return app.exec();
}
