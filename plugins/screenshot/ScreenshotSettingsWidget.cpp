#include "ScreenshotSettingsWidget.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <utility>

ScreenshotSettingsWidget::ScreenshotSettingsWidget(const Values &values, QWidget *parent,
                                                   std::function<void(const Values &)> changed,
                                                   std::function<int()> clearHistory)
    : QWidget(parent)
    , m_values(values)
    , m_changed(std::move(changed))
    , m_clearHistory(std::move(clearHistory))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *form = new QFormLayout;
    form->setSpacing(8);

    auto *dirRow = new QHBoxLayout;
    m_saveDir = new QLineEdit(values.saveDir, this);
    m_saveDir->setPlaceholderText(tr("Pictures\\TypeGo Screenshots"));
    auto *browse = new QPushButton(tr("Browse..."), this);
    dirRow->addWidget(m_saveDir, 1);
    dirRow->addWidget(browse);
    form->addRow(tr("Save folder:"), dirRow);

    m_historyLimit = new QSpinBox(this);
    m_historyLimit->setRange(0, 200);
    m_historyLimit->setValue(qBound(0, values.historyLimit, 200));
    m_historyLimit->setToolTip(tr("0 disables screenshot history"));
    form->addRow(tr("History size:"), m_historyLimit);

    m_magnifier = new QCheckBox(tr("Show magnifier while selecting"), this);
    m_magnifier->setChecked(values.magnifier);
    form->addRow(QString(), m_magnifier);

    m_magnifierZoom = new QSpinBox(this);
    m_magnifierZoom->setRange(2, 20);
    m_magnifierZoom->setValue(qBound(2, values.magnifierZoom, 20));
    m_magnifierZoom->setSuffix(tr("x"));
    form->addRow(tr("Magnifier zoom:"), m_magnifierZoom);
    layout->addLayout(form);

    auto *historyRow = new QHBoxLayout;
    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("muted"));
    auto *clear = new QPushButton(tr("Clear history"), this);
    historyRow->addWidget(m_count, 1);
    historyRow->addWidget(clear);
    layout->addLayout(historyRow);

    auto *hint = new QLabel(tr("Default shortcut: F1 (Ctrl+Z undo, Ctrl+S save, Enter copy, Ctrl+P pin)."), this);
    hint->setWordWrap(true);
    hint->setObjectName(QStringLiteral("muted"));
    layout->addWidget(hint);
    layout->addStretch();
    updateCount(values.historyCount);

    connect(browse, &QPushButton::clicked, this, &ScreenshotSettingsWidget::browse);
    connect(m_saveDir, &QLineEdit::editingFinished, this, [this]() {
        m_values.saveDir = m_saveDir->text().trimmed();
        notify();
    });
    connect(m_historyLimit, &QSpinBox::valueChanged, this, [this](int value) {
        m_values.historyLimit = value;
        notify();
    });
    connect(m_magnifier, &QCheckBox::toggled, this, [this](bool checked) {
        m_values.magnifier = checked;
        notify();
    });
    connect(m_magnifierZoom, &QSpinBox::valueChanged, this, [this](int value) {
        m_values.magnifierZoom = value;
        notify();
    });
    connect(clear, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, tr("Clear screenshot history"),
                tr("Delete all saved screenshots? This cannot be undone."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        if (m_clearHistory) updateCount(m_clearHistory());
    });
}

void ScreenshotSettingsWidget::notify()
{
    if (m_changed) m_changed(m_values);
}

void ScreenshotSettingsWidget::updateCount(int count)
{
    m_values.historyCount = count;
    m_count->setText(tr("%n screenshot(s) in history", nullptr, count));
}

void ScreenshotSettingsWidget::browse()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose save folder"), m_saveDir->text());
    if (dir.isEmpty()) return;
    m_saveDir->setText(QDir::toNativeSeparators(dir));
    m_values.saveDir = dir;
    notify();
}