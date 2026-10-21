#include "global_shortcuts.h"

#include "shortcuts.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QGuiApplication>
#include <QUuid>

#include <optional>

struct PortalShortcut {
    QString id;
    QVariantMap properties;
};
using PortalShortcuts = QList<PortalShortcut>;
Q_DECLARE_METATYPE(PortalShortcut)
Q_DECLARE_METATYPE(PortalShortcuts)

QDBusArgument& operator<<(QDBusArgument& argument, const PortalShortcut& shortcut) {
    argument.beginStructure();
    argument << shortcut.id << shortcut.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, PortalShortcut& shortcut) {
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.properties;
    argument.endStructure();
    return argument;
}

namespace {
constexpr auto kPortalService = "org.freedesktop.portal.Desktop";
constexpr auto kPortalPath = "/org/freedesktop/portal/desktop";
constexpr auto kPortalInterface = "org.freedesktop.portal.GlobalShortcuts";
constexpr auto kRequestInterface = "org.freedesktop.portal.Request";
constexpr auto kSessionInterface = "org.freedesktop.portal.Session";

QString portalId(int action) {
    if (action == ruler::CaptureShortcutAction) return QStringLiteral("capture");
    if (action == ruler::HistoryShortcutAction) return QStringLiteral("history");
    return QStringLiteral("mode_%1").arg(action);
}

QString description(int action, bool english) {
    static const QStringList en{
        QStringLiteral("Start distance or path measurement"), QStringLiteral("Start area and perimeter measurement"),
        QStringLiteral("Start perimeter measurement"), QStringLiteral("Start automatic area measurement"),
        QStringLiteral("Start angle measurement"), QStringLiteral("Start counting"),
        QStringLiteral("Start scale calibration"), QStringLiteral("Open magnifier mode")};
    static const QStringList ru{
        QStringLiteral("Начать измерение линии или пути"), QStringLiteral("Начать измерение площади и периметра"),
        QStringLiteral("Начать измерение периметра"), QStringLiteral("Начать автоматическое измерение области"),
        QStringLiteral("Начать измерение угла"), QStringLiteral("Начать подсчёт"),
        QStringLiteral("Начать калибровку масштаба"), QStringLiteral("Открыть режим лупы")};
    if (action == ruler::CaptureShortcutAction)
        return english ? QStringLiteral("Capture the screen") : QStringLiteral("Сделать снимок экрана");
    if (action == ruler::HistoryShortcutAction)
        return english ? QStringLiteral("Show measurement history") : QStringLiteral("Показать историю измерений");
    if (action < 0 || action >= en.size()) return {};
    return english ? en[action] : ru[action];
}

std::optional<QString> preferredTrigger(const QKeySequence& sequence) {
    if (sequence.count() != 1) return std::nullopt;
    const QKeyCombination combination = sequence[0];
    const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    QStringList parts;
    if (modifiers.testFlag(Qt::ControlModifier)) parts << QStringLiteral("CTRL");
    if (modifiers.testFlag(Qt::AltModifier)) parts << QStringLiteral("ALT");
    if (modifiers.testFlag(Qt::ShiftModifier)) parts << QStringLiteral("SHIFT");
    if (modifiers.testFlag(Qt::KeypadModifier)) parts << QStringLiteral("NUM");
    if (modifiers.testFlag(Qt::MetaModifier)) parts << QStringLiteral("LOGO");

    const int key = combination.key();
    QString name;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        name = QChar(QLatin1Char('a').unicode() + key - Qt::Key_A);
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        name = QChar(QLatin1Char('0').unicode() + key - Qt::Key_0);
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        name = QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);
    else {
        switch (key) {
        case Qt::Key_Return: name = QStringLiteral("Return"); break;
        case Qt::Key_Enter: name = QStringLiteral("KP_Enter"); break;
        case Qt::Key_Escape: name = QStringLiteral("Escape"); break;
        case Qt::Key_Tab: name = QStringLiteral("Tab"); break;
        case Qt::Key_Backspace: name = QStringLiteral("BackSpace"); break;
        case Qt::Key_Delete: name = QStringLiteral("Delete"); break;
        case Qt::Key_Insert: name = QStringLiteral("Insert"); break;
        case Qt::Key_Home: name = QStringLiteral("Home"); break;
        case Qt::Key_End: name = QStringLiteral("End"); break;
        case Qt::Key_PageUp: name = QStringLiteral("Page_Up"); break;
        case Qt::Key_PageDown: name = QStringLiteral("Page_Down"); break;
        case Qt::Key_Left: name = QStringLiteral("Left"); break;
        case Qt::Key_Right: name = QStringLiteral("Right"); break;
        case Qt::Key_Up: name = QStringLiteral("Up"); break;
        case Qt::Key_Down: name = QStringLiteral("Down"); break;
        case Qt::Key_Space: name = QStringLiteral("space"); break;
        case Qt::Key_Comma: name = QStringLiteral("comma"); break;
        case Qt::Key_Period: name = QStringLiteral("period"); break;
        case Qt::Key_Slash: name = QStringLiteral("slash"); break;
        case Qt::Key_Backslash: name = QStringLiteral("backslash"); break;
        case Qt::Key_Minus: name = QStringLiteral("minus"); break;
        case Qt::Key_Equal: name = QStringLiteral("equal"); break;
        case Qt::Key_BracketLeft: name = QStringLiteral("bracketleft"); break;
        case Qt::Key_BracketRight: name = QStringLiteral("bracketright"); break;
        case Qt::Key_Semicolon: name = QStringLiteral("semicolon"); break;
        case Qt::Key_Apostrophe: name = QStringLiteral("apostrophe"); break;
        case Qt::Key_QuoteLeft: name = QStringLiteral("grave"); break;
        default: return std::nullopt;
        }
    }
    parts << name;
    return parts.join(QLatin1Char('+'));
}

