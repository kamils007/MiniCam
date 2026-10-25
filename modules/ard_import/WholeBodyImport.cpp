#include "WholeBodyImport.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool boolean(const QJsonObject& object, const char* name, bool expected) {
    const auto value = object.value(QLatin1String(name));
    return value.isBool() && value.toBool() == expected;
}
bool number(const QJsonObject& object, const char* name, int expected) {
    const auto value = object.value(QLatin1String(name));
    return value.isDouble() && value.toDouble() == expected;
}
QString sha(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QByteArray stepBytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly) && file.size() >= 128 && file.size() <= 256 * 1024 * 1024,
            "Brak kompletnego pliku STEP z konwersji ARD.");
    const auto bytes = file.readAll();
    require(file.error() == QFileDevice::NoError, "Blad odczytu wyniku konwersji ARD.");
    return bytes;
}
void validateCls(const QJsonObject& ready, const QJsonObject& final) {
    for (const auto& state : {ready, final}) {
        require(state["schema"].toString() == "feature-cls-session-14-v1"
                    && boolean(state, "ready", true) && boolean(state, "argument_bridge_selftest", true)
                    && number(state, "open_return_code", 1)
                    && number(state, "validation_before", 101) && number(state, "validation_after", 101),
                "Niepotwierdzona biezaca sesja licencyjna konwersji ARD.");
    }
    require(boolean(final, "close_completed", true) && number(final, "validation_after_feature", 101),
            "Niepotwierdzone zakonczenie sesji licencyjnej konwersji ARD.");
    for (const auto* field : {"error_type", "release_error", "close_error", "lease_expired"})
        require(!final.contains(field), "Blad sesji licencyjnej konwersji ARD.");
    require(ready["classification_flag"].isBool() && ready["classification_flag"] == final["classification_flag"],
            "Zmiana stanu sesji licencyjnej podczas konwersji ARD.");
    if (final["classification_flag"].toBool())
        require(number(final, "release_return_code", 201) || number(final, "release_return_code", 202)
                    || number(final, "release_return_code", 207), "Niepotwierdzone zwolnienie zasobu CLS.");
}
}

