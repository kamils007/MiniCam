#include "ArdSolidImport.h"
#include "WholeBodyImport.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProgressDialog>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <stdexcept>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
[[noreturn]] void fail(const QString& text) { throw std::runtime_error(text.toUtf8().constData()); }
QByteArray read(const QString& path, qint64 limit) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size()<=0 || file.size()>limit)
        fail("Nie można odczytać danych konwersji ARD: " + QFileInfo(path).fileName());
    const auto data=file.readAll();
    if(file.error()!=QFileDevice::NoError)fail(file.errorString());
    return data;
}
void write(const QString& path,const QByteArray& bytes) {
    QSaveFile file(path);
    if(!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly)
       || file.write(bytes)!=bytes.size() || !file.commit())fail("Nie można przygotować odczytu ARD.");
}
QString sha(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}
class ImportProgress final : public QProgressDialog {
public:
    explicit ImportProgress(QWidget* parent):QProgressDialog(
        "Konwersja bryły z ARD…\nPozostaw ALPHACAM zamknięty do zakończenia.",QString(),0,0,parent) {
        setWindowTitle("Otwórz ARD");setWindowModality(Qt::ApplicationModal);
        setCancelButton(nullptr);setWindowFlag(Qt::WindowCloseButtonHint,false);
        setMinimumDuration(0);setAutoClose(false);
    }
    // The native worker must finish its license cleanup, including on error.
    void reject() override {}
};
}

camcore::ImportedModel readArdSolid(const QByteArray& source,
                                   const camcore::ArdDocument& document,QWidget* parent)
{
#ifndef Q_OS_WIN
    Q_UNUSED(source);Q_UNUSED(document);Q_UNUSED(parent);
    fail("Odczyt brył z ARD wymaga Windows i skonfigurowanej instalacji ALPHACAM 2025.4.");
#endif
    if(document.solids.size()!=1 || document.solids.front().bodyCount!=1)
        fail("Ten plik ARD zawiera wiele brył. Ich odczyt nie jest jeszcze obsługiwany.");
    QSettings settings("MiniCAM","MiniCAM");
    const QDir installed(qEnvironmentVariable("LOCALAPPDATA")+"/Programs/Python");
    QString pythonDefault;
    for(const auto& folder:installed.entryList({"Python3*"},QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name|QDir::Reversed)) {
        const auto path=installed.filePath(folder+"/python.exe");
        if(QFileInfo(path).isFile()){pythonDefault=path;break;}
    }
    if(pythonDefault.isEmpty())pythonDefault=QStandardPaths::findExecutable("python");
    const QString python=settings.value("alphacam/python",pythonDefault).toString();
    const QString root=settings.value("alphacam/root","C:/Program Files/Hexagon/ALPHACAM 2025").toString();
    const QString solidlink=settings.value("alphacam/solidlink",
        "C:/Program Files/Common Files/Hexagon/SolidLink 2025.4/code/bin/SolidLink.exe").toString();
    const QDir bin=QFileInfo(solidlink).absoluteDir();
    if(!QFileInfo(python).isFile() || !QFileInfo(QDir(root).filePath("CLS/LicManaged.dll")).isFile()
       || !QFileInfo(bin.filePath("SLInterop.dll")).isFile()
       || !QFileInfo(bin.filePath("SPAIInterop.dll")).isFile()
       || !QFileInfo(bin.filePath("SPAIInteropC.dll")).isFile())
        fail("Brakuje składników konwersji ARD. Sprawdź ścieżki Python, ALPHACAM i SolidLink w ustawieniach ALPHACAM w menu Plik.");
    const QString jobs=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/ard-import/jobs";
    const QString job=QDir(jobs).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if(!QDir().mkpath(job))fail("Nie można utworzyć folderu odczytu ARD.");
    try {
        QTemporaryDir scripts;
        if(!scripts.isValid())fail("Nie można przygotować składników odczytu ARD.");
        const QStringList files{"translator_trace/bulk_conversion_probe.py", "translator_trace/solidlink_bulk_worker.py",
            "ard_import/read_ard_body.py", "step_ard/feature_cls_session_14.ps1", "step_ard/feature_cls_invocation.cs"};
        for(const auto& name:files) {
            const QString target=name=="ard_import/read_ard_body.py" ? "ard_payload_reader.py" : QFileInfo(name).fileName();
            write(scripts.filePath(target),read(":/ard-import/"+name,8*1024*1024));
        }
        const QString input=QDir(job).filePath("source.ard"),out=QDir(job).filePath("result");
        write(input,source);
        ImportProgress progress(parent);QProcess process;
        process.setWorkingDirectory(job);
        process.setStandardOutputFile(QDir(job).filePath("worker_stdout.txt"));
        process.setStandardErrorFile(QDir(job).filePath("worker_stderr.txt"));
#ifdef Q_OS_WIN
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args){args->flags|=CREATE_NO_WINDOW;});
#endif
        QObject::connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&progress,[&](int,QProcess::ExitStatus){progress.done(0);});
        QObject::connect(&process,&QProcess::errorOccurred,&progress,[&](QProcess::ProcessError error){if(error==QProcess::FailedToStart)progress.done(0);});
        process.start(python,{"-I","-u",scripts.filePath("bulk_conversion_probe.py"),"--ard",input,
                              "--bin",bin.absolutePath(),"--root",root,"--out",out});
        if(process.state()!=QProcess::NotRunning)progress.exec();
        if(process.error()==QProcess::FailedToStart)fail("Nie można uruchomić odczytu ARD: "+process.errorString());
        if(process.exitStatus()!=QProcess::NormalExit)fail("Proces odczytu ARD zakończył się awarią.");
        const auto reportBytes=read(QDir(out).filePath("bulk_result.json"),8*1024*1024);
        const auto report=QJsonDocument::fromJson(reportBytes).object();
        if(process.exitCode()!=0 || !report["native_conversion_succeeded"].toBool())
            fail("Nie udało się przekonwertować bryły ARD. "+report["error"].toString());
        const auto payload=QByteArray::fromStdString(document.solids.front().parasolid);
        return ardimport::modelFromWholeBodyResult(reportBytes,QDir(out).filePath("converted.step"),
                                                   sha(source),sha(payload));
    }catch(const std::exception& e){fail(QString::fromUtf8(e.what())+"\n\nRaport odczytu: "+job);}
}
