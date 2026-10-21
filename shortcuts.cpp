#include "shortcuts.h"
#include "global_shortcuts.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace ruler {
namespace {

struct ActionInfo {
    int id;
    QString key;
    QString label;
    QString labelEn;
    QString defaultSequence;
};

const QVector<ActionInfo>& actionSpecs() {
    static const QVector<ActionInfo> value{
        {0, QStringLiteral("mode/0"), QStringLiteral("Линия и путь"), QStringLiteral("Distance and path"), QStringLiteral("Ctrl+Alt+1")},
        {1, QStringLiteral("mode/1"), QStringLiteral("Площадь и периметр"), QStringLiteral("Area and perimeter"), QStringLiteral("Ctrl+Alt+2")},
        {2, QStringLiteral("mode/2"), QStringLiteral("Периметр"), QStringLiteral("Perimeter"), QStringLiteral("Ctrl+Alt+3")},
        {3, QStringLiteral("mode/3"), QStringLiteral("Автообласть"), QStringLiteral("Automatic area"), QStringLiteral("Ctrl+Alt+4")},
        {4, QStringLiteral("mode/4"), QStringLiteral("Угол"), QStringLiteral("Angle"), QStringLiteral("Ctrl+Alt+5")},
        {5, QStringLiteral("mode/5"), QStringLiteral("Подсчёт"), QStringLiteral("Count"), QStringLiteral("Ctrl+Alt+6")},
        {6, QStringLiteral("mode/6"), QStringLiteral("Калибровка"), QStringLiteral("Calibration"), QStringLiteral("Ctrl+Alt+7")},
        {7, QStringLiteral("mode/7"), QStringLiteral("Лупа"), QStringLiteral("Magnifier"), QStringLiteral("Ctrl+Alt+8")},
        {CaptureShortcutAction, QStringLiteral("capture"), QStringLiteral("Новый снимок"), QStringLiteral("New capture"), QStringLiteral("Ctrl+Alt+C")},
        {HistoryShortcutAction, QStringLiteral("history"), QStringLiteral("История"), QStringLiteral("History"), QStringLiteral("Ctrl+Alt+H")},
    };
    return value;
}

QSettings shortcutSettings() {
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("desktop-ruler"), QStringLiteral("desktop-ruler"));
}

QKeySequence defaultSequence(const ActionInfo& action) {
    return QKeySequence(action.defaultSequence, QKeySequence::PortableText);
}

QKeySequence readSequence(QSettings& settings, const ActionInfo& action) {
    const QString stored = settings.value(action.key).toString();
    if (stored.isNull()) return defaultSequence(action);
    return QKeySequence(stored, QKeySequence::PortableText);
}

bool isTextEntry(QWidget* focus) {
    for (QWidget* current = focus; current; current = current->parentWidget()) {
        if (qobject_cast<QLineEdit*>(current) || qobject_cast<QTextEdit*>(current) ||
            qobject_cast<QPlainTextEdit*>(current) || qobject_cast<QAbstractSpinBox*>(current) ||
            qobject_cast<QKeySequenceEdit*>(current))
            return true;
        if (auto* combo = qobject_cast<QComboBox*>(current); combo && combo->isEditable())
            return true;
    }
    return false;
}

