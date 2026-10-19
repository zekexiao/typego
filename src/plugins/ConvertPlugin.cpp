#include "ConvertPlugin.h"

#include <QApplication>
#include <QClipboard>
#include <QGridLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace {
struct UnitDef {
    QString key;
    QString label;
    double factorToMeter;
};

const QList<UnitDef> &lengthUnits()
{
    static const QList<UnitDef> units = {
        {QStringLiteral("mm"), QStringLiteral("Millimeter"), 0.001},
        {QStringLiteral("cm"), QStringLiteral("Centimeter"), 0.01},
        {QStringLiteral("m"), QStringLiteral("Meter"), 1.0},
        {QStringLiteral("km"), QStringLiteral("Kilometer"), 1000.0},
        {QStringLiteral("in"), QStringLiteral("Inch"), 0.0254},
        {QStringLiteral("ft"), QStringLiteral("Foot"), 0.3048},
        {QStringLiteral("yd"), QStringLiteral("Yard"), 0.9144},
        {QStringLiteral("mi"), QStringLiteral("Mile"), 1609.344},
    };
    return units;
}

QString normalizedUnit(QString unit)
{
    unit = unit.trimmed().toLower();
    if (unit == QStringLiteral("meter") || unit == QStringLiteral("meters")) return QStringLiteral("m");
    if (unit == QStringLiteral("kilometer") || unit == QStringLiteral("kilometers")) return QStringLiteral("km");
    if (unit == QStringLiteral("centimeter") || unit == QStringLiteral("centimeters")) return QStringLiteral("cm");
    if (unit == QStringLiteral("millimeter") || unit == QStringLiteral("millimeters")) return QStringLiteral("mm");
    if (unit == QStringLiteral("inch") || unit == QStringLiteral("inches")) return QStringLiteral("in");
    if (unit == QStringLiteral("foot") || unit == QStringLiteral("feet")) return QStringLiteral("ft");
    if (unit == QStringLiteral("yard") || unit == QStringLiteral("yards")) return QStringLiteral("yd");
    if (unit == QStringLiteral("mile") || unit == QStringLiteral("miles")) return QStringLiteral("mi");
    return unit;
}

bool parseLength(const QString &query, double *value, UnitDef *unit)
{
    static const QRegularExpression pattern(QStringLiteral("^\\s*([+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+))\\s*([a-zA-Z]+)\\s*$"));
    const QRegularExpressionMatch match = pattern.match(query);
    if (!match.hasMatch()) return false;

    bool ok = false;
    const double amount = match.captured(1).toDouble(&ok);
    if (!ok || !std::isfinite(amount)) return false;

    const QString key = normalizedUnit(match.captured(2));
    for (const UnitDef &candidate : lengthUnits()) {
        if (candidate.key == key) {
            *value = amount;
            *unit = candidate;
            return true;
        }
    }
    return false;
}

QString formatNumber(double value)
{
    QString text = QString::number(value, 'g', 12);
    if (text == QStringLiteral("-0")) return QStringLiteral("0");
    return text;
}
}

class ConvertView final : public QWidget {
public:
    explicit ConvertView(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_title(new QLabel(this))
        , m_subtitle(new QLabel(this))
        , m_grid(new QGridLayout)
    {
        setObjectName(QStringLiteral("convertView"));
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);

        m_title->setObjectName(QStringLiteral("pageTitle"));
        m_subtitle->setObjectName(QStringLiteral("muted"));
        layout->addWidget(m_title);
        layout->addWidget(m_subtitle);

        m_grid->setContentsMargins(0, 4, 0, 0);
        m_grid->setHorizontalSpacing(12);
        m_grid->setVerticalSpacing(12);
        layout->addLayout(m_grid);
        layout->addStretch();
    }

    void setQuery(const QString &query)
    {
        while (QLayoutItem *item = m_grid->takeAt(0)) {
            delete item->widget();
            delete item;
        }

        double amount = 0.0;
        UnitDef from;
        if (!parseLength(query, &amount, &from)) {
            m_title->setText(QObject::tr("Convert"));
            m_subtitle->setText(QObject::tr("Try values like 1mile, 10km, 25cm"));
            addCard(QObject::tr("Unsupported input"), QObject::tr("Length units are supported for now"));
            return;
        }

        m_title->setText(QObject::tr("Convert %1").arg(query.trimmed()));
        m_subtitle->setText(QObject::tr("Length conversion"));
        const double meters = amount * from.factorToMeter;
        int row = 0;
        int column = 0;
        for (const UnitDef &target : lengthUnits()) {
            const QString value = formatNumber(meters / target.factorToMeter);
            addCard(value + QStringLiteral(" ") + target.key, target.label, row, column);
            if (++column == 2) {
                column = 0;
                ++row;
            }
        }
    }

private:
    void addCard(const QString &value, const QString &label, int row = 0, int column = 0)
    {
        auto *card = new QWidget(this);
        card->setObjectName(QStringLiteral("convertCard"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(4);
        auto *valueLabel = new QLabel(value, card);
        valueLabel->setObjectName(QStringLiteral("convertValue"));
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *nameLabel = new QLabel(label, card);
        nameLabel->setObjectName(QStringLiteral("muted"));
        layout->addWidget(valueLabel);
        layout->addWidget(nameLabel);
        m_grid->addWidget(card, row, column);
    }

    QLabel *m_title;
    QLabel *m_subtitle;
    QGridLayout *m_grid;
};

ConvertPlugin::ConvertPlugin(QObject *parent)
    : IPlugin(parent)
{
}

QVariantList ConvertPlugin::query(const QString &args)
{
    QVariantMap item;
    item["title"] = args.trimmed().isEmpty() ? tr("Enter a value to convert") : tr("Convert %1").arg(args.trimmed());
    item["subtitle"] = tr("Examples: 1mile, 10km, 25cm");
    item["icon"] = "";
    QVariantMap action;
    action["type"] = QStringLiteral("none");
    action["plugin"] = command();
    item["action"] = action;
    return QVariantList() << item;
}

bool ConvertPlugin::execute(const QVariantMap &action)
{
    Q_UNUSED(action)
    return true;
}

IPlugin::LaunchMode ConvertPlugin::launchMode(const QString &args) const
{
    return args.trimmed().isEmpty() ? LaunchMode::List : LaunchMode::QueryView;
}

void ConvertPlugin::activateQuery(const QString &args)
{
    m_query = args;
    if (m_view) m_view->setQuery(m_query);
}

QWidget *ConvertPlugin::viewUrl()
{
    if (!m_view) {
        m_view = new ConvertView;
        connect(m_view, &QObject::destroyed, this, [this]() { m_view = nullptr; });
    }
    m_view->setQuery(m_query);
    return m_view;
}
