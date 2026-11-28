#include "Shortcuts.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/Lang.h"

ShortcutMap::ShortcutMap(const QList<Action>& actions) : m_actions(actions) { reset(); }

void ShortcutMap::reset() {
    m_keys.clear();
    for (const Action& a : m_actions) m_keys.insert(a.id, a.defaults);
}

QString ShortcutMap::find(const QKeySequence& key) const {
    for (const Action& a : m_actions)
        if (m_keys.value(a.id).contains(key)) return a.id;
    return {};
}

QStringList ShortcutMap::assign(const QString& id, const QKeySequence& key) {
    QStringList lost;
    if (!m_keys.contains(id)) return lost;
    if (!key.isEmpty())
        for (const Action& a : m_actions)
            if (a.id != id && m_keys[a.id].removeAll(key) > 0) lost << a.name;       // jeden klawisz - jedno dzialanie
    m_keys[id] = key.isEmpty() ? QList<QKeySequence>{} : QList<QKeySequence>{key};
    return lost;
}

void ShortcutMap::load(QSettings& store) {
    reset();
    for (const Action& a : m_actions) {
        const QString k = QStringLiteral("shortcuts/") + a.id;
        if (!store.contains(k)) continue;
        QList<QKeySequence> keys;
        for (const QString& part : store.value(k).toString().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QKeySequence s = QKeySequence::fromString(part, QKeySequence::PortableText);
            if (!s.isEmpty() && s[0].key() != Qt::Key_unknown) keys << s;
        }
        m_keys[a.id] = keys;
    }
    // klawisz przypisany przez uzytkownika ma pierwszenstwo przed domyslnym innego dzialania (np. po nowej wersji)
    for (const Action& a : m_actions) {
        if (!store.contains(QStringLiteral("shortcuts/") + a.id)) continue;
        for (const QKeySequence& s : m_keys.value(a.id))
            for (const Action& other : m_actions)
                if (other.id != a.id && !store.contains(QStringLiteral("shortcuts/") + other.id)) m_keys[other.id].removeAll(s);
    }
}

void ShortcutMap::save(QSettings& store) const {
    for (const Action& a : m_actions) {
        const QString k = QStringLiteral("shortcuts/") + a.id;
        if (m_keys.value(a.id) == a.defaults) {
            store.remove(k);
            continue;
        }
        QStringList parts;
        for (const QKeySequence& s : m_keys.value(a.id)) parts << s.toString(QKeySequence::PortableText);
        store.setValue(k, parts.join(QLatin1Char('\n')));
    }
}

QString ShortcutMap::match(const QKeyEvent* e) const {
    const int key = e->key();
    if (key == 0 || key == Qt::Key_unknown || key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta)
        return {};
    const Qt::KeyboardModifiers mods = e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    QString id = find(QKeySequence(QKeyCombination(mods, Qt::Key(key))));
    // znaki takie jak "+" wymagaja na wielu ukladach klawisza Shift - sam znak tez pasuje
    const bool symbol = key > Qt::Key_Space && key <= Qt::Key_AsciiTilde && !(key >= Qt::Key_A && key <= Qt::Key_Z) &&
                        !(key >= Qt::Key_0 && key <= Qt::Key_9);
    if (id.isEmpty() && symbol && (mods & Qt::ShiftModifier)) id = find(QKeySequence(QKeyCombination(mods & ~Qt::ShiftModifier, Qt::Key(key))));
    return id;
}

QString ShortcutMap::describe(const QList<QKeySequence>& keys) {
    QStringList parts;
    for (const QKeySequence& s : keys) parts << s.toString(QKeySequence::NativeText);
    return parts.join(QStringLiteral(", "));
}

// ---------------------------------------------------------------- edytor
ShortcutEditor::ShortcutEditor(const ShortcutMap& map, QWidget* parent) : QWidget(parent), m_map(map) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    m_table = new QTableWidget(int(m_map.actions().size()), 2);
    m_table->setObjectName(QStringLiteral("shortcutTable"));
    m_table->setHorizontalHeaderLabels({T("Action"), T("Shortcut")});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setColumnWidth(1, 190);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFixedHeight(260);
    m_table->setStyleSheet(QStringLiteral("QTableWidget { background:#1a1a1a; border:1px solid #3a3a3a; gridline-color:#2a2a2a; }"
                                          "QTableWidget::item:selected { background:#4a4a4a; color:#f0f0f0; }"
                                          "QHeaderView::section { background:#262626; color:#9d9d9d; border:0; padding:3px 6px; }"));
    v->addWidget(m_table);
    auto* h = new QHBoxLayout;
    auto* caption = new QLabel(T("New shortcut"));
    m_edit = new QKeySequenceEdit;
    m_edit->setObjectName(QStringLiteral("shortcutEdit"));
    m_edit->setMaximumSequenceLength(1);
    m_edit->setStyleSheet(QStringLiteral("QLineEdit { background:#1a1a1a; border:1px solid #3a3a3a; border-radius:2px; padding:2px 8px; min-height:22px; }"));
    auto* clear = new QPushButton(T("Remove shortcut"));
    auto* reset = new QPushButton(T("Reset all to default"));
    for (QPushButton* b : {clear, reset}) b->setAutoDefault(false);
    h->addWidget(caption);
    h->addWidget(m_edit, 1);
    h->addWidget(clear);
    h->addWidget(reset);
    v->addLayout(h);
    m_status = new QLabel(T("Select an action, click the field and press the new key combination."));
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QStringLiteral("color:#9d9d9d; font-size:11px;"));
    v->addWidget(m_status);
    fill();
    m_table->selectRow(0);
    showCurrent();

    connect(m_table, &QTableWidget::currentCellChanged, this, [this] { showCurrent(); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { m_edit->setFocus(); });
    connect(m_edit, &QKeySequenceEdit::editingFinished, this, [this] {
        const int row = m_table->currentRow();
        if (row < 0 || m_edit->keySequence().isEmpty()) return;
        const QKeySequence key = m_edit->keySequence();
        if (m_map.keys(m_map.actions().at(row).id) == QList<QKeySequence>{key}) return;
        const QStringList lost = m_map.assign(m_map.actions().at(row).id, key);
        fill();
        m_table->selectRow(row);
        m_status->setText(lost.isEmpty() ? T("Assigned: %1").arg(key.toString(QKeySequence::NativeText))
                                         : T("Assigned: %1 — removed from: %2").arg(key.toString(QKeySequence::NativeText), lost.join(QStringLiteral(", "))));
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        const int row = m_table->currentRow();
        if (row < 0) return;
        m_map.assign(m_map.actions().at(row).id, QKeySequence());
        fill();
        m_table->selectRow(row);
        showCurrent();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        m_map.reset();
        fill();
        showCurrent();
    });
}

void ShortcutEditor::fill() {
    for (int i = 0; i < m_map.actions().size(); ++i) {
        const ShortcutMap::Action& a = m_map.actions().at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(a.name));
        const QList<QKeySequence> keys = m_map.keys(a.id);
        auto* item = new QTableWidgetItem(keys.isEmpty() ? T("no shortcut") : ShortcutMap::describe(keys));
        if (keys.isEmpty()) item->setForeground(QColor(0x77, 0x77, 0x77));
        m_table->setItem(i, 1, item);
    }
}

void ShortcutEditor::showCurrent() {
    const int row = m_table->currentRow();
    const QSignalBlocker block(m_edit);
    const QList<QKeySequence> keys = row >= 0 ? m_map.keys(m_map.actions().at(row).id) : QList<QKeySequence>{};
    m_edit->setKeySequence(keys.isEmpty() ? QKeySequence() : keys.first());
}
