#include <QApplication>
#include <QDomDocument>
#include <QString>
#include <QMap>
#include <QStringList>
#include <QTextStream>
#include <QFile>
#include <QDir>
#include <QRegExp>

#include <iostream>
#include <cstdlib>

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
    return QString();
}

QString indir;
QString outfilebase;
QFile fstringout;
QTextStream fdataout;
QMap<QString, QFile *> transFiles;
QString laststring;
int totalstringCount = 0;
int stringCount = 0;
QStringList strings;

void parseElement(QDomElement &element)
{
    laststring.clear();

    for (QDomNode child = element.firstChild(); !child.isNull();
         child = child.nextSibling())
    {
        QDomElement info = child.toElement();
        if (!info.isNull())
        {
            if (info.tagName() == "value" || // UI theme
                info.tagName() == "template" || // UI theme
                info.tagName() == "helptext" || // UI theme
                info.tagName() == "text" || // Menu theme
                info.tagName() == "description") // Menu theme
            {
                if (info.attribute("lang", "").isEmpty())
                {
                    laststring = getFirstText(info);
                    if (!laststring.trimmed().isEmpty())
                    {
                        // Escape quotes
                        laststring.replace("\"", QString("\\\""));
                        // Escape xml-escaped newline
                        laststring.replace("\\n", QString("<newline>"));

                        // Remove newline whitespace added by
                        // xml formatting
                        QStringList lines = laststring.split('\n');
                        QStringList::iterator lineIt;
                        for (lineIt = lines.begin(); lineIt != lines.end();
                             ++lineIt)
                        {
                            (*lineIt) = (*lineIt).trimmed();
                        }
                        laststring = lines.join(" ");

                        laststring.replace(QString("<newline>"), QString("\\n"));
                        
                        if (!strings.contains(laststring))
                            strings << laststring;
                        ++stringCount;
                        ++totalstringCount;
                    }
                }
//                 else
//                 {
//                     QString language = info.attribute("lang", "").toLower();
// 
//                     if (!transFiles.contains(language))
//                     {
//                         QFile *tmp = new QFile(outfilebase + '_' + language + ".ts");
//                         if (!tmp->open(IO_WriteOnly))
//                         {
//                             cerr << "couldn't open language file\n";
//                             exit(-1);
//                         }
// 
//                         transFiles[language] = tmp;
// 
//                         QTextStream dstream(transFiles[language]);
//                         dstream << "</context>\n"
//                                 << "<context>\n"
//                                 << "    <name>ThemeUI</name>\n";
//                     }
// 
//                     QTextStream dstream(transFiles[language]);
// 
//                     dstream << "    <message>\n"
//                             << "        <source>" << laststring.utf8() << "</source>\n"
//                             << "        <translation>" << getFirstText(info).utf8() << "</translation>\n"
//                             << "    </message>\n";
//                 }
            }
            else
                parseElement(info);
        }
    }
}

void parseDirectory(QString dir)
{
    QDir themeDir(dir);

    cout << "Searching directory: " << qPrintable(themeDir.path()) << endl;
    
    themeDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    themeDir.setSorting(QDir::DirsFirst);
    
    QDomDocument doc;
    QFileInfoList themeFiles = themeDir.entryInfoList();
    QFileInfoList::const_iterator it;
    for (it = themeFiles.begin(); it != themeFiles.end(); ++it)
    {
        if ((*it).isDir())
        {
            parseDirectory((*it).filePath());
            continue;
        }

//         if ((themeDir.dirName() != "default") &&
//             (themeDir.dirName() != "default-wide") &&
//             !themeDir.exists("themeinfo.xml"))
//             return;

        if ((*it).suffix() != "xml")
            continue;

        cout << "  Found: " << qPrintable((*it).filePath()) << endl;
        
        QFile fin((*it).absoluteFilePath());

        if (!fin.open(QIODevice::ReadOnly))
        {
            cerr << "can't open " << qPrintable((*it).absoluteFilePath()) << endl;
            continue;
        }

        QString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;

        if (!doc.setContent(&fin, false, &errorMsg, &errorLine, &errorColumn))
        {
            cerr << "Error parsing: " << qPrintable((*it).absoluteFilePath()) << endl;
            cerr << "at line: " << errorLine << "  column: "
                 << errorColumn << endl;
            cerr << qPrintable(errorMsg) << endl;
            fin.close();
            continue;
        }

        fin.close();

        stringCount = 0;
        
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
        
        cout << "    Contains " << stringCount << " total strings" << endl;
    }

}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv, false);

    if (a.argc() < 2)
    {
        cerr << "You must specify at least a starting directory." << endl;
        a.exit(-1);
    }

    indir = a.argv()[1];
    if (a.argc() == 3)
        outfilebase = a.argv()[2];
    else
        outfilebase = indir;

    if (indir.isEmpty() || outfilebase.isEmpty())
    {
        cerr << "no filenames\n";
        exit(-1);
    }

    QDir themeDir(indir);
    if (!themeDir.exists())
    {
        cerr << "Starting directory does not exist\n";
        exit(-1);
    }

    parseDirectory(indir);

    fstringout.setFileName(outfilebase + '/' + "themestrings.h");

    if (!fstringout.open(QIODevice::WriteOnly))
    {
        cerr << "can't open " << qPrintable(outfilebase) << " for writing\n";
        exit(-1);
    }

    fdataout.setDevice(&fstringout);

    fdataout << QString("void strings_null() {\n");

    QStringList::const_iterator strit;
    for (strit = strings.begin(); strit != strings.end(); ++strit)
    {
        fdataout << QString("    ThemeUI::tr(\"%1\");\n")
                            .arg((*strit).toUtf8().data());
    }
    
    fdataout << QString("}\n");
    fstringout.close();

//    QMap<QString, QFile *>::Iterator it;
//    for (it = transFiles.begin(); it != transFiles.end(); ++it)
//    {
//        it.value()->close();
//    }

    cout << endl;
    cout << "---------------------------------------" << endl;
    cout << "Found " << totalstringCount << " total strings" << endl;
    cout << strings.count() << " unique" << endl;

    return 0;
}

