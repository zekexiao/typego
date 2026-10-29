#include "ClipboardSettingsWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <utility>

ClipboardSettingsWidget::ClipboardSettingsWidget(const Values &values, QWidget *parent,
                                                 std::function<void(const Values &)> changed,
                                                 std::function<int()> clearHistory)
    : QWidget(parent), m_values(values), m_changed(std::move(changed)), m_clearHistory(std::move(clearHistory))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *form = new QFormLayout;
    form->setSpacing(8);
    m_maxItems = new QSpinBox(this);
    m_maxItems->setRange(10, 200);
    m_maxItems->setValue(qBound(10, values.maxItems, 200));
    form->addRow(tr("History size:"), m_maxItems);

    m_captureText = new QCheckBox(tr("Record copied text"), this);
    m_captureText->setChecked(values.captureText);
    form->addRow(QString(), m_captureText);
    m_captureImages = new QCheckBox(tr("Record copied images"), this);
    m_captureImages->setChecked(values.captureImages);
    form->addRow(QString(), m_captureImages);
    m_captureFiles = new QCheckBox(tr("Record copied files"), this);
    m_captureFiles->setChecked(values.captureFiles);
    form->addRow(QString(), m_captureFiles);
    layout->addLayout(form);

    auto *historyRow = new QHBoxLayout;
    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("muted"));
    historyRow->addWidget(m_count, 1);
    auto *clear = new QPushButton(tr("Clear history"), this);
    historyRow->addWidget(clear);
    layout->addLayout(historyRow);
    layout->addStretch();
    updateCount(values.count);

    connect(m_maxItems, &QSpinBox::valueChanged, this, [this](int value) {
        m_values.maxItems = value;
        notify();
    });
    connect(m_captureText, &QCheckBox::toggled, this, [this](bool checked) {
        m_values.captureText = checked;
        notify();
    });
    connect(m_captureImages, &QCheckBox::toggled, this, [this](bool checked) {
        m_values.captureImages = checked;
        notify();
    });
    connect(m_captureFiles, &QCheckBox::toggled, this, [this](bool checked) {
        m_values.captureFiles = checked;
        notify();
    });
    connect(clear, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, tr("Clear clipboard history"),
                tr("Delete all recorded clipboard items? This cannot be undone."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        if (m_clearHistory) updateCount(m_clearHistory());
    });
}

void ClipboardSettingsWidget::notify()
{
    if (m_changed) m_changed(m_values);
}

void ClipboardSettingsWidget::updateCount(int count)
{
    m_values.count = count;
    m_count->setText(tr("%n item(s) recorded", nullptr, count));
}