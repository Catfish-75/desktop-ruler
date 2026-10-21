#include "../shortcuts.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

class ShortcutSettingsTest final : public QObject {
    Q_OBJECT
private slots:
    void defaultsCancelConflictAndSave() {
        QTemporaryDir settingsDir;
        QVERIFY(settingsDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("desktop-ruler"), QStringLiteral("desktop-ruler"));
        settings.clear();
        settings.sync();

        QWidget host;
        ruler::ShortcutController controller(&host, [](int) {});

        bool defaultsShown = false;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            const auto edits = dialog->findChildren<QKeySequenceEdit*>();
            defaultsShown = edits.size() == 10 &&
                edits[0]->keySequence() == QKeySequence(QStringLiteral("Ctrl+Alt+1")) &&
                edits[7]->keySequence() == QKeySequence(QStringLiteral("Ctrl+Alt+8")) &&
                edits[8]->keySequence() == QKeySequence(QStringLiteral("Ctrl+Alt+C")) &&
                edits[9]->keySequence() == QKeySequence(QStringLiteral("Ctrl+Alt+H"));
            dialog->reject();
        });
        controller.showSettings(true);
        QVERIFY(defaultsShown);
        settings.sync();
        QVERIFY(settings.allKeys().isEmpty());

        bool conflictRejected = false;
        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            const auto edits = dialog->findChildren<QKeySequenceEdit*>();
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            if (edits.size() != 10 || !buttons) { dialog->reject(); return; }
            edits[0]->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+9")));
            edits[1]->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+9")));
            QTimer::singleShot(0, [dialog] {
                if (auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                    warning->accept();
            });
            buttons->button(QDialogButtonBox::Ok)->click();
            conflictRejected = dialog->isVisible();
            dialog->reject();
        });
        controller.showSettings(true);
        QVERIFY(conflictRejected);
        settings.sync();
        QVERIFY(settings.allKeys().isEmpty());

        QTimer::singleShot(0, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            const auto edits = dialog->findChildren<QKeySequenceEdit*>();
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            if (edits.size() != 10 || !buttons) { dialog->reject(); return; }
            edits[0]->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+9")));
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        controller.showSettings(true);
        settings.sync();
        QCOMPARE(settings.value(QStringLiteral("shortcuts/mode/0")).toString(),
                 QStringLiteral("Ctrl+Shift+9"));
    }
};

QTEST_MAIN(ShortcutSettingsTest)
#include "test_shortcuts.moc"
