#pragma once

#include <QObject>
#include <QString>

#include <vector>

#include <gp_Pnt.hxx>

#include "Geometry.h"
#include "ModelImport.h"

class InputBar;
class OccView;

// Stan, który da się cofnąć: bryła na ekranie i geometrie (Cofnij / Ponów).
struct EditState
{
    camcore::ImportedModel shown;
    std::vector<camcore::Geometry> geometries;
};

// Co polecenie dostaje od okna: widok, pasek wprowadzania, komunikaty i zmiany
// modelu z cofaniem. Okno (MainWindow) implementuje ten interfejs.
class CommandHost
{
public:
    virtual ~CommandHost() = default;

    virtual OccView* view() = 0;
    virtual InputBar* inputBar() = 0;
    virtual void showMessage(const QString& message) = 0;
    virtual void releaseSnap() = 0; // wyłącza aktywny uchwyt (wraca krzyż)

    virtual EditState editState() const = 0;
    // Pokazuje stan "after" i zapisuje zmianę na liście Cofnij / Ponów.
    virtual void commitEdit(const QString& text, const EditState& before, const EditState& after) = 0;

    // Polecenie się skończyło (wykonane albo przerwane): okno sprząta widok i pasek.
    virtual void commandFinished(const QString& message) = 0;
};

// Polecenie interaktywne (jak w Alphacam): prowadzi użytkownika krok po kroku –
// wybór elementów, punkty klikane w widoku albo wpisane w pasku wprowadzania.
// Okno tworzy polecenie, wywołuje start() i przekazuje mu zdarzenia z widoku
// i paska. Nowe polecenia (Obróć, Lustro, Skaluj…) to kolejne klasy w app/commands.
class Command : public QObject
{
    Q_OBJECT

public:
    explicit Command(CommandHost& host, QObject* parent = nullptr) : QObject(parent), m_host(host) {}

    virtual QString name() const = 0;
    // false = polecenie nie może ruszyć (np. brak modelu) – okno je od razu usuwa.
    virtual bool start() = 0;

    // Zdarzenia przekazywane przez okno.
    virtual void selectionChanged(int /*count*/) {}
    virtual void selectionConfirmed() {}      // PPM, Enter albo OK przy wyborze
    virtual void pointEntered(const gp_Pnt&) {} // punkt kliknięty albo wpisany
    virtual void cancel();                      // Esc albo "Zakończ"

    virtual bool isSelecting() const { return false; } // czy teraz wybiera elementy

protected:
    CommandHost& host() { return m_host; }
    void finish(const QString& message) { m_host.commandFinished(message); }

private:
    CommandHost& m_host;
};
