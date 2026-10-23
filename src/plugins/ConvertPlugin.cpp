#include "ConvertPlugin.h"

#include "UnitConversion.h"

#include <QGridLayout>
#include <QLabel>
#include <QVariantMap>
#include <QVBoxLayout>
#include <QWidget>

// 换算视图：离开插件后由 releaseInactiveResources 销毁，重进时重建。
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
        UnitConversion::LengthUnit from;
        if (!UnitConversion::parseLengthQuery(query, &amount, &from)) {
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
        for (const UnitConversion::LengthUnit &target : UnitConversion::lengthUnits()) {
            const QString value = UnitConversion::formatNumber(meters / target.factorToMeter);
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

ConvertPlugin::~ConvertPlugin() = default;

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

PluginExecutionResult ConvertPlugin::execute(const PluginInvocation &invocation)
{
    // 与旧适配层语义一致：占位项执行成功即隐藏窗口。
    Q_UNUSED(invocation)
    return {PluginExecutionResult::Status::Succeeded, PluginExecutionResult::HostAction::Hide};
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

QWidget *ConvertPlugin::createView(QWidget *parent)
{
    if (!m_view) {
        m_view = new ConvertView(parent);
        connect(m_view, &QObject::destroyed, this, [this]() { m_view = nullptr; });
    }
    m_view->setQuery(m_query);
    return m_view;
}

void ConvertPlugin::releaseInactiveResources()
{
    // 视图只在激活期间存在；销毁指针由 destroyed 信号复位。
    if (m_view) m_view->deleteLater();
}
