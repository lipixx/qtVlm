/**********************************************************************
qtVlm: Virtual Loup de mer GUI
Copyright (C) 2008 - Christophe Thomas aka Oxygen77

http://qtvlm.sf.net

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Original code: zyGrib: meteorological GRIB file viewer
Copyright (C) 2008 - Jacques Zaninetti - http://zygrib.free.fr

***********************************************************************/

#include <QApplication>
#include <QTextCodec>
#include <QTranslator>
#include <QTime>
#include <QLibraryInfo>
#include <QLocale>
#include <QDebug>
#include <QDir>

#include "MainWindow.h"
#include "settings.h"

#if 0 /*put 1 to force crash on assert, useful for debugging*/
void crashingMessageHandler(QtMsgType type, const char *msg)
{
    switch(type){
    case QtDebugMsg:
        fprintf(stderr,"Debug: %s\n", msg);
        break;
    case QtWarningMsg:
        fprintf(stderr,"Warning: %s\n", msg);
        break;
    case QtCriticalMsg:
        fprintf(stderr,"Critical: %s\n", msg);
        break;
    case QtFatalMsg:
        fprintf(stderr,"Fatal: %s\n", msg);
        __asm("int3");
        abort();
    }
}
int main(int argc, char *argv[])
{
    qInstallMsgHandler(crashingMessageHandler);
#else
int main(int argc, char *argv[])
{
#endif
    int currentExitCode=0;
    do{
    QApplication app(argc, argv);
    qsrand(QTime::currentTime().msec());
    QString appExeFolder=QApplication::applicationDirPath();
#ifdef __UNIX_QTVLM
    QString curDir=QDir::currentPath();
    qWarning() << "currentPath returns: " << curDir << "applicationDirPath returns: " << appExeFolder;
    if ( QString::compare(curDir,appExeFolder,Qt::CaseSensitive)!=0 )
    {
        QDir::setCurrent(appExeFolder);
        curDir=QDir::currentPath();
        qWarning() << "currentPath modified: " << curDir << "applicationDirPath returns: " << appExeFolder;
    }
#endif
    appExeFolder=QDir::currentPath();
    qWarning()<<"Current path"<<appExeFolder;
    //checks tree
    QDir dirCheck;
    QStringList appDirs;
    QString dirName;
    appDirs<<"icon"<<"img"<<"img/flags"<<"img/boats"<<"grib"<<"maps"<<"polar"<<"tr"<<"tracks";
    QStringListIterator dirIterator(appDirs);
    while (dirIterator.hasNext())
    {
        dirName=appExeFolder+"/"+dirIterator.next();
        dirCheck=QDir(dirName);
        if (!dirCheck.exists())
            dirCheck.mkdir(dirName);
        //qWarning() << "Checking folder: " << dirName;
    }
    QTextCodec::setCodecForTr(QTextCodec::codecForName("utf8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));
    Settings::initSettings();
    double fontInc=Settings::getSetting("defaultFontSizeInc",0).toDouble();
    if(fontInc<-3 || fontInc>5)
    {
        fontInc=0;
        Settings::setSetting("defaultFontSizeInc",0);
    }
    QFont def(Settings::getSetting("defaultFontName",QApplication::font().family()).toString());
    def.setPointSizeF(8.25+fontInc);
    QApplication::setFont(def);

    QTranslator translator;
    QTranslator translatorQt;
    QString lang = Settings::getSetting("appLanguage", "none").toString();
    if (lang == "none") {  // first call
        qWarning() << "Setting default lang=fr";
        lang = "fr";
        Settings::setSetting("appLanguage", lang);
    }

    if (lang == "fr") {
        qWarning() << "Loading fr";
        QLocale::setDefault(QLocale("fr_FR"));
        translator.load( QString("qtVlm_") + lang,"tr/");
        translatorQt.load( QString("qt_fr"),"tr/");
        app.installTranslator(&translatorQt);
        app.installTranslator(&translator);
    }
    else if (lang == "en") {
        qWarning() << "Loading en";
        QLocale::setDefault(QLocale("en_US"));
        translator.load( QString("tr/qtVlm_") + lang);
        app.installTranslator(&translator);
    }
    app.setQuitOnLastWindowClosed(true);

    MainWindow win(800, 600);
    if(win.getRestartNeeded())
    {
        app.quit();
        continue;
    }
    if(win.getFinishStart())
    {
        win.show();

        app.installTranslator(NULL);

        currentExitCode= app.exec();
        break;
    }
    else
    {
        app.quit();
        currentExitCode= 0;
        break;
    }
    }while (true);
    return currentExitCode;
}
