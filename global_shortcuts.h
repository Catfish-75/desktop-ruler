#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QHash>
#include <QKeySequence>
#include <QVariantMap>

#include <functional>

class QDBusObjectPath;

namespace ruler {

class GlobalShortcutPortal final : public QObject {
    Q_OBJECT
public:
    using ActionCallback = std::function<void(int)>;
    using StatusCallback = std::function<void(bool, const QString&)>;

    GlobalShortcutPortal(ActionCallback actionCallback, StatusCallback statusCallback,
                         QObject* parent = nullptr);
    ~GlobalShortcutPortal() override;

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled, const QHash<int, QKeySequence>& bindings, bool english);

private slots:
    void onRequestResponse(uint response, const QVariantMap& results);
    void onActivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId,
                     qulonglong timestamp, const QVariantMap& options);

private:
    enum class RequestKind { None, CreateSession, BindShortcuts };

    void beginCreateSession();
    void beginBindShortcuts();
    bool registerHostApplication();
    bool watchRequest(const QString& token, RequestKind kind);
    void requestReplyReceived(const QString& expectedPath, const QDBusMessage& reply);
    void fail(const QString& detail);
    void closePendingRequest();
    void closeSession();
    void notify(bool success, const QString& text);

    ActionCallback actionCallback_;
    StatusCallback statusCallback_;
    QString busConnectionName_;
    QDBusConnection bus_;
    QHash<int, QKeySequence> bindings_;
    QHash<QString, int> actionByPortalId_;
    QString pendingPath_;
    QString sessionHandle_;
    RequestKind pendingKind_ = RequestKind::None;
    bool hostApplicationRegistered_ = false;
    bool enabled_ = false;
    bool english_ = false;
};

} // namespace ruler