QString expectedRequestPath(const QDBusConnection& bus, const QString& token) {
    QString sender = bus.baseService();
    if (sender.startsWith(QLatin1Char(':'))) sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
}

QString portalBusConnectionName() {
    return QStringLiteral("desktop_ruler_shortcuts_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-')));
}
} // namespace

namespace ruler {

GlobalShortcutPortal::GlobalShortcutPortal(ActionCallback actionCallback,
                                           StatusCallback statusCallback,
                                           QObject* parent)
    : QObject(parent), actionCallback_(std::move(actionCallback)),
      statusCallback_(std::move(statusCallback)), busConnectionName_(portalBusConnectionName()),
      bus_(QDBusConnection::connectToBus(QDBusConnection::SessionBus, busConnectionName_)) {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<PortalShortcuts>();
}

GlobalShortcutPortal::~GlobalShortcutPortal() {
    closePendingRequest();
    closeSession();
    bus_.disconnect(QString::fromLatin1(kPortalService), QString::fromLatin1(kPortalPath),
                    QString::fromLatin1(kPortalInterface), QStringLiteral("Activated"),
                    this, SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    QDBusConnection::disconnectFromBus(busConnectionName_);
}

void GlobalShortcutPortal::setEnabled(bool enabled, const QHash<int, QKeySequence>& bindings,
                                      bool english) {
    english_ = english;
    if (!enabled) {
        if (!enabled_ && pendingKind_ == RequestKind::None && sessionHandle_.isEmpty()) return;
        enabled_ = false;
        closePendingRequest();
        closeSession();
        notify(true, english_ ? QStringLiteral("Global shortcuts disabled.")
                              : QStringLiteral("Глобальные сочетания отключены."));
        return;
    }
    if (enabled_ && bindings == bindings_) return;
    if (enabled_ || pendingKind_ != RequestKind::None) {
        enabled_ = false;
        closePendingRequest();
        closeSession();
    }
    enabled_ = true;
    bindings_ = bindings;

    if (!bus_.isConnected()) {
        fail(english_ ? QStringLiteral("The user D-Bus session is unavailable.")
                      : QStringLiteral("Недоступна пользовательская шина D-Bus."));
        return;
    }
    // Register the peer before any other portal call: the portal associates
    // the application ID with this D-Bus connection on its first request.
    if (!registerHostApplication()) return;

    QDBusInterface portal(QString::fromLatin1(kPortalService), QString::fromLatin1(kPortalPath),
                          QString::fromLatin1(kPortalInterface), bus_);
    if (!portal.isValid() || portal.property("version").toUInt() < 1) {
        fail(english_ ? QStringLiteral("The desktop does not provide the global shortcut portal.")
                      : QStringLiteral("В окружении нет портала глобальных сочетаний клавиш."));
        return;
    }
    if (!bus_.connect(QString::fromLatin1(kPortalService), QString::fromLatin1(kPortalPath),
                      QString::fromLatin1(kPortalInterface), QStringLiteral("Activated"), this,
                      SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)))) {
        fail(english_ ? QStringLiteral("Could not listen for global shortcut activations.")
                      : QStringLiteral("Не удалось подключить обработчик глобальных сочетаний."));
        return;
    }
    beginCreateSession();
}

