#include <QApplication>
#include <QSpinBox>
#include <QPixmap>
#include <QTimer>
#include <KPluginFactory>
#include <KCModule>
#include <KConfigGroup>
#include <KSharedConfig>
#include <cstdlib>

void check(bool condition) { if (!condition) qFatal("Native settings check failed"); }
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    check(argc == 3);
    KPluginMetaData metadata(QString::fromLocal8Bit(argv[1]), KPluginMetaData::AllowEmptyMetaData);
    auto result = KPluginFactory::instantiatePlugin<KCModule>(metadata);
    check(bool(result));
    auto *module = result.plugin;
    auto *radius = module->widget()->findChild<QSpinBox *>("Radius");
    auto *strength = module->widget()->findChild<QSpinBox *>("Strength");
    auto *saturation = module->widget()->findChild<QSpinBox *>("Saturation");
    check(radius && strength && saturation);
    radius->setValue(220); strength->setValue(125); saturation->setValue(75);
    check(module->needsSave());
    module->save();
    KConfigGroup group(KSharedConfig::openConfig("kwinrc"), "Effect-nxglow");
    check(group.readEntry("Radius", 0.) == 220 && group.readEntry("Strength", 0.) == 1.25
          && group.readEntry("Saturation", 0.) == .75 && !module->needsSave());
    module->defaults();
    check(radius->value() == 110 && strength->value() == 85 && saturation->value() == 100);
    check(module->needsSave());
    module->load();
    check(radius->value() == 220 && !module->needsSave());
    module->widget()->resize(440, 180);
    module->widget()->show();
    QTimer::singleShot(300, [&] {
        check(module->widget()->grab().save(QString::fromLocal8Bit(argv[2])));
        qInfo("PASS: native plugin loading, save, defaults, cancel/reload and rendering");
        app.quit();
    });
    return app.exec();
}
