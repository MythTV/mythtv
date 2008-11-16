#include <QApplication>
#include <QDomDocument>
#include <QString>
#include <QMap>
#include <QStringList>
#include <q3textstream.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <qregexp.h>

using namespace std;

QString getFirstText(QDomElement element)
{
    for (QDomNode dname = element.firstChild(); !dname.isNull();
         dname = dname.nextSibling())
    {
        QDomText t = dname.toText();
        if (!t.isNull())
            return t.data();
    }
    return "";
}

QString infile = "";
//QString outfilebase = "";
QFile fstringout;
Q3TextStream fdataout;
//QMap<QString, QFile *> transFiles;
QString laststring = "";

void parseElement(QDomElement &element)
{
    laststring = "";

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "value")
            {
                if (info.attribute("lang", "") == "")
                {
                    laststring = getFirstText(info);
                    laststring.replace(QRegExp("\""), QString("\\\""));
                    fdataout << QString("    ThemeUI::tr(\"%1\");\n")
                                            .arg(laststring.toUtf8().data());
                }
                else
                {
                    QString language = info.attribute("lang", "").lower();
/*
                    if (!transFiles.contains(language))
                    {
                        QFile *tmp = new QFile(outfilebase + "_" + language + ".ts");
                        if (!tmp->open(IO_WriteOnly))
                        {
                            cerr << "couldn't open language file\n";
                            exit(-1);
                        }

                        transFiles[language] = tmp;

                        QTextStream dstream(transFiles[language]);
                        dstream << "</context>\n"
                                << "<context>\n"
                                << "    <name>ThemeUI</name>\n";
                    }

                    QTextStream dstream(transFiles[language]);

                    dstream << "    <message>\n"
                            << "        <source>" << laststring.utf8() << "</source>\n"
                            << "        <translation>" << getFirstText(info).utf8() << "</translation>\n"
                            << "    </message>\n";
*/
                }
            }
            else
                parseElement(info);
        }
    }
}


int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);

    if (a.argc() != 2)
    {
        cerr << "wrong num of args\n";
        exit(-1);
    }

    infile = a.argv()[1];
    //outfilebase = a.argv()[2];

    if (infile == "")// || outfilebase == "")
    {
        cerr << "no filenames\n";
        exit(-1);
    }

    QDomDocument doc;
    QFile fin(infile);

    if (!fin.open(QIODevice::ReadOnly))
    {
//         cerr << "can't open " << infile << endl;
        exit(-1);
    }

    QFileInfo ininfo(infile);

    fstringout.setName("themestrings.h");

    if (!fstringout.open(QIODevice::WriteOnly))
    {
//         cerr << "can't open " << ininfo.fileName() << " for writing\n";
        exit(-1);
    }

    fdataout.setDevice(&fstringout);

    fdataout << QString("void strings_null() {\n");

    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!doc.setContent(&fin, false, &errorMsg, &errorLine, &errorColumn))
    {
//         cerr << "Error parsing: " << infile << endl;
//         cerr << "at line: " << errorLine << "  column: " << errorColumn << endl;
//         cerr << errorMsg << endl;
        fin.close();
        return -1;
    }

    fin.close();

    QDomElement docElem = doc.documentElement();
    QDomNode n = docElem.firstChild();
    while (!n.isNull())
    {
        QDomElement e = n.toElement();
        if (!e.isNull())
        {
            parseElement(e);
        }
        n = n.nextSibling();
    }

    fdataout << QString("}\n");
    fstringout.close();

//    QMap<QString, QFile *>::Iterator it;
//    for (it = transFiles.begin(); it != transFiles.end(); ++it)
//    {
//        it.data()->close();
//    }

    return 0;
}

