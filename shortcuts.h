#pragma once

#include <functional>
#include <memory>
#include <QVector>

class QShortcut;
class QWidget;
namespace ruler { class GlobalShortcutPortal; }

namespace ruler {

// Mode actions use the same 0..7 order as ruler::Mode. Utility actions are
// negative so the caller can dispatch them without depending on UI widgets.
constexpr int CaptureShortcutAction = -1;
constexpr int HistoryShortcutAction = -2;

class ShortcutController final {
public:
    using Callback = std::function<void(int)>;

    // The controller is non-owning; the host must outlive it. Installed
    // QShortcuts are parented to host and live with that widget.
    ShortcutController(QWidget* host, Callback callback);
    ~ShortcutController();
    ShortcutController(const ShortcutController&) = delete;
    ShortcutController& operator=(const ShortcutController&) = delete;

    void reload();
    void showSettings(bool english);

private:
    QWidget* host_;
    Callback callback_;
    QVector<QShortcut*> shortcuts_;
    std::unique_ptr<GlobalShortcutPortal> globalPortal_;
    bool english_ = false;
};

} // namespace ruler
