#include "JsonTreeItem.h"

JsonTreeItem::JsonTreeItem(const QString &key, const QVariant &value,
                           JsonNodeType type, JsonTreeItem *parent)
    : m_parent(parent), m_key(key), m_value(value), m_type(type) {
}

JsonTreeItem::~JsonTreeItem() {
    qDeleteAll(m_children);
}

void JsonTreeItem::appendChild(JsonTreeItem *child) {
    if (child) child->m_row = m_children.size();
    m_children.append(child);
    if (child) {
        child->m_parent = this;
    }
}

void JsonTreeItem::insertChild(int row, JsonTreeItem *child) {
    if (!child) return;
    if (row < 0 || row > m_children.size()) row = m_children.size();
    m_children.insert(row, child);
    child->m_parent = this;
    for (int i = row; i < m_children.size(); ++i) m_children[i]->m_row = i;
}

void JsonTreeItem::removeChild(int row) {
    if (row < 0 || row >= m_children.size())
        return;
    delete takeChild(row);
}

JsonTreeItem *JsonTreeItem::child(int row) {
    if (row < 0 || row >= m_children.size())
        return nullptr;
    return m_children.at(row);
}

JsonTreeItem *JsonTreeItem::takeChild(int row) {
    if (row < 0 || row >= m_children.size())
        return nullptr;
    JsonTreeItem *child = m_children.takeAt(row);
    if (child) child->m_parent = nullptr;
    for (int i = row; i < m_children.size(); ++i) m_children[i]->m_row = i;
    return child;
}

int JsonTreeItem::childCount() const {
    return m_children.size();
}

int JsonTreeItem::row() const {
    return m_row;
}

JsonTreeItem *JsonTreeItem::parentItem() {
    return m_parent;
}

QString JsonTreeItem::key() const {
    return m_key;
}

void JsonTreeItem::setKey(const QString &key) {
    m_key = key;
}

QVariant JsonTreeItem::value() const {
    return m_value;
}

void JsonTreeItem::setValue(const QVariant &value) {
    m_value = value;
}

JsonNodeType JsonTreeItem::nodeType() const {
    return m_type;
}

void JsonTreeItem::setNodeType(JsonNodeType type) {
    m_type = type;
}

JsonTreeItem *JsonTreeItem::clone(JsonTreeItem *parent) const {
    auto *item = new JsonTreeItem(m_key, m_value, m_type, parent);
    for (auto *child : m_children) {
        item->appendChild(child->clone(item));
    }
    return item;
}

QList<JsonTreeItem *> JsonTreeItem::takeChildren() {
    QList<JsonTreeItem *> children;
    children.swap(m_children);
    for (auto *child : children) child->m_parent = nullptr;
    return children;
}
