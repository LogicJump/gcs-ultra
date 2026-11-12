#include "AppSettingsTest.h"

#include <QtCore/QLocale>
#include <QtCore/QScopeGuard>
#include <QtCore/QSettings>

#include "AppSettings.h"
#include "FirmwarePluginManager.h"
#include "QGCApplication.h"
#include "QGCMAVLink.h"
#include "SettingsManager.h"

UT_REGISTER_TEST(AppSettingsTest, TestLabel::Unit)

void AppSettingsTest::_defaultTestLanguageIsEnglish()
{
    QVERIFY(qgcApp());
    QCOMPARE(qgcApp()->getCurrentLanguage().language(), QLocale::English);
    QCOMPARE(QLocale().language(), QLocale::English);
}

void AppSettingsTest::_explicitLanguageCanReturnToEnglish()
{
    QVERIFY(qgcApp());
    QSettings settings;
    const QVariant savedLanguage = settings.value(AppSettings::qLocaleLanguageName);
    const auto restoreLanguage = qScopeGuard([&settings, savedLanguage] {
        if (savedLanguage.isValid()) {
            settings.setValue(AppSettings::qLocaleLanguageName, savedLanguage);
        } else {
            settings.remove(AppSettings::qLocaleLanguageName);
        }
        qgcApp()->setLanguage();
    });

    settings.setValue(AppSettings::qLocaleLanguageName, QLocale::Chinese);
    qgcApp()->setLanguage();
    QCOMPARE(qgcApp()->getCurrentLanguage().language(), QLocale::Chinese);
    QCOMPARE(QLocale().language(), QLocale::Chinese);

    settings.setValue(AppSettings::qLocaleLanguageName, QLocale::English);
    qgcApp()->setLanguage();
    QCOMPARE(qgcApp()->getCurrentLanguage().language(), QLocale::English);
    QCOMPARE(QLocale().language(), QLocale::English);
}

void AppSettingsTest::_preferredFirmwareClassEnumFiltered()
{
    _verifyFirmwareClassEnumFiltered(SettingsManager::instance()->appSettings()->preferredFirmwareClass());
}

void AppSettingsTest::_offlineEditingFirmwareClassEnumFiltered()
{
    _verifyFirmwareClassEnumFiltered(SettingsManager::instance()->appSettings()->offlineEditingFirmwareClass());
}

void AppSettingsTest::_verifyFirmwareClassEnumFiltered(Fact *fact)
{
    const QList<QGCMAVLink::FirmwareClass_t> supportedClasses = FirmwarePluginManager::instance()->supportedFirmwareClasses();

    const QVariantList enumValues = fact->enumValues();
    QCOMPARE(fact->enumStrings().count(), enumValues.count());
    QVERIFY(!enumValues.isEmpty());

    for (const QVariant &enumValue : enumValues) {
        const auto firmwareClass = static_cast<QGCMAVLink::FirmwareClass_t>(enumValue.toUInt());
        QVERIFY2(supportedClasses.contains(firmwareClass),
                 qPrintable(QStringLiteral("%1 enum offers unsupported firmware class %2").arg(fact->name()).arg(enumValue.toUInt())));
    }

    QVERIFY2(supportedClasses.contains(static_cast<QGCMAVLink::FirmwareClass_t>(fact->rawValue().toUInt())),
             qPrintable(QStringLiteral("%1 value is an unsupported firmware class %2").arg(fact->name()).arg(fact->rawValue().toUInt())));
}
