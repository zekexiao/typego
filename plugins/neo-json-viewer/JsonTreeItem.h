#pragma once

#include <QList>
#include <QVariant>
#include <QString>

enum class JsonNodeType {
    Root,
    Object,
    Array,
    String,
    Number,
    Boolean,
    Null
};

class JsonTreeItem {
public:
    explicit JsonTreeItem(const QString &key = QString(), const QVariant &value = QVariant(),
                          JsonNodeType type = JsonNodeType::Null, JsonTreeItem *parent = nullptr);
    ~JsonTreeItem();

    void appendChild(JsonTreeItem *child);
    void insertChild(int row, JsonTreeItem *child);
    void removeChild(int row);
    JsonTreeItem *takeChild(int row);
    QList<JsonTreeItem *> takeChildren();
    JsonTreeItem *child(int row);
    int childCount() const;
    int row() const;
    JsonTreeItem *parentItem();

    QString key() const;
    void setKey(const QString &key);

    QVariant value() const;
    void setValue(const QVariant &value);

    JsonNodeType nodeType() const;
    void setNodeType(JsonNodeType type);

    JsonTreeItem *clone(JsonTreeItem *parent = nullptr) const;

private:
    QList<JsonTreeItem *> m_children;
    JsonTreeItem *m_parent;
    int m_row = 0;
    QString m_key;
    QVariant m_value;
    JsonNodeType m_type;
};
