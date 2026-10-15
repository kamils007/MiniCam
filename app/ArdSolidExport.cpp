#include "ArdSolidExport.h"
#include "ModelExport.h"
#include <Standard_Failure.hxx>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QTemporaryDir>
#include <stdexcept>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
[[noreturn]] void error(const QString& message)
{
    throw std::runtime_error(message.toUtf8().constData());
}
QString configuredFile(QSettings& settings, const char* key, const QString& suggested,
                       const QString& title, const QString& filter, QWidget* parent)
{
    QString path = settings.value(key, suggested).toString();
    if (!QFileInfo::exists(path)) {
        path = QFileDialog::getOpenFileName(parent, title, suggested, filter);
        if (path.isEmpty())
            error(QString::fromUtf8("Anulowano konfigurację zapisu bryły ARD."));
        settings.setValue(key, path);
    }
    return path;
}
}

std::string createArdSolidRecord(const camcore::ImportedModel& model, QWidget* parent)
{
#ifndef Q_OS_WIN
    Q_UNUSED(model);
    Q_UNUSED(parent);
    error(QString::fromUtf8("Zapis bryły przez Feature.dll wymaga Windows x64."));
#else
    const QDir application(QCoreApplication::applicationDirPath());
    const QString helper = application.filePath("minicam-feature-host.exe");
    if (!QFileInfo::exists(helper))
        error(QString::fromUtf8("Brak minicam-feature-host.exe obok aplikacji. Zbuduj także cel minicam-feature-host."));
    QSettings settings("MiniCAM", "MiniCAM");
    const QString feature = configuredFile(settings, "ard/featureDll",
        application.filePath("Feature/Feature.dll"),
        QString::fromUtf8("Wybierz sprawdzoną Feature.dll z ALPHACAM 2025"), "Feature.dll (Feature.dll)", parent);
    const QDir featureDir = QFileInfo(feature).absoluteDir();
    const QString environment = configuredFile(settings, "ard/hostEnvironment",
        application.filePath("Feature/host_environment_2025.json"),
        QString::fromUtf8("Wybierz odczyt ustawień hosta z działającej próby Python"), "JSON (*.json)", parent);
    const QString catalog = featureDir.filePath("Feature.po");
    if (!QFileInfo::exists(catalog))
        error(QString::fromUtf8("Brak Feature.po w katalogu Feature.dll."));

    QTemporaryDir temporary;
    if (!temporary.isValid())
        error(QString::fromUtf8("Nie udało się utworzyć katalogu roboczego zapisu ARD."));
    const QString input = temporary.filePath("current-model.step");
    const QString output = temporary.filePath("feature_object.bin");
    // Export the actual current shape, including its baked placement and colors.
    // ImportAs(STEP) must succeed; the source filename is never substituted for it.
    try {
        camcore::exportModel(model, input.toUtf8().toStdString());
    } catch (const Standard_Failure& failure) {
        const char* message = failure.GetMessageString();
        error(QString::fromUtf8("Nie udało się przygotować bieżącej bryły: ")
              + QString::fromUtf8(message ? message : "Open CASCADE failure"));
    }

    QProgressDialog progress(QString::fromUtf8("Zapisywanie bryły ARD…"),
                              QString::fromUtf8("Anuluj"), 0, 0, parent);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    QProcess process;
    process.setWorkingDirectory(temporary.path());
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
    process.start(helper, {feature, environment, input, output, catalog});
    if (!process.waitForStarted(5000))
        error(QString::fromUtf8("Nie udało się uruchomić hosta Feature.dll: ") + process.errorString());
    QElapsedTimer timer;
    timer.start();
    while (!process.waitForFinished(100)) {
        QApplication::processEvents();
        if (progress.wasCanceled() || timer.elapsed() > 60000) {
            process.kill();
            process.waitForFinished(5000);
            error(progress.wasCanceled() ? QString::fromUtf8("Anulowano zapis ARD.")
                                         : QString::fromUtf8("Przekroczono limit 60 sekund zapisu bryły ARD."));
        }
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
        error(QString::fromUtf8("Feature.dll nie zapisała bieżącej bryły. Plik docelowy nie został zmieniony.\n")
              + detail + QString::fromUtf8("\nSprawdzona próba Python obsługiwała wejście Parasolid. Import bieżącej bryły STEP musi być obsługiwany przez bibliotekę."));
    }
    QFile record(output);
    if (!record.open(QIODevice::ReadOnly) || record.size() < 8)
        error(QString::fromUtf8("Host nie utworzył poprawnego rekordu bryły ARD."));
    return record.readAll().toStdString();
#endif
}