namespace ardimport {
QJsonObject inspectWholeBody(const camcore::ImportedModel& model) {
    require(!model.shape.IsNull(), "STEP nie zawiera geometrii bryly ARD.");
    require(BRepCheck_Analyzer(model.shape).IsValid(), "STEP z ARD zawiera niepoprawna geometrie.");
    TopTools_IndexedMapOfShape solids, faces, edges, vertices;
    TopExp::MapShapes(model.shape, TopAbs_SOLID, solids);
    require(solids.Extent() == 1, "Konwersja ARD nie zwrocila dokladnie jednej bryly.");
    for (const auto kind : {TopAbs_FACE, TopAbs_EDGE, TopAbs_VERTEX}) {
        TopExp_Explorer extra(model.shape, kind, TopAbs_SOLID);
        require(!extra.More(), "Konwersja ARD zwrocila dodatkowa geometrie poza bryla.");
    }
    for (TopExp_Explorer shell(solids(1), TopAbs_SHELL); shell.More(); shell.Next())
        require(BRep_Tool::IsClosed(shell.Current()), "Konwersja ARD zwrocila otwarta powloke.");
    TopExp::MapShapes(model.shape, TopAbs_FACE, faces);
    TopExp::MapShapes(model.shape, TopAbs_EDGE, edges);
    TopExp::MapShapes(model.shape, TopAbs_VERTEX, vertices);
    GProp_GProps volume, area;
    BRepGProp::VolumeProperties(model.shape, volume);
    BRepGProp::SurfaceProperties(model.shape, area);
    require(std::isfinite(volume.Mass()) && volume.Mass() > 0
                && std::isfinite(area.Mass()) && area.Mass() > 0,
            "Niepoprawna objetosc lub pole bryly z konwersji ARD.");
    Bnd_Box box;
    BRepBndLib::AddOptimal(model.shape, box, false, false);
    require(!box.IsVoid() && !box.IsOpen(), "Niepoprawna obwiednia bryly z ARD.");
    double bounds[6];
    box.Get(bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);
    QJsonArray limits, dimensions;
    for (double value : bounds) {
        require(std::isfinite(value), "Nieskonczona obwiednia bryly z ARD.");
        limits.append(value);
    }
    for (int axis = 0; axis < 3; ++axis) {
        require(bounds[axis + 3] > bounds[axis], "Zerowy wymiar bryly z ARD.");
        dimensions.append(bounds[axis + 3] - bounds[axis]);
    }
    return {{"occt_geometry_valid", true}, {"solids", solids.Extent()}, {"faces", faces.Extent()},
            {"edges", edges.Extent()}, {"vertices", vertices.Extent()}, {"volume_mm3", volume.Mass()},
            {"area_mm2", area.Mass()}, {"bounds_mm", limits}, {"dimensions_mm", dimensions},
            {"color_entries", int(model.colors.size())}, {"source_geometry_compared", false}};
}

camcore::ImportedModel modelFromWholeBodyResult(const QByteArray& bytes, const QString& stepPath,
                                               const QString& sourceHash, const QString& payloadHash) {
    try {
        static const QRegularExpression hashPattern(QStringLiteral("^[0-9a-f]{64}$"));
        require(hashPattern.match(sourceHash).hasMatch() && hashPattern.match(payloadHash).hasMatch(),
                "Brak identyfikacji zrodla konwersji ARD.");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(bytes, &error);
        require(error.error == QJsonParseError::NoError && document.isObject(), "Niepoprawny raport konwersji ARD.");
        const auto report = document.object(), native = report["native"].toObject();
        const auto source = report["source"].toObject(), result = native["result"].toObject();
        require(report["schema"].toString() == "minicam-solidlink-bulk-probe-v4"
                    && native["schema"] == report["schema"] && native["phase"].toString() == "finished"
                    && native["conversion_entry"].toString() == "original_SLInterop_RunInteropStandard",
                "Nieobslugiwany przebieg konwersji ARD.");
        require(boolean(report, "native_conversion_succeeded", true) && number(report, "worker_exit_code", 0)
                    && number(report, "cls_exit_code", 0) && boolean(native, "native_conversion_succeeded", true)
                    && boolean(native, "whole_body_conversion", true) && boolean(native, "acam_used", false)
                    && boolean(result, "original_return", true) && number(result, "raw", 0)
                    && boolean(result, "success", true) && boolean(result, "partial", false)
                    && report["error"].isNull() && native["error"].isNull()
                    && !native.contains("cleanup_error") && !report.contains("cls_cleanup_timeout")
                    && !report.contains("timed_out"), "Niepotwierdzona pelna konwersja bryly ARD.");
        require(boolean(report, "source_ard_unchanged", true) && boolean(report, "payload_unchanged", true)
                    && source["source_ard_sha256"].toString() == sourceHash
                    && source["parasolid_sha256"].toString() == payloadHash
                    && native["source_sha256"].toString() == payloadHash
                    && number(source, "declared_body_count", 1), "Wynik konwersji nie odpowiada otwieranemu ARD.");
        validateCls(report["cls_ready"].toObject(), report["cls_result"].toObject());
        const auto step = stepBytes(stepPath);
        require(sha(step) == native["output_sha256"].toString()
                    && native["output_size_bytes"].isDouble() && native["output_size_bytes"].toDouble() == step.size(),
                "Plik STEP nie odpowiada potwierdzonej konwersji ARD.");
        const auto envelope = step.trimmed();
        require(envelope.startsWith("ISO-10303-21;") && envelope.endsWith("END-ISO-10303-21;")
                    && envelope.contains("HEADER;") && envelope.contains("DATA;"), "Niepelny plik STEP z ARD.");
        auto model = camcore::importModel(stepPath.toUtf8().toStdString());
        inspectWholeBody(model);
        require(sha(stepBytes(stepPath)) == sha(step), "STEP zmienil sie podczas odczytu ARD.");
        return model;
    } catch (const Standard_Failure& error) {
        const auto* detail = error.GetMessageString();
        throw std::runtime_error(std::string("OCCT: ") + (detail ? detail : "STEP import failed"));
    }
}
}
