// SPDX-License-Identifier: GPL-2.0-or-later
#include <KCModule>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KPluginFactory>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPainter>
#include <QSpinBox>
#include <QSignalBlocker>
#include <array>

class GlowPreview final : public QWidget
{
public:
    explicit GlowPreview(QWidget *parent) : QWidget(parent) { setFixedHeight(120); }
    int radius = 110, strength = 85, saturation = 100;
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::AlternateBase));
        painter.drawRoundedRect(rect(), 12, 12);
        const QPointF center(width() / 2., 60);
        for (int side : {-1, 1}) {
            QColor colour = QColor::fromHsv(side < 0 ? 265 : 185, qMin(255, saturation * 127 / 100), 235);
            colour.setAlpha(qMin(220, strength));
            QRadialGradient glow(center + QPointF(side * 38, 0), 35 + radius * .24);
            glow.setColorAt(0, colour);
            colour.setAlpha(0);
            glow.setColorAt(1, colour);
            painter.setBrush(glow);
            painter.drawRoundedRect(rect(), 12, 12);
        }
        QRectF window(center.x() - 65, 31, 130, 58);
        painter.setBrush(palette().color(QPalette::Base));
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRoundedRect(window, 7, 7);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawRoundedRect(QRectF(window.x() + 12, 45, 42, 5), 2, 2);
        painter.setBrush(palette().color(QPalette::Mid));
        painter.drawRoundedRect(QRectF(window.x() + 12, 59, 100, 4), 2, 2);
        painter.drawRoundedRect(QRectF(window.x() + 12, 70, 72, 4), 2, 2);
    }
};

class GlowConfig final : public KCModule
{
    Q_OBJECT
public:
    GlowConfig(QObject *parent, const KPluginMetaData &data) : KCModule(parent, data)
    {
        setButtons(Default | Apply);
        widget()->setMinimumWidth(460);
        auto *layout = new QVBoxLayout(widget());
        layout->setContentsMargins(24, 20, 24, 20);
        layout->setSpacing(8);
        auto *title = new QLabel(tr("nx glow"), widget());
        auto font = title->font();
        font.setPointSizeF(font.pointSizeF() * 1.7);
        font.setBold(true);
        title->setFont(font);
        layout->addWidget(title);
        layout->addWidget(new QLabel(tr("A little light around everything you do."), widget()));
        layout->addSpacing(8);
        m_preview = new GlowPreview(widget());
        m_preview->setAccessibleName(tr("Illustrative glow preview"));
        layout->addWidget(m_preview);
        auto *previewCaption = new QLabel(tr("Illustrative preview · Apply to update your desktop"), widget());
        previewCaption->setAlignment(Qt::AlignCenter);
        layout->addWidget(previewCaption);
        const std::array<QString, 3> labels{tr("Glow size"), tr("Brightness"), tr("Colour intensity")};
        const std::array<QString, 3> descriptions{tr("From a close halo to a wide, soft wash."),
            tr("Keep it subtle or let your windows shine."), tr("From muted light to rich, vivid colour.")};
        for (size_t i = 0; i < m_controls.size(); ++i) {
            layout->addSpacing(10);
            auto *row = new QHBoxLayout;
            auto *label = new QLabel(labels[i], widget());
            auto labelFont = label->font();
            labelFont.setBold(true);
            label->setFont(labelFont);
            row->addWidget(label);
            row->addStretch();
            auto *control = new QSpinBox(widget());
            control->setObjectName(QString::fromLatin1(keys[i]));
            control->setRange(i == 0 ? 10 : 0, i == 0 ? 300 : 200);
            control->setSuffix(i == 0 ? tr(" px") : tr("%"));
            control->setAccessibleName(labels[i]);
            control->setMinimumWidth(96);
            label->setBuddy(control);
            row->addWidget(control);
            layout->addLayout(row);
            auto *slider = new QSlider(Qt::Horizontal, widget());
            slider->setObjectName(QString::fromLatin1(keys[i]) + QStringLiteral("Slider"));
            slider->setRange(control->minimum(), control->maximum());
            slider->setAccessibleName(labels[i]);
            slider->setPageStep(10);
            layout->addWidget(slider);
            layout->addWidget(new QLabel(descriptions[i], widget()));
            m_controls[i] = control;
            m_sliders[i] = slider;
            connect(slider, &QSlider::valueChanged, control, &QSpinBox::setValue);
            connect(control, &QSpinBox::valueChanged, slider, &QSlider::setValue);
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
            m_sliders[i]->setValue(m_saved[i]);
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
        m_preview->radius = m_controls[0]->value();
        m_preview->strength = m_controls[1]->value();
        m_preview->saturation = m_controls[2]->value();
        m_preview->update();
    }
    static constexpr std::array<const char *, 3> keys{"Radius", "Strength", "Saturation"};
    static constexpr std::array<int, 3> defaultsValues{110, 85, 100};
    std::array<QSpinBox *, 3> m_controls{};
    std::array<QSlider *, 3> m_sliders{};
    GlowPreview *m_preview = nullptr;
    std::array<int, 3> m_saved{};
};
K_PLUGIN_CLASS(GlowConfig)
#include "config.moc"