class ShortcutDialog final : public QDialog {
public:
    explicit ShortcutDialog(QWidget* parent, bool english, bool globalEnabled)
        : QDialog(parent), english_(english) {
        setWindowTitle(english ? QStringLiteral("Keyboard shortcuts") : QStringLiteral("Сочетания клавиш"));
        setModal(true);

        auto* root = new QVBoxLayout(this);
        auto* form = new QFormLayout;
        auto settings = shortcutSettings();
        settings.beginGroup(QStringLiteral("shortcuts"));
        for (const auto& action : actionSpecs()) {
            auto* row = new QWidget(this);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto* edit = new QKeySequenceEdit(readSequence(settings, action), row);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            edit->setMaximumSequenceLength(1);
#endif
            auto* clear = new QPushButton(english ? QStringLiteral("Clear") : QStringLiteral("Очистить"), row);
            clear->setAutoDefault(false);
            clear->setToolTip(english ? QStringLiteral("Remove this shortcut") : QStringLiteral("Убрать это сочетание"));
            QObject::connect(clear, &QPushButton::clicked, edit, [edit] { edit->clear(); });
            rowLayout->addWidget(edit, 1);
            rowLayout->addWidget(clear);
            const QString label = english ? action.labelEn : action.label;
            form->addRow(label, row);
            edits_.insert(action.id, edit);
        }
        settings.endGroup();
        root->addLayout(form);

        globalEnabled_ = new QCheckBox(
            english ? QStringLiteral("Register actions with the desktop for this run")
                    : QStringLiteral("Зарегистрировать действия в окружении до выхода"), this);
        globalEnabled_->setChecked(globalEnabled);
        globalEnabled_->setToolTip(english
            ? QStringLiteral("Registration makes actions visible to the desktop. The combinations above are suggestions only; the desktop or compositor must assign the actual keys.")
            : QStringLiteral("Регистрация показывает действия окружению. Сочетания выше — только предложения; реальные клавиши назначаются в окружении или композиторе."));
        root->addWidget(globalEnabled_);

        const QString globalHelpText = english
            ? QStringLiteral("Hyprland/XDPH: run `hyprctl globalshortcuts`, then add a bind such as `hl.bind(\"SUPER + SHIFT + A\", hl.dsp.global(\"desktop-ruler:mode_0\"))` to your Hyprland config. IDs: `desktop-ruler:mode_0`…`mode_7`, `desktop-ruler:capture`, `desktop-ruler:history`.")
            : QStringLiteral("Hyprland/XDPH: выполните `hyprctl globalshortcuts`, затем добавьте сочетание, например `hl.bind(\"SUPER + SHIFT + A\", hl.dsp.global(\"desktop-ruler:mode_0\"))`, в конфигурацию Hyprland. ID: `desktop-ruler:mode_0`…`mode_7`, `desktop-ruler:capture`, `desktop-ruler:history`.");
        auto* globalHelp = new QLabel(globalHelpText, this);
        globalHelp->setWordWrap(true);
        globalHelp->setTextInteractionFlags(Qt::TextSelectableByMouse);
        globalHelp->setVisible(globalEnabled);
        QObject::connect(globalEnabled_, &QCheckBox::toggled, globalHelp, &QWidget::setVisible);
        root->addWidget(globalHelp);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        auto* reset = buttons->addButton(english ? QStringLiteral("Reset defaults") : QStringLiteral("Сбросить"),
                                         QDialogButtonBox::ResetRole);
        reset->setAutoDefault(false);
        QObject::connect(reset, &QPushButton::clicked, this, [this] {
            for (const auto& action : actionSpecs())
                edits_.value(action.id)->setKeySequence(defaultSequence(action));
        });
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this] { acceptIfValid(); });
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);
        resize(430, sizeHint().height());
    }

    QHash<int, QKeySequence> sequences() const {
        QHash<int, QKeySequence> result;
        for (const auto& action : actionSpecs())
            result.insert(action.id, edits_.value(action.id)->keySequence());
        return result;
    }

    bool globalEnabled() const { return globalEnabled_->isChecked(); }

