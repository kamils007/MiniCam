#pragma once
#include <string>
#include <vector>
#include <Quantity_Color.hxx>

namespace camcore {

// Nazwa warstwy na geometrie jeszcze niesklasyfikowane (jak w Alphacam).
inline const char* const kApsLayer = "Geometrie APS";
// Geometrie APS są zawsze zielone, jak w Alphacam.
inline const Quantity_Color kApsColor(0.00, 0.75, 0.00, Quantity_TOC_sRGB);

// Warstwa: nazwa, kolor i widoczność. Geometrie rysują się kolorem swojej warstwy.
struct Layer
{
    std::string name;
    Quantity_Color color = Quantity_Color(0.10, 0.45, 1.00, Quantity_TOC_sRGB);
    bool visible = true;
};

// Lista warstw projektu. Zawsze jest w niej warstwa APS (pierwsza); warstwy
// użytkownika dochodzą przez createLayer – z rozpoznawania cech, a później
// także ręcznie z panelu Warstwy.
class LayerList
{
public:
    LayerList() { m_layers.push_back({kApsLayer, kApsColor}); }

    // Tworzy warstwę użytkownika o tej nazwie albo zwraca istniejącą.
    // Bez podanego koloru warstwa dostaje kolejny kolor z palety.
    Layer& createLayer(const std::string& name)
    {
        if (Layer* l = find(name))
            return *l;
        m_layers.push_back({name, nextColor()});
        return m_layers.back();
    }
    Layer& createLayer(const std::string& name, const Quantity_Color& color)
    {
        Layer& l = createLayer(name);
        l.color = color;
        return l;
    }

    // Usuwa warstwę użytkownika (warstwy APS nie da się usunąć).
    void removeLayer(const std::string& name)
    {
        for (size_t i = 1; i < m_layers.size(); ++i)
            if (m_layers[i].name == name) {
                m_layers.erase(m_layers.begin() + static_cast<long>(i));
                return;
            }
    }

    // Zostawia samą warstwę APS (np. po wczytaniu nowej bryły).
    void clearUserLayers() { m_layers.resize(1); }

    Layer* find(const std::string& name)
    {
        for (Layer& l : m_layers)
            if (l.name == name)
                return &l;
        return nullptr;
    }
    const Layer* find(const std::string& name) const
    {
        return const_cast<LayerList*>(this)->find(name);
    }
    // Warstwa o tej nazwie, a gdy jej nie ma – APS.
    const Layer& layerOrAps(const std::string& name) const
    {
        const Layer* l = find(name);
        return l ? *l : m_layers.front();
    }

    const std::vector<Layer>& all() const { return m_layers; } // [0] = APS

private:
    Quantity_Color nextColor() const
    {
        static const double palette[][3] = {
            {0.10, 0.45, 1.00}, // niebieski
            {0.85, 0.10, 0.85}, // fioletowy
            {0.95, 0.55, 0.00}, // pomarańczowy
            {0.00, 0.70, 0.75}, // turkusowy
            {0.90, 0.10, 0.10}, // czerwony
            {0.55, 0.35, 0.10}, // brązowy
        };
        const size_t n = sizeof(palette) / sizeof(palette[0]);
        const double* c = palette[(m_layers.size() - 1) % n];
        return Quantity_Color(c[0], c[1], c[2], Quantity_TOC_sRGB);
    }

    std::vector<Layer> m_layers;
};

} // namespace camcore
