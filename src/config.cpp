// SPDX-License-Identifier: GPL-2.0-or-later
#include <KCModule>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KPluginFactory>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFormLayout>
#include <QSpinBox>
#include <QSignalBlocker>
#include <array>

class GlowConfig final : public KCModule
{
    Q_OBJECT
public:
    GlowConfig(QObject *parent, const KPluginMetaData &data) : KCModule(parent, data)
    {
        auto *layout = new QFormLayout(widget());
        const std::array<QString, 3> labels{tr("Glow size:"), tr("Brightness:"), tr("Colour intensity:")};
        for (size_t i = 0; i < m_controls.size(); ++i) {
            auto *control = new QSpinBox(widget());
            control->setObjectName(QString::fromLatin1(keys[i]));
            control->setRange(i == 0 ? 10 : 0, i == 0 ? 300 : 200);
            control->setSuffix(i == 0 ? tr(" px") : tr("%"));
            control->setAccessibleName(labels[i]);
            layout->addRow(labels[i], control);
            m_controls[i] = control;
            connect(control, &QSpinBox::valueChanged, this, &GlowConfig::updateState);
        }
        load();
    }
    void load() override
    {
        auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
        config->reparseConfiguration();
        KConfigGroup group(config, "Effect-nxglow");
        for (size_t i = 0; i < m_controls.size(); ++i) {
            QSignalBlocker blocker(m_controls[i]);
            const double scale = i == 0 ? 1 : 100;
            m_controls[i]->setValue(qRound(group.readEntry(keys[i], defaultsValues[i] / scale) * scale));
            m_saved[i] = m_controls[i]->value();
        }
        updateState();
    }
    void save() override
    {
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")), "Effect-nxglow");
        for (size_t i = 0; i < m_controls.size(); ++i) {
            m_saved[i] = m_controls[i]->value();
            group.writeEntry(keys[i], m_saved[i] / (i == 0 ? 1.0 : 100.0));
        }
        group.sync();
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"),
            QStringLiteral("org.kde.kwin.Effects"), QStringLiteral("reconfigureEffect"));
        message << QStringLiteral("nxglow");
        QDBusConnection::sessionBus().asyncCall(message);
        updateState();
    }
    void defaults() override
    {
        for (size_t i = 0; i < m_controls.size(); ++i) m_controls[i]->setValue(defaultsValues[i]);
        updateState();
    }
private:
    void updateState()
    {
        bool changed = false, defaulted = true;
        for (size_t i = 0; i < m_controls.size(); ++i) {
            changed |= m_controls[i]->value() != m_saved[i];
            defaulted &= m_controls[i]->value() == defaultsValues[i];
        }
        setNeedsSave(changed);
        setRepresentsDefaults(defaulted);
    }
    static constexpr std::array<const char *, 3> keys{"Radius", "Strength", "Saturation"};
    static constexpr std::array<int, 3> defaultsValues{110, 85, 100};
    std::array<QSpinBox *, 3> m_controls{};
    std::array<int, 3> m_saved{};
};
K_PLUGIN_CLASS(GlowConfig)
#include "config.moc"