bool GlobalShortcutPortal::registerHostApplication() {
    if (hostApplicationRegistered_) return true;

    QDBusInterface registry(QString::fromLatin1(kPortalService), QString::fromLatin1(kPortalPath),
                            QStringLiteral("org.freedesktop.host.portal.Registry"), bus_);
    QString appId;
    if (auto* guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        appId = guiApp->desktopFileName();
    if (appId.isEmpty()) appId = QCoreApplication::applicationName();
    if (appId.endsWith(QStringLiteral(".desktop"))) appId.chop(8);
    if (appId.isEmpty()) {
        fail(english_ ? QStringLiteral("The desktop did not provide an application ID required by the shortcut portal.")
                      : QStringLiteral("Окружение не предоставило ID приложения, необходимый порталу сочетаний клавиш."));
        return false;
    }

    const QDBusMessage reply = registry.call(QDBus::Block, QStringLiteral("Register"), appId, QVariantMap{});
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod") ||
            reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.UnknownInterface"))
            return true; // Older portals can still identify the application automatically.
        if (reply.errorMessage().contains(QStringLiteral("already associated with an application ID"),
                                          Qt::CaseInsensitive)) {
            // Qt or the launcher may already have registered this peer before
            // this optional integration was reached.
            hostApplicationRegistered_ = true;
            return true;
        }
        const QString detail = QStringLiteral("%1: %2").arg(reply.errorName(), reply.errorMessage());
        fail(english_ ? QStringLiteral("Could not register the application with the desktop portal: %1").arg(detail)
                      : QStringLiteral("Не удалось зарегистрировать приложение в портале окружения: %1").arg(detail));
        return false;
    }
    hostApplicationRegistered_ = true;
    return true;
}

void GlobalShortcutPortal::beginCreateSession() {
    const QString token = QStringLiteral("desktop_ruler_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-')));
    if (!watchRequest(token, RequestKind::CreateSession)) return;

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    options.insert(QStringLiteral("session_handle_token"), token + QStringLiteral("_session"));
    QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(kPortalService),
        QString::fromLatin1(kPortalPath), QString::fromLatin1(kPortalInterface), QStringLiteral("CreateSession"));
    call << options;
    const QString expected = pendingPath_;
    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(call), this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, expected](QDBusPendingCallWatcher* done) {
            requestReplyReceived(expected, done->reply());
            done->deleteLater();
        });
}

void GlobalShortcutPortal::beginBindShortcuts() {
    PortalShortcuts requested;
    actionByPortalId_.clear();
    for (auto it = bindings_.cbegin(); it != bindings_.cend(); ++it) {
        const auto trigger = preferredTrigger(it.value());
        if (!trigger) continue;
        const QString id = portalId(it.key());
        QVariantMap properties;
        properties.insert(QStringLiteral("description"), description(it.key(), english_));
        properties.insert(QStringLiteral("preferred_trigger"), *trigger);
        requested.append(PortalShortcut{id, properties});
        actionByPortalId_.insert(id, it.key());
    }
    if (requested.isEmpty()) {
        fail(english_ ? QStringLiteral("No configured shortcut can be registered globally; local shortcuts remain active.")
                      : QStringLiteral("Нет сочетаний, которые можно зарегистрировать глобально; локальные сочетания работают."));
        return;
    }

    const QString token = QStringLiteral("desktop_ruler_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-')));
    if (!watchRequest(token, RequestKind::BindShortcuts)) return;

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), token);
    QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(kPortalService),
        QString::fromLatin1(kPortalPath), QString::fromLatin1(kPortalInterface), QStringLiteral("BindShortcuts"));
    call << QDBusObjectPath(sessionHandle_) << QVariant::fromValue(requested) << QString() << options;
    const QString expected = pendingPath_;
    auto* watcher = new QDBusPendingCallWatcher(bus_.asyncCall(call), this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, expected](QDBusPendingCallWatcher* done) {
            requestReplyReceived(expected, done->reply());
            done->deleteLater();
        });
}

