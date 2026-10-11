// Podpowiedź polecenia w rogu widoku 3D – napis jak na starym terminalu.
#include "OccView.h"

#include "RetroFont.h"

#include <AIS_InteractiveObject.hxx>
#include <Graphic3d_ArrayOfSegments.hxx>
#include <Graphic3d_AspectLine3d.hxx>
#include <Graphic3d_Group.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Prs3d_Presentation.hxx>
#include <SelectMgr_Selection.hxx>
#include <QTimer>

#include <cmath>

namespace {

constexpr int kTypeMs = 45;    // co ile pojawia się kolejna litera
constexpr int kBlinkMs = 530;  // mruganie kursora
constexpr int kMargin = 18;    // odstęp od rogu okna [px]
constexpr double kScale = 0.8; // pikseli na jednostkę czcionki (wielka litera ≈ 17 px)
constexpr double kCursorWidth = 14; // kursor-blok [jednostki czcionki]

// Napis z odcinków w pikselach, przyklejony do prawego górnego rogu okna
// (współrzędne względem rogu, y w górę). Zielony fosfor: szeroka ciemna poświata
// i cienka jasna linia na wierzchu.
class HintText : public AIS_InteractiveObject
{
public:
    std::vector<retrofont::Segment> segments;
    double lineWidth = 1.6;

    void Compute(const Handle(PrsMgr_PresentationManager)&, const Handle(Prs3d_Presentation)& prs,
                 const Standard_Integer) override
    {
        if (segments.empty())
            return;
        Handle(Graphic3d_ArrayOfSegments) lines = new Graphic3d_ArrayOfSegments(int(segments.size()) * 2);
        for (const retrofont::Segment& s : segments) {
            lines->AddVertex(s.x1, s.y1, 0.0);
            lines->AddVertex(s.x2, s.y2, 0.0);
        }
        const Quantity_Color glow(0.04, 0.30, 0.10, Quantity_TOC_sRGB);
        const Quantity_Color phosphor(0.40, 1.0, 0.45, Quantity_TOC_sRGB);
        for (const auto& [color, width] : {std::pair{glow, lineWidth * 3.0}, std::pair{phosphor, lineWidth}}) {
            Handle(Graphic3d_Group) group = prs->NewGroup();
            group->SetGroupPrimitivesAspect(new Graphic3d_AspectLine3d(color, Aspect_TOL_SOLID, width));
            group->AddPrimitiveArray(lines);
        }
    }
    void ComputeSelection(const Handle(SelectMgr_Selection)&, const Standard_Integer) override {}
};

} // namespace

void OccView::showHint(const QString& text)
{
    // Stare terminale znały tylko wielkie litery.
    const QString upper = text.simplified().toUpper();
    if (upper == m_hintText)
        return;
    if (!m_hintTimer) {
        m_hintTimer = new QTimer(this);
        connect(m_hintTimer, &QTimer::timeout, this, &OccView::hintTick);
    }
    // Wspólny początek zostaje, dopisujemy tylko to, co się zmieniło.
    int same = 0;
    while (same < m_hintTyped && same < upper.size() && upper[same] == m_hintText[same])
        ++same;
    m_hintText = upper;
    m_hintTyped = same;
    m_hintCursor = true;
    if (upper.isEmpty())
        m_hintTimer->stop();
    else
        m_hintTimer->start(kTypeMs);
    redrawHint();
}

void OccView::hintTick()
{
    if (m_hintTyped < m_hintText.size()) {
        ++m_hintTyped;
        m_hintCursor = true;
        if (m_hintTyped == m_hintText.size())
            m_hintTimer->setInterval(kBlinkMs);
    } else {
        m_hintCursor = !m_hintCursor;
    }
    redrawHint();
}

void OccView::redrawHint()
{
    if (m_context.IsNull())
        return;
    if (m_hintText.isEmpty()) {
        if (!m_hint.IsNull()) {
            m_context->Remove(m_hint, Standard_False);
            m_hint.Nullify();
            m_view->Redraw();
        }
        return;
    }

    // Miejsce na cały napis jest znane od początku – litery dopisują się w miejscu,
    // a nie przesuwają w lewo.
    const qreal r = devicePixelRatioF();
    const double scale = kScale * r;
    const double total = retrofont::width(m_hintText) + kCursorWidth + 4;
    std::vector<retrofont::Segment> glyphs;
    const double cursorX = retrofont::layout(m_hintText.left(m_hintTyped), glyphs) + 2;
    if (m_hintCursor) {
        // Kursor-blok z poziomych kresek, jak linie obrazu kineskopu.
        const int rows = std::max(2, int(std::lround((retrofont::kBaseline - retrofont::kCapTop) * scale / 2.0)));
        for (int i = 0; i <= rows; ++i) {
            const double y = retrofont::kCapTop + (retrofont::kBaseline - retrofont::kCapTop) * i / rows;
            glyphs.push_back({cursorX, y, cursorX + kCursorWidth, y});
        }
    }

    Handle(HintText) hint = Handle(HintText)::DownCast(m_hint);
    const bool created = hint.IsNull();
    if (created) {
        hint = new HintText();
        hint->SetTransformPersistence(new Graphic3d_TransformPers(
            Graphic3d_TMF_2d, Aspect_TOTP_RIGHT_UPPER, Graphic3d_Vec2i(int(kMargin * r), int(kMargin * r))));
        hint->SetZLayer(Graphic3d_ZLayerId_TopOSD);
        hint->SetMutable(Standard_True);
        m_hint = hint;
    }
    hint->lineWidth = 1.6 * r;
    hint->segments.clear();
    for (const retrofont::Segment& s : glyphs) {
        // Jednostki czcionki (y w dół) → piksele względem prawego górnego rogu (y w górę).
        hint->segments.push_back({(s.x1 - total) * scale, -(s.y1 - retrofont::kTop) * scale,
                                  (s.x2 - total) * scale, -(s.y2 - retrofont::kTop) * scale});
    }
    if (created)
        m_context->Display(hint, 0, -1, Standard_False); // -1 = nie da się go wybrać
    else
        m_context->Redisplay(hint, Standard_False);
    m_view->Redraw();
}
