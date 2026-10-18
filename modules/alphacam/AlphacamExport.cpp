#include "AlphacamExport.h"
#include "ModelExport.h"
#include <Standard_Failure.hxx>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QVBoxLayout>
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace alphacam {
namespace {
[[noreturn]] void fail(const QString& text)
{
    throw std::runtime_error(text.toUtf8().constData());
}

struct Configuration {
    QString python, runtime, root, solidlink;
};

QString pythonDefault()
{
    const QDir installed(qEnvironmentVariable("LOCALAPPDATA") + "/Programs/Python");
    for (const QString& folder : installed.entryList({"Python3*"}, QDir::Dirs | QDir::NoDotAndDotDot,
                                                    QDir::Name | QDir::Reversed)) {
        const QString path = installed.filePath(folder + "/python.exe");
        if (QFileInfo(path).isFile())
            return path;
    }
    return QStandardPaths::findExecutable("python");
}

Configuration configuration()
{
    QSettings settings("MiniCAM", "MiniCAM");
    return {settings.value("alphacam/python", pythonDefault()).toString(),
            settings.value("alphacam/runtime", QCoreApplication::applicationDirPath() + "/alphacam-export").toString(),
            settings.value("alphacam/root", "C:/Program Files/Hexagon/ALPHACAM 2025").toString(),
            settings.value("alphacam/solidlink", "C:/Program Files/Common Files/Hexagon/SolidLink 2025.4/code/bin/SolidLink.exe").toString()};
}

QString missing(const Configuration& c)
{
    if (!QFileInfo(c.python).isFile())
        return "Wskaż plik python.exe z instalacji Python 3 x64.";
    if (!QFileInfo(QDir(c.runtime).filePath("step_ard/export_record.py")).isFile()
        || !QFileInfo(QDir(c.runtime).filePath("step_ard/data/1_test.ard")).isFile())
        return "Wskaż rozpakowany folder modułu alphacam-export.";
    if (!QFileInfo(QDir(c.root).filePath("Add-Ins/Feature/Feature.dll")).isFile()
        || !QFileInfo(QDir(c.root).filePath("Add-Ins/SolidImport/SolidImport.dll")).isFile())
        return "Wskaż folder instalacji ALPHACAM 2025.4.";
    if (!QFileInfo(c.solidlink).isFile())
        return "Wskaż plik SolidLink.exe z instalacji 2025.4.";
    return {};
}

QByteArray readFile(const QString& path, qint64 maximum)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 1 || file.size() > maximum)
        fail("Nie można odczytać wyniku eksportu: " + QDir::toNativeSeparators(path));
    const QByteArray data = file.readAll();
    if (data.size() != file.size())
        fail("Niepełny odczyt pliku: " + QDir::toNativeSeparators(path));
    return data;
}

QString hash(const QByteArray& data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

// Escape cannot reject this dialog while the native workers own a CLS session.
// Their existing internal deadlines and normal cleanup govern completion.
class ExportProgress : public QProgressDialog {
public:
    explicit ExportProgress(QWidget* parent)
        : QProgressDialog("Przygotowywanie bieżącej bryły…", QString(), 0, 0, parent)
    {
        setWindowTitle("Zapisz do ALPHACAM");
        setWindowModality(Qt::ApplicationModal);
        setCancelButton(nullptr);
        setWindowFlag(Qt::WindowCloseButtonHint, false);
        setMinimumDuration(0);
        setAutoClose(false);
    }
    void reject() override {}
};
}

bool configure(QWidget* parent)
{
    const Configuration previous = configuration();
    QDialog dialog(parent);
    dialog.setWindowTitle("Ustawienia eksportu ALPHACAM");
    auto* layout = new QVBoxLayout(&dialog);
    auto* description = new QLabel("Eksport brył: ALPHACAM i SolidLink 2025.4, Python 3 x64.\n"
                                  "Na czas eksportu zamknij ALPHACAM.", &dialog);
    layout->addWidget(description);
    auto* form = new QFormLayout;
    layout->addLayout(form);
    auto field = [&](const QString& title, const QString& value, bool directory, const QString& filter) {
        auto* row = new QHBoxLayout;
        auto* edit = new QLineEdit(value, &dialog);
        edit->setMinimumWidth(420);
        auto* browse = new QPushButton("…", &dialog);
        row->addWidget(edit);
        row->addWidget(browse);
        form->addRow(title, row);
        QObject::connect(browse, &QPushButton::clicked, &dialog, [&, edit, title, directory, filter] {
            const QString selected = directory
                ? QFileDialog::getExistingDirectory(&dialog, title, edit->text())
                : QFileDialog::getOpenFileName(&dialog, title, edit->text(), filter);
            if (!selected.isEmpty())
                edit->setText(QDir::fromNativeSeparators(selected));
        });
        return edit;
    };
    auto* python = field("Python x64", previous.python, false, "Python (python.exe);;Wszystkie pliki (*)");
    auto* runtime = field("Folder modułu", previous.runtime, true, {});
    auto* root = field("Folder ALPHACAM", previous.root, true, {});
    auto* solidlink = field("SolidLink", previous.solidlink, false, "SolidLink (SolidLink.exe)");
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const Configuration current{python->text().trimmed(), runtime->text().trimmed(),
                                    root->text().trimmed(), solidlink->text().trimmed()};
        const QString problem = missing(current);
        if (!problem.isEmpty()) {
            QMessageBox::information(&dialog, "Ustawienia ALPHACAM", problem);
            return;
        }
        QSettings settings("MiniCAM", "MiniCAM");
        settings.setValue("alphacam/python", QFileInfo(current.python).absoluteFilePath());
        settings.setValue("alphacam/runtime", QFileInfo(current.runtime).absoluteFilePath());
        settings.setValue("alphacam/root", QFileInfo(current.root).absoluteFilePath());
        settings.setValue("alphacam/solidlink", QFileInfo(current.solidlink).absoluteFilePath());
        dialog.accept();
    });
    return dialog.exec() == QDialog::Accepted;
}