bool GlobalShortcutPortal::watchRequest(const QString& token, RequestKind kind) {
    pendingPath_ = expectedRequestPath(bus_, token);
    pendingKind_ = kind;
    const bool connected = bus_.connect(QString::fromLatin1(kPortalService), pendingPath_,
        QString::fromLatin1(kRequestInterface), QStringLiteral("Response"), this,
        SLOT(onRequestResponse(uint,QVariantMap)));
    if (!connected) {
        pendingPath_.clear();
        pendingKind_ = RequestKind::None;
        fail(english_ ? QStringLiteral("Could not subscribe to the portal response.")
                      : QStringLiteral("Не удалось ожидать ответ портала."));
        return false;
    }
    return true;
}

void GlobalShortcutPortal::requestReplyReceived(const QString& expectedPath, const QDBusMessage& reply) {
    if (pendingPath_ != expectedPath || pendingKind_ == RequestKind::None) return;
    if (reply.type() == QDBusMessage::ErrorMessage) {
        const QString detail = QStringLiteral("%1: %2").arg(reply.errorName(), reply.errorMessage());
        fail(english_ ? QStringLiteral("The desktop shortcut portal rejected the request: %1").arg(detail)
                      : QStringLiteral("Портал сочетаний клавиш отклонил запрос: %1").arg(detail));
        return;
    }
    if (reply.arguments().isEmpty()) {
        fail(english_ ? QStringLiteral("The desktop shortcut portal returned an empty response.")
                      : QStringLiteral("Портал сочетаний клавиш вернул пустой ответ."));
        return;
    }
    const QString actualPath = qvariant_cast<QDBusObjectPath>(reply.arguments().constFirst()).path();
    if (actualPath.isEmpty()) {
        fail(english_ ? QStringLiteral("The desktop returned an invalid portal request handle.")
                      : QStringLiteral("Окружение вернуло неверный адрес запроса портала."));
        return;
    }
    if (actualPath == pendingPath_) return;

    bus_.disconnect(QString::fromLatin1(kPortalService), pendingPath_,
                    QString::fromLatin1(kRequestInterface), QStringLiteral("Response"), this,
                    SLOT(onRequestResponse(uint,QVariantMap)));
    pendingPath_ = actualPath;
    if (!bus_.connect(QString::fromLatin1(kPortalService), pendingPath_,
                      QString::fromLatin1(kRequestInterface), QStringLiteral("Response"), this,
                      SLOT(onRequestResponse(uint,QVariantMap))))
        fail(english_ ? QStringLiteral("Could not follow the portal request response.")
                      : QStringLiteral("Не удалось подключиться к ответу портала."));
}

