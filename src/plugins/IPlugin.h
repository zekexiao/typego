#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QWidget;

class IPlugin : public QObject {
    Q_OBJECT
public:
    explicit IPlugin(QObject *parent = nullptr) : QObject(parent) {}
    ~IPlugin() override = default;

    virtual QString command() const = 0;
    virtual QString description() const = 0;
    virtual QVariantList query(const QString &args) = 0;
    virtual bool execute(const QVariantMap &action) = 0;

    // The widget is created lazily. Once inserted into a layout, Qt owns it.
    virtual QWidget *viewUrl() { return nullptr; }
};