std::string createSolidRecord(const camcore::ImportedModel& model, QWidget* parent)
{
#ifndef Q_OS_WIN
    Q_UNUSED(model);
    Q_UNUSED(parent);
    fail("Eksport bryły ALPHACAM wymaga Windows x64.");
#endif
    if (sizeof(void*) != 8)
        fail("Eksport bryły ALPHACAM wymaga MiniCam x64.");
    if (model.shape.IsNull())
        fail("Brak bryły do zapisania.");
    Configuration config = configuration();
    if (!missing(config).isEmpty()) {
        if (!configure(parent))
            fail("Anulowano konfigurację eksportu ALPHACAM.");
        config = configuration();
    }
    const QString jobs = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                       + "/alphacam/jobs";
    const QString job = QDir(jobs).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(job))
        fail("Nie można utworzyć katalogu eksportu ALPHACAM.");
    try {
        const QDir directory(job);
        const QString input = directory.filePath("current-model.step");
        // Always serialize the current OCCT shape, including edited placement
        // and colors. Never use the original import file as a shortcut.
        camcore::exportModel(model, input.toUtf8().toStdString());
        const QString stepHash = hash(readFile(input, 256LL * 1024 * 1024));
        const QJsonObject request{{"schema", "minicam-alphacam-request-v1"},
                                  {"step", input}, {"step_sha256", stepHash},
                                  {"alphacam_root", config.root}, {"solidlink_server", config.solidlink}};
        const QByteArray requestBytes = QJsonDocument(request).toJson();
        const QString requestPath = directory.filePath("request.json");
        const QString responsePath = directory.filePath("response.json");
        QSaveFile file(requestPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(requestBytes) != requestBytes.size() || !file.commit())
            fail("Nie można przygotować zlecenia eksportu.");

        ExportProgress progress(parent);
        progress.setLabelText("Konwersja bryły do ALPHACAM…\nPozostaw ALPHACAM zamknięty do zakończenia.");
        QProcess process;
        process.setWorkingDirectory(job);
        process.setStandardOutputFile(directory.filePath("worker_stdout.txt"));
        process.setStandardErrorFile(directory.filePath("worker_stderr.txt"));
#ifdef Q_OS_WIN
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif
        QObject::connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                         &progress, [&](int, QProcess::ExitStatus) { progress.done(0); });
        QObject::connect(&process, &QProcess::errorOccurred, &progress, [&](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
                progress.done(0);
        });
        process.start(config.python, {"-I", "-u", QDir(config.runtime).filePath("step_ard/export_record.py"),
                                      "--request", requestPath, "--response", responsePath});
        if (process.state() != QProcess::NotRunning)
            progress.exec();
        if (process.error() == QProcess::FailedToStart)
            fail("Nie można uruchomić konwertera: " + process.errorString());
        if (process.exitStatus() != QProcess::NormalExit)
            fail("Proces konwersji zakończył się awarią.");
        QJsonParseError parseError;
        const QJsonDocument responseDocument = QJsonDocument::fromJson(readFile(responsePath, 1024 * 1024), &parseError);
        const QJsonObject response = responseDocument.object();
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()
            || response.value("schema").toString() != "minicam-alphacam-response-v1")
            fail("Niepoprawna odpowiedź konwertera ALPHACAM.");
        if (process.exitCode() != 0 || response.value("ok").toBool() != true)
            fail(response.value("error").toString("Konwersja bryły nie powiodła się."));
        const QByteArray record = readFile(directory.filePath("record.bin"), 512LL * 1024 * 1024);
        if (response.value("record_filename").toString() != "record.bin"
            || response.value("step_sha256").toString() != stepHash
            || response.value("record_sha256").toString() != hash(record)
            || response.value("record_size_bytes").toDouble(-1) != record.size()
            || response.value("bodies").toInt(-1) != 1
            || hash(readFile(input, 256LL * 1024 * 1024)) != stepHash)
            fail("Wynik konwersji nie odpowiada bieżącej bryle MiniCam.");
        return record.toStdString();
    } catch (const Standard_Failure& error) {
        fail("Nie można przygotować bieżącej bryły: " + QString::fromUtf8(error.GetMessageString())
             + "\nRaporty: " + QDir::toNativeSeparators(job));
    } catch (const std::exception& error) {
        fail(QString::fromUtf8(error.what()) + "\nRaporty: " + QDir::toNativeSeparators(job));
    }
}
}