void GlobalShortcutPortal::onRequestResponse(uint response, const QVariantMap& results) {
    const RequestKind kind = pendingKind_;
    if (kind == RequestKind::None) return;
    bus_.disconnect(QString::fromLatin1(kPortalService), pendingPath_,
                    QString::fromLatin1(kRequestInterface), QStringLiteral("Response"), this,
                    SLOT(onRequestResponse(uint,QVariantMap)));
    pendingPath_.clear();
    pendingKind_ = RequestKind::None;

    if (response != 0) {
        fail(response == 1
            ? (english_ ? QStringLiteral("Global shortcut setup was canceled; local shortcuts remain available.")
                        : QStringLiteral("Настройка глобальных сочетаний отменена; локальные сочетания работают."))
            : (english_ ? QStringLiteral("The desktop could not complete global shortcut setup; local shortcuts remain available.")
                        : QStringLiteral("Не удалось завершить настройку глобальных сочетаний; локальные сочетания работают.")));
        return;
    }

    if (kind == RequestKind::CreateSession) {
        sessionHandle_ = results.value(QStringLiteral("session_handle")).toString();
        if (sessionHandle_.isEmpty() || !sessionHandle_.startsWith(QLatin1Char('/'))) {
            fail(english_ ? QStringLiteral("The portal returned an invalid shortcut session.")
                          : QStringLiteral("Портал вернул неверный сеанс сочетаний клавиш."));
            return;
        }
        beginBindShortcuts();
        return;
    }

    const PortalShortcuts bound = qdbus_cast<PortalShortcuts>(results.value(QStringLiteral("shortcuts")));
    const int requestedCount = actionByPortalId_.size();
    QHash<QString, int> accepted;
    for (const auto& entry : bound) {
        const QString id = entry.id;
        if (actionByPortalId_.contains(id)) accepted.insert(id, actionByPortalId_.value(id));
    }
    actionByPortalId_ = accepted;
    if (accepted.isEmpty()) {
        fail(english_ ? QStringLiteral("The portal accepted no global shortcuts; local shortcuts remain available.")
                      : QStringLiteral("Портал не принял глобальные сочетания; локальные сочетания работают."));
        return;
    }
    const int acceptedCount = accepted.size();
    notify(true, english_
        ? QStringLiteral("Registered %1 of %2 actions with the desktop. This does not assign keys; configure them in your desktop or compositor settings.").arg(acceptedCount).arg(requestedCount)
        : QStringLiteral("Зарегистрировано действий в окружении: %1 из %2. Это не назначает клавиши; настройте сочетания в параметрах окружения или композитора.").arg(acceptedCount).arg(requestedCount));
}

void GlobalShortcutPortal::onActivated(const QDBusObjectPath& sessionHandle,
                                       const QString& shortcutId, qulonglong timestamp,
                                       const QVariantMap& options) {
    Q_UNUSED(timestamp)
    Q_UNUSED(options)
    if (!enabled_ || sessionHandle.path() != sessionHandle_) return;
    const auto it = actionByPortalId_.constFind(shortcutId);
    if (it != actionByPortalId_.cend() && actionCallback_) actionCallback_(it.value());
}

void GlobalShortcutPortal::fail(const QString& detail) {
    enabled_ = false;
    closePendingRequest();
    closeSession();
    actionByPortalId_.clear();
    bus_.disconnect(QString::fromLatin1(kPortalService), QString::fromLatin1(kPortalPath),
                    QString::fromLatin1(kPortalInterface), QStringLiteral("Activated"), this,
                    SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    notify(false, detail);
}

void GlobalShortcutPortal::closePendingRequest() {
    if (pendingPath_.isEmpty()) return;
    bus_.disconnect(QString::fromLatin1(kPortalService), pendingPath_,
                    QString::fromLatin1(kRequestInterface), QStringLiteral("Response"), this,
                    SLOT(onRequestResponse(uint,QVariantMap)));
    QDBusMessage close = QDBusMessage::createMethodCall(QString::fromLatin1(kPortalService), pendingPath_,
        QString::fromLatin1(kRequestInterface), QStringLiteral("Close"));
    bus_.asyncCall(close);
    pendingPath_.clear();
    pendingKind_ = RequestKind::None;
}

void GlobalShortcutPortal::closeSession() {
    bus_.disconnect(QString::fromLatin1(kPortalService), QString::fromLatin1(kPortalPath),
                    QString::fromLatin1(kPortalInterface), QStringLiteral("Activated"), this,
                    SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    if (sessionHandle_.isEmpty()) return;
    QDBusMessage close = QDBusMessage::createMethodCall(QString::fromLatin1(kPortalService), sessionHandle_,
        QString::fromLatin1(kSessionInterface), QStringLiteral("Close"));
    bus_.asyncCall(close);
    sessionHandle_.clear();
    actionByPortalId_.clear();
}

void GlobalShortcutPortal::notify(bool success, const QString& text) {
    if (statusCallback_) statusCallback_(success, text);
}

} // namespace ruler

#include "global_shortcuts.moc"
