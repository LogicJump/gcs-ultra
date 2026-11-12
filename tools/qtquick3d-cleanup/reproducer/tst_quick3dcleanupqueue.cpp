#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopedPointer>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {

#ifdef Q_OS_WIN
void verifyLoadedModule(const wchar_t* moduleName)
{
    const HMODULE module = GetModuleHandleW(moduleName);
    QVERIFY(module);

    wchar_t modulePath[MAX_PATH] = {};
    const DWORD modulePathLength = GetModuleFileNameW(module, modulePath, MAX_PATH);
    QVERIFY(modulePathLength > 0);

    const QString loadedPath = QString::fromWCharArray(modulePath, int(modulePathLength));
    qInfo().noquote() << "Loaded module:" << loadedPath;

    const QString overrideRoot = qEnvironmentVariable("QT_QUICK3D_OVERRIDE_ROOT");
    if (!overrideRoot.isEmpty())
        QVERIFY2(loadedPath.startsWith(overrideRoot, Qt::CaseInsensitive), qPrintable(loadedPath));
}
#endif

}  // namespace

class tst_Quick3DCleanupQueue : public QObject
{
    Q_OBJECT

private slots:
    void lastViewDestructionDrainsPendingResources();
};

void tst_Quick3DCleanupQueue::lastViewDestructionDrainsPendingResources()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setSource(QUrl(QStringLiteral("qrc:/cleanupqueue.qml")));
    QCOMPARE(view->status(), QQuickView::Ready);
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view.data()));

    QSignalSpy frameSpy(view.data(), &QQuickWindow::afterFrameEnd);
    view->requestUpdate();
    QVERIFY(frameSpy.wait(5000));

#ifdef Q_OS_WIN
    verifyLoadedModule(L"Qt6Quick3DUtilsd.dll");
    verifyLoadedModule(L"Qt6Quick3DRuntimeRenderd.dll");
    verifyLoadedModule(L"Qt6Quick3Dd.dll");
    verifyLoadedModule(L"qquick3dplugind.dll");
#endif

    QObject* root = view->rootObject();
    QVERIFY(root);
    QVERIFY(root->setProperty("loadView", false));
    QTRY_VERIFY_WITH_TIMEOUT(!root->property("viewLoaded").toBool(), 5000);

    frameSpy.clear();
    view->requestUpdate();
    QVERIFY(frameSpy.wait(5000));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QTest::failOnWarning(QRegularExpression(
        QStringLiteral("Pending resource cleanup queue not empty, but no RCI or window to clean it up!")));
    view.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

QTEST_MAIN(tst_Quick3DCleanupQueue)

#include "tst_quick3dcleanupqueue.moc"
