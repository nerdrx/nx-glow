#include <QApplication>
#include <QSpinBox>
#include <QSlider>
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
    auto *slider = module->widget()->findChild<QSlider *>("RadiusSlider");
    check(slider);
    slider->setValue(220);
    check(radius->value() == 220); strength->setValue(125); saturation->setValue(75);
    check(module->needsSave());
    module->save();
    KConfigGroup group(KSharedConfig::openConfig("kwinrc"), "Effect-nxglow");
    check(group.readEntry("Radius", 0.) == 220 && group.readEntry("Strength", 0.) == 1.25
          && group.readEntry("Saturation", 0.) == .75 && !module->needsSave());
    module->defaults();
    check(radius->value() == 110 && slider->value() == 110 && strength->value() == 85 && saturation->value() == 100);
    check(module->needsSave());
    module->load();
    check(radius->value() == 220 && slider->value() == 220 && !module->needsSave());
    module->widget()->adjustSize();
    module->widget()->show();
    QTimer::singleShot(300, [&] {
        check(module->widget()->grab().save(QString::fromLocal8Bit(argv[2])));
        qInfo("PASS: native plugin loading, save, defaults, cancel/reload and rendering");
        QPalette dark = app.palette();
        dark.setColor(QPalette::Window, QColor("#191323"));
        dark.setColor(QPalette::WindowText, QColor("#eee8f5"));
        dark.setColor(QPalette::Base, QColor("#21192f"));
        dark.setColor(QPalette::AlternateBase, QColor("#241a34"));
        dark.setColor(QPalette::Text, QColor("#eee8f5"));
        dark.setColor(QPalette::Button, QColor("#332843"));
        dark.setColor(QPalette::ButtonText, QColor("#eee8f5"));
        dark.setColor(QPalette::Highlight, QColor("#a875f3"));
        app.setPalette(dark);
        QTimer::singleShot(100, [&] {
            check(module->widget()->grab().save(QString::fromLocal8Bit(argv[2]) + ".dark.png"));
            app.quit();
        });
    });
    return app.exec();
}
