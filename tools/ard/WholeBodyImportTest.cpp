#include "WholeBodyImport.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRep_Builder.hxx>
#include <STEPControl_Writer.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Pln.hxx>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
QByteArray read(const QString& path) {
    QFile file(path); require(file.open(QIODevice::ReadOnly), "Cannot read test input"); return file.readAll();
}
QString sha(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QJsonObject connection() {
    return {{"schema", "feature-cls-session-14-v1"}, {"ready", true}, {"argument_bridge_selftest", true},
            {"open_return_code", 1}, {"validation_before", 101}, {"validation_after", 101},
            {"classification_flag", false}, {"close_completed", true}, {"validation_after_feature", 101}};
}
QJsonObject report(const QByteArray& step) {
    return {{"schema", "minicam-solidlink-bulk-probe-v4"}, {"native_conversion_succeeded", true},
            {"worker_exit_code", 0}, {"cls_exit_code", 0}, {"error", QJsonValue::Null},
            {"source_ard_unchanged", true}, {"payload_unchanged", true},
            {"source", QJsonObject{{"source_ard_sha256", sha("ARD fixture")}, {"parasolid_sha256", sha("payload fixture")},
                                    {"declared_body_count", 1}}},
            {"cls_ready", connection()}, {"cls_result", connection()},
            {"native", QJsonObject{{"schema", "minicam-solidlink-bulk-probe-v4"}, {"phase", "finished"},
                {"conversion_entry", "original_SLInterop_RunInteropStandard"}, {"whole_body_conversion", true},
                {"acam_used", false}, {"native_conversion_succeeded", true}, {"error", QJsonValue::Null},
                {"source_sha256", sha("payload fixture")}, {"output_sha256", sha(step)}, {"output_size_bytes", step.size()},
                {"result", QJsonObject{{"original_return", true}, {"raw", 0}, {"success", true}, {"partial", false}}}}}};
}
void rejected(const QJsonObject& value, const QString& path) {
    bool failed = false;
    try { ardimport::modelFromWholeBodyResult(QJsonDocument(value).toJson(), path, sha("ARD fixture"), sha("payload fixture")); }
    catch (const std::exception&) { failed = true; }
    require(failed, "Invalid or uncorrelated conversion was accepted");
}
void invalidShape(const TopoDS_Shape& shape) {
    bool failed = false;
    try { ardimport::inspectWholeBody({shape, {}}); }
    catch (const std::exception&) { failed = true; }
    require(failed, "Non-solid or extra geometry was accepted");
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        if (argc == 5) {
            const auto model = ardimport::modelFromWholeBodyResult(read(QString::fromLocal8Bit(argv[2])),
                QString::fromLocal8Bit(argv[1]), QString::fromLatin1(argv[3]), QString::fromLatin1(argv[4]));
            std::cout << QJsonDocument(ardimport::inspectWholeBody(model)).toJson().constData();
            return 0;
        }
        require(argc == 1, "Usage: checker STEP report.json SOURCE_SHA256 PAYLOAD_SHA256, or no args for synthetic checks");
        QTemporaryDir directory; require(directory.isValid(), "No test directory");
        const auto path = directory.filePath("translated.step");
        const auto solid = BRepPrimAPI_MakeBox(gp_Pnt(10, -20, 7), 2, 3, 4).Shape();
        STEPControl_Writer writer;
        require(writer.Transfer(solid, STEPControl_ManifoldSolidBrep) == IFSelect_RetDone, "Test STEP transfer failed");
        require(writer.Write(path.toUtf8().constData()) == IFSelect_RetDone, "Test STEP write failed");
        const auto original = report(read(path));
        auto model = ardimport::modelFromWholeBodyResult(QJsonDocument(original).toJson(), path, sha("ARD fixture"), sha("payload fixture"));
        const auto metrics = ardimport::inspectWholeBody(model);
        require(std::abs(metrics["volume_mm3"].toDouble() - 24) < 1e-6, "Volume changed during STEP import");
        const auto bounds = metrics["bounds_mm"].toArray();
        const double expected[] = {10, -20, 7, 12, -17, 11};
        for (int i = 0; i < 6; ++i) require(std::abs(bounds[i].toDouble() - expected[i]) < 1e-6, "Source placement changed");
        for (int test = 0; test < 9; ++test) {
            auto value = original;
            auto native = value["native"].toObject();
            if (test == 0) { auto source = value["source"].toObject(); source["source_ard_sha256"] = sha("different ARD"); value["source"] = source; }
            if (test == 1) native["source_sha256"] = sha("different payload");
            if (test == 2) native["output_sha256"] = sha("stale STEP");
            if (test == 3) { auto result = native["result"].toObject(); result["original_return"] = false; native["result"] = result; }
            if (test == 4) { auto result = native["result"].toObject(); result["partial"] = true; native["result"] = result; }
            if (test == 5) { auto cls = value["cls_result"].toObject(); cls["close_completed"] = false; value["cls_result"] = cls; }
            if (test == 6) value["worker_exit_code"] = false; // JSON false is not an integer success code.
            if (test == 7) native["cleanup_error"] = "failed cleanup";
            if (test == 8) value["source_ard_unchanged"] = false;
            value["native"] = native;
            rejected(value, path);
        }
        auto appended = read(path) + "\nmodified";
        QFile changed(path); require(changed.open(QIODevice::WriteOnly), "Cannot change test STEP"); changed.write(appended); changed.close();
        rejected(original, path);
        invalidShape(BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 0, 1, 0, 1).Shape());
        BRep_Builder builder; TopoDS_Compound extra; builder.MakeCompound(extra); builder.Add(extra, solid);
        builder.Add(extra, BRepBuilderAPI_MakeEdge(gp_Pnt(100, 0, 0), gp_Pnt(101, 0, 0)).Shape()); invalidShape(extra);
        TopoDS_Compound multiple; builder.MakeCompound(multiple); builder.Add(multiple, solid);
        builder.Add(multiple, BRepPrimAPI_MakeBox(gp_Pnt(100, 0, 0), 2, 3, 4).Shape()); invalidShape(multiple);
        std::cout << "Whole-body STEP placement, geometry, report correlation and failure checks passed.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