private:
    void acceptIfValid() {
        QHash<QString, int> seen;
        for (const auto& action : actionSpecs()) {
            const QKeySequence sequence = edits_.value(action.id)->keySequence();
            if (sequence.count() > 1) {
                QMessageBox::warning(this,
                    english_ ? QStringLiteral("One key combination only") : QStringLiteral("Нужно одно сочетание"),
                    english_ ? QStringLiteral("Each action accepts one key combination.")
                             : QStringLiteral("Для каждого действия задаётся одно сочетание клавиш."));
                edits_.value(action.id)->setFocus();
                return;
            }
            if (sequence.isEmpty()) continue;
            const QString normalized = sequence.toString(QKeySequence::PortableText);
            if (seen.contains(normalized)) {
                const auto duplicate = std::find_if(actionSpecs().cbegin(), actionSpecs().cend(),
                    [&seen, &normalized](const ActionInfo& candidate) {
                        return candidate.id == seen.value(normalized);
                    });
                const QString first = duplicate == actionSpecs().cend() ? QString() :
                    (english_ ? duplicate->labelEn : duplicate->label);
                const QString current = english_ ? action.labelEn : action.label;
                QMessageBox::warning(this,
                    english_ ? QStringLiteral("Shortcut conflict") : QStringLiteral("Конфликт сочетаний"),
                    english_ ? QStringLiteral("%1 and %2 use the same shortcut.").arg(first, current)
                             : QStringLiteral("Для «%1» и «%2» задано одно сочетание.").arg(first, current));
                edits_.value(action.id)->setFocus();
                return;
            }
            seen.insert(normalized, action.id);
        }
        QDialog::accept();
    }

    bool english_;
    QHash<int, QKeySequenceEdit*> edits_;
    QCheckBox* globalEnabled_ = nullptr;
};

} // namespace

ShortcutController::ShortcutController(QWidget* host, Callback callback)
    : host_(host), callback_(std::move(callback)) {
    if (!host_) return;
    for (const auto& action : actionSpecs()) {
        auto* shortcut = new QShortcut(QKeySequence(), host_);
        shortcut->setContext(Qt::ApplicationShortcut);
        shortcut->setAutoRepeat(false);
        QObject::connect(shortcut, &QShortcut::activated, host_, [this, action] {
            if (!host_ || !host_->isVisible() || !host_->isActiveWindow() ||
                QApplication::activeModalWidget() || isTextEntry(QApplication::focusWidget()))
                return;
            if (callback_) callback_(action.id);
        });
        shortcuts_.append(shortcut);
    }
    reload();
}

ShortcutController::~ShortcutController() {
    for (auto* shortcut : shortcuts_) delete shortcut;
}

void ShortcutController::reload() {
    if (shortcuts_.size() != actionSpecs().size()) return;
    auto settings = shortcutSettings();
    settings.beginGroup(QStringLiteral("shortcuts"));
    for (int i = 0; i < actionSpecs().size(); ++i)
        shortcuts_[i]->setKey(readSequence(settings, actionSpecs()[i]));
    settings.endGroup();
}

void ShortcutController::showSettings(bool english) {
    if (!host_) return;
    english_ = english;
    ShortcutDialog dialog(host_, english, globalPortal_ && globalPortal_->isEnabled());
    if (dialog.exec() != QDialog::Accepted) return;

    auto settings = shortcutSettings();
    settings.beginGroup(QStringLiteral("shortcuts"));
    const auto sequences = dialog.sequences();
    for (const auto& action : actionSpecs())
        settings.setValue(action.key, sequences.value(action.id).toString(QKeySequence::PortableText));
    settings.endGroup();
    settings.sync();
    reload();

    if (dialog.globalEnabled() && !globalPortal_) {
        globalPortal_ = std::make_unique<GlobalShortcutPortal>(
            [this](int action) {
                // Global activation deliberately bypasses the local focus guard.
                // The window callback remains responsible for refusing actions
                // during an active canvas measurement.
                if (callback_) callback_(action);
            },
            [this](bool success, const QString& message) {
                Q_UNUSED(success)
                if (!host_) return;
                QMessageBox::information(host_, english_ ? QStringLiteral("Global shortcuts")
                                                         : QStringLiteral("Глобальные сочетания"), message);
            },
            host_);
    }
    if (globalPortal_) globalPortal_->setEnabled(dialog.globalEnabled(), sequences, english_);
}

} // namespace ruler
