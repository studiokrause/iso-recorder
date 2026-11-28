#pragma once
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QString>
#include <QWidget>

class QKeyEvent;
class QKeySequenceEdit;
class QLabel;
class QSettings;
class QTableWidget;

// Skroty klawiszowe: lista dzialan z domyslnymi klawiszami, zmiany uzytkownika (zapisywane tylko roznice od
// domyslnych) i dopasowanie nacisnietego klawisza do dzialania.
class ShortcutMap {
public:
    struct Action {
        QString id;
        QString name;                      // nazwa dla czlowieka (juz przetlumaczona)
        QList<QKeySequence> defaults;
    };

    ShortcutMap() = default;
    explicit ShortcutMap(const QList<Action>& actions);

    const QList<Action>& actions() const { return m_actions; }
    QList<QKeySequence> keys(const QString& id) const { return m_keys.value(id); }
    // Przypisuje klawisz do dzialania (pusty = bez skrotu) i zabiera go innym; zwraca nazwy dzialan, ktore go stracily.
    QStringList assign(const QString& id, const QKeySequence& key);
    void reset();
    void load(QSettings& store);           // klucze "shortcuts/<id>"
    void save(QSettings& store) const;
    // Dzialanie dla nacisnietego klawisza albo pusty napis.
    QString match(const QKeyEvent* e) const;
    static QString describe(const QList<QKeySequence>& keys);

private:
    QString find(const QKeySequence& key) const;
    QList<Action> m_actions;
    QHash<QString, QList<QKeySequence>> m_keys;
};

// Edytor skrotow do okna Settings: tabela dzialan, pole przechwytujace klawisz, usuwanie i przywracanie domyslnych.
class ShortcutEditor : public QWidget {
    Q_OBJECT
public:
    explicit ShortcutEditor(const ShortcutMap& map, QWidget* parent = nullptr);
    const ShortcutMap& map() const { return m_map; }

private:
    void fill();
    void showCurrent();
    ShortcutMap m_map;
    QTableWidget* m_table;
    QKeySequenceEdit* m_edit;
    QLabel* m_status;
};
