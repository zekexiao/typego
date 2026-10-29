#include "JsonTreeModel.h"
#include <QFile>
#include <QSaveFile>
#include <QStringDecoder>
#include <cmath>
#include <cstring>

namespace {
// 直接构造文档以保留 64 位整数，拒绝无法无损表示的二进制和扩展类型。
class MessagePackReader {
public:
    explicit MessagePackReader(const QByteArray &data) : m_data(data) {}
    yyjson_doc *decode() {
        yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val *root = readValue(doc, 0);
        yyjson_doc *result = nullptr;
        if (root && m_pos == m_data.size()) {
            yyjson_mut_doc_set_root(doc, root);
            result = yyjson_mut_doc_imut_copy(doc, nullptr);
        }
        yyjson_mut_doc_free(doc);
        return result;
    }
private:
    const QByteArray &m_data;
    qsizetype m_pos = 0;
    bool uintN(int count, quint64 &value) {
        if (count > m_data.size() - m_pos) return false;
        value = 0;
        while (count--) value = (value << 8) | quint8(m_data[m_pos++]);
        return true;
    }
    yyjson_mut_val *readValue(yyjson_mut_doc *doc, int depth) {
        quint64 tag;
        if (depth > 512 || !uintN(1, tag)) return nullptr;
        if (tag <= 0x7f) return yyjson_mut_uint(doc, tag);
        if (tag >= 0xe0) return yyjson_mut_sint(doc, qint8(tag));
        quint64 count = 0;
        if ((tag & 0xe0) == 0xa0 || tag == 0xd9 || tag == 0xda || tag == 0xdb) {
            if ((tag & 0xe0) == 0xa0) count = tag & 0x1f;
            else if (!uintN(1 << (tag - 0xd9), count)) return nullptr;
            if (count > quint64(m_data.size() - m_pos)) return nullptr;
            QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
            const QString validated = decoder(QByteArrayView(m_data.constData() + m_pos, qsizetype(count)));
            if (decoder.hasError()) return nullptr;
            auto *value = yyjson_mut_strncpy(doc, m_data.constData() + m_pos, size_t(count));
            m_pos += qsizetype(count);
            return value;
        }
        const bool array = (tag & 0xf0) == 0x90 || tag == 0xdc || tag == 0xdd;
        const bool object = (tag & 0xf0) == 0x80 || tag == 0xde || tag == 0xdf;
        if (array || object) {
            if (tag < 0xa0) count = tag & 0x0f;
            else if (!uintN((tag == 0xdc || tag == 0xde) ? 2 : 4, count)) return nullptr;
            if (count > quint64(m_data.size() - m_pos) / (object ? 2 : 1)) return nullptr;
            auto *value = array ? yyjson_mut_arr(doc) : yyjson_mut_obj(doc);
            while (count--) {
                auto *key = object ? readValue(doc, depth + 1) : nullptr;
                if (object && !yyjson_mut_is_str(key)) return nullptr;
                auto *child = readValue(doc, depth + 1);
                if (!child) return nullptr;
                if (object) yyjson_mut_obj_add(value, key, child);
                else yyjson_mut_arr_append(value, child);
            }
            return value;
        }
        if (tag == 0xc0) return yyjson_mut_null(doc);
        if (tag == 0xc2 || tag == 0xc3) return yyjson_mut_bool(doc, tag == 0xc3);
        if (tag >= 0xcc && tag <= 0xd3) {
            quint64 bits;
            const int width = 1 << ((tag - 0xcc) % 4);
            if (!uintN(width, bits)) return nullptr;
            if (tag <= 0xcf) return yyjson_mut_uint(doc, bits);
            // 先扩展符号位，再按位解释，避免有符号溢出。
            if (width < 8 && (bits & (quint64(1) << (width * 8 - 1))))
                bits |= (~quint64(0)) << (width * 8);
            qint64 number;
            std::memcpy(&number, &bits, sizeof(number));
            return yyjson_mut_sint(doc, number);
        }
        if (tag == 0xca || tag == 0xcb) {
            quint64 bits;
            if (!uintN(tag == 0xca ? 4 : 8, bits)) return nullptr;
            double number;
            if (tag == 0xca) {
                quint32 raw = quint32(bits);
                float value;
                std::memcpy(&value, &raw, sizeof(value));
                number = value;
            } else std::memcpy(&number, &bits, sizeof(number));
            return std::isfinite(number) ? yyjson_mut_real(doc, number) : nullptr;
        }
        return nullptr;
    }
};
}

JsonTreeModel::JsonTreeModel(QObject *parent)
    : QAbstractItemModel(parent), m_rootItem(new JsonTreeItem("root", QVariant(), JsonNodeType::Root)), m_maxTextLength(1024), m_prettyPrint(true) {
}

JsonTreeModel::~JsonTreeModel() {
    delete m_rootItem;
}

QVariant JsonTreeModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    JsonTreeItem *item = static_cast<JsonTreeItem *>(index.internalPointer());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (index.column() == 0) {
            return item->key();
        } else if (index.column() == 1) {
            if (role == Qt::EditRole) {
                return item->value();
            }
            if (item->nodeType() == JsonNodeType::Object) {
                return tr("{ %1 items }").arg(item->childCount());
            }
            if (item->nodeType() == JsonNodeType::Array) {
                return tr("[ %1 items ]").arg(item->childCount());
            }
            if (item->nodeType() == JsonNodeType::Null) {
                return tr("null");
            }
            QString str = item->value().toString();
            if (m_maxTextLength > 0 && str.length() > m_maxTextLength) {
                str = str.left(m_maxTextLength) + tr("... (%1 chars total)").arg(str.length());
            }
            return str;
        }
    }

    return QVariant();
}

bool JsonTreeModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    JsonTreeItem *item = static_cast<JsonTreeItem *>(index.internalPointer());

    if (!(flags(index) & Qt::ItemIsEditable)) return false;
    if (index.column() == 0) {
        const QString key = value.toString();
        auto *parent = item->parentItem();
        for (int i = 0; i < parent->childCount(); ++i)
            if (parent->child(i) != item && parent->child(i)->key() == key) return false;
        item->setKey(key);
    } else {
        QVariant converted = value;
        if (item->nodeType() == JsonNodeType::Number) {
            const QByteArray text = value.toString().toUtf8();
            yyjson_doc *doc = yyjson_read(text.constData(), size_t(text.size()), 0);
            if (!doc) return false;
            const auto *root = yyjson_doc_get_root(doc);
            if (!yyjson_is_num(root)) { yyjson_doc_free(doc); return false; }
            if (yyjson_is_sint(root)) converted = QVariant::fromValue(qlonglong(yyjson_get_sint(root)));
            else if (yyjson_is_uint(root)) converted = QVariant::fromValue(qulonglong(yyjson_get_uint(root)));
            else converted = yyjson_get_real(root);
            yyjson_doc_free(doc);
        } else if (item->nodeType() == JsonNodeType::Boolean) {
            const QString text = value.toString();
            if (text != QLatin1String("true") && text != QLatin1String("false")) return false;
            converted = text == QLatin1String("true");
        }
        item->setValue(converted);
        if (item->nodeType() == JsonNodeType::Null) item->setNodeType(JsonNodeType::String);
    }

    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

Qt::ItemFlags JsonTreeModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = QAbstractItemModel::flags(index);
    auto *item = static_cast<JsonTreeItem *>(index.internalPointer());
    if ((index.column() == 0 && item->parentItem()->nodeType() == JsonNodeType::Object)
        || (index.column() == 1 && item->nodeType() != JsonNodeType::Object
            && item->nodeType() != JsonNodeType::Array)) flags |= Qt::ItemIsEditable;
    return flags;
}

QVariant JsonTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0)
            return tr("Key");
        if (section == 1)
            return tr("Value");
    }
    return QVariant();
}

QModelIndex JsonTreeModel::index(int row, int column, const QModelIndex &parent) const {
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    JsonTreeItem *parentItem;
    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<JsonTreeItem *>(parent.internalPointer());

    if (!parentItem)
        return QModelIndex();

    JsonTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex JsonTreeModel::parent(const QModelIndex &index) const {
    if (!index.isValid())
        return QModelIndex();

    JsonTreeItem *childItem = static_cast<JsonTreeItem *>(index.internalPointer());
    JsonTreeItem *parentItem = childItem->parentItem();

    if (!parentItem || parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int JsonTreeModel::rowCount(const QModelIndex &parent) const {
    JsonTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<JsonTreeItem *>(parent.internalPointer());

    return parentItem->childCount();
}

int JsonTreeModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent)
    return 2;
}

bool JsonTreeModel::insertRows(int row, int count, const QModelIndex &parent) {
    JsonTreeItem *parentItem = parent.isValid() ? static_cast<JsonTreeItem*>(parent.internalPointer()) : m_rootItem;

    if (count <= 0 || parent.column() > 0 || row < 0 || row > parentItem->childCount()
        || (parentItem->nodeType() != JsonNodeType::Object && parentItem->nodeType() != JsonNodeType::Array))
        return false;

    beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        auto *item = new JsonTreeItem(tr("new"), QVariant(), JsonNodeType::Null);
        parentItem->insertChild(row + i, item);
    }
    endInsertRows();

    if (parentItem->nodeType() == JsonNodeType::Array) {
        updateArrayKeys(parentItem);
        if (parentItem->childCount())
            emit dataChanged(index(0, 0, parent), index(parentItem->childCount() - 1, 1, parent), {Qt::DisplayRole});
    }
    if (parent.isValid()) emit dataChanged(parent, parent.siblingAtColumn(1), {Qt::DisplayRole});

    return true;
}

bool JsonTreeModel::removeRows(int row, int count, const QModelIndex &parent) {
    JsonTreeItem *parentItem = parent.isValid() ? static_cast<JsonTreeItem*>(parent.internalPointer()) : m_rootItem;

    if (count <= 0 || parent.column() > 0 || row < 0 || row > parentItem->childCount()
        || count > parentItem->childCount() - row)
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        parentItem->removeChild(row);
    }
    endRemoveRows();

    if (parentItem->nodeType() == JsonNodeType::Array) {
        updateArrayKeys(parentItem);
        if (parentItem->childCount())
            emit dataChanged(index(0, 0, parent), index(parentItem->childCount() - 1, 1, parent), {Qt::DisplayRole});
    }
    if (parent.isValid()) emit dataChanged(parent, parent.siblingAtColumn(1), {Qt::DisplayRole});

    return true;
}

bool JsonTreeModel::loadJson(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError) return false;
    return loadData(data, true);
}

bool JsonTreeModel::loadFromString(const QString &jsonStr) {
    return loadData(jsonStr.toUtf8(), false);
}

bool JsonTreeModel::loadData(const QByteArray &data, bool allowMessagePack) {
    yyjson_doc *doc = yyjson_read(data.constData(), size_t(data.size()), 0);
    if (!doc && allowMessagePack) doc = MessagePackReader(data).decode();
    if (!doc) return false;
    auto *item = createItemFromJsonImpl(yyjson_doc_get_root(doc), QString());
    yyjson_doc_free(doc);
    if (!item) return false;
    beginResetModel();
    delete m_rootItem;
    m_rootItem = new JsonTreeItem(QStringLiteral("root"), {}, JsonNodeType::Root);
    m_rootItem->appendChild(item);
    endResetModel();
    return true;
}

JsonTreeItem *JsonTreeModel::createItemFromJsonImpl(yyjson_val *node, const QString &key, int depth) {
    if (depth > 512) return nullptr;
    yyjson_type type = yyjson_get_type(node);

    switch (type) {
    case YYJSON_TYPE_OBJ: {
        auto *item = new JsonTreeItem(key, QVariant(), JsonNodeType::Object);
        size_t idx, max;
        yyjson_val *keyVal, *val;
        yyjson_obj_foreach(node, idx, max, keyVal, val) {
            auto *child = createItemFromJsonImpl(val,
                QString::fromUtf8(yyjson_get_str(keyVal), qsizetype(yyjson_get_len(keyVal))), depth + 1);
            if (!child) { delete item; return nullptr; }
            item->appendChild(child);
        }
        return item;
    }
    case YYJSON_TYPE_ARR: {
        auto *item = new JsonTreeItem(key, QVariant(), JsonNodeType::Array);
        size_t idx, max;
        yyjson_val *val;
        yyjson_arr_foreach(node, idx, max, val) {
            auto *child = createItemFromJsonImpl(val, QStringLiteral("[%1]").arg(idx), depth + 1);
            if (!child) { delete item; return nullptr; }
            item->appendChild(child);
        }
        return item;
    }
    case YYJSON_TYPE_STR: {
        return new JsonTreeItem(key, QString::fromUtf8(yyjson_get_str(node), qsizetype(yyjson_get_len(node))), JsonNodeType::String);
    }
    case YYJSON_TYPE_NUM: {
        QVariant numValue;
        if (yyjson_is_sint(node)) {
            numValue = QVariant(static_cast<qlonglong>(yyjson_get_sint(node)));
        } else if (yyjson_is_uint(node)) {
            numValue = QVariant::fromValue(qulonglong(yyjson_get_uint(node)));
        } else if (yyjson_is_real(node)) {
            numValue = QVariant(yyjson_get_real(node));
        } else {
            numValue = QVariant(yyjson_get_num(node));
        }
        return new JsonTreeItem(key, numValue, JsonNodeType::Number);
    }
    case YYJSON_TYPE_BOOL: {
        return new JsonTreeItem(key, QVariant(yyjson_get_bool(node)), JsonNodeType::Boolean);
    }
    case YYJSON_TYPE_NULL: {
        return new JsonTreeItem(key, QVariant(), JsonNodeType::Null);
    }
    default:
        return new JsonTreeItem(key, QVariant(), JsonNodeType::Null);
    }
}

JsonTreeItem *JsonTreeModel::createItemFromJson(yyjson_val *node, const QString &key) {
    return createItemFromJsonImpl(node, key);
}

bool JsonTreeModel::saveJson(const QString &filePath) const {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    if (!doc) return false;
    auto *root = m_rootItem->childCount() ? createMutVal(m_rootItem->child(0), doc) : yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    size_t length = 0;
    char *json = yyjson_mut_write(doc, m_prettyPrint ? YYJSON_WRITE_PRETTY : 0, &length);
    yyjson_mut_doc_free(doc);
    if (!json) return false;
    // 直接写入 UTF-8 缓冲，避免大文档经 QString 往返转换。
    QSaveFile file(filePath);
    const bool saved = file.open(QIODevice::WriteOnly)
        && file.write(json, qint64(length)) == qint64(length) && file.commit();
    free(json);
    return saved;
}

QString JsonTreeModel::toJsonString() const {
    return m_rootItem->childCount() ? itemToJsonString(index(0, 0)) : QStringLiteral("{}");
}

QString JsonTreeModel::itemToJsonString(const QModelIndex &index) const {
    if (!index.isValid())
        return QString();

    JsonTreeItem *item = static_cast<JsonTreeItem *>(index.internalPointer());
    if (!item)
        return QString();

    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val *root = createMutVal(item, doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_write_flag flags = m_prettyPrint ? YYJSON_WRITE_PRETTY : 0;
    const char *json = yyjson_mut_write(doc, flags, nullptr);
    QString result;
    if (json) {
        result = QString::fromUtf8(json);
        free((void *)json);
    }
    yyjson_mut_doc_free(doc);
    return result;
}

yyjson_mut_val *JsonTreeModel::createMutVal(JsonTreeItem *item, yyjson_mut_doc *doc) const {
    yyjson_mut_val *val = nullptr;

    switch (item->nodeType()) {
    case JsonNodeType::Object: {
        val = yyjson_mut_obj(doc);
        for (int i = 0; i < item->childCount(); ++i) {
            serializeItem(item->child(i), doc, val);
        }
        break;
    }
    case JsonNodeType::Array: {
        val = yyjson_mut_arr(doc);
        for (int i = 0; i < item->childCount(); ++i) {
            serializeItem(item->child(i), doc, val);
        }
        break;
    }
    case JsonNodeType::String: {
        QByteArray strData = item->value().toString().toUtf8();
        val = yyjson_mut_strncpy(doc, strData.constData(), size_t(strData.size()));
        break;
    }
    case JsonNodeType::Number: {
        QVariant v = item->value();
        if (v.typeId() == QMetaType::LongLong || v.typeId() == QMetaType::Int) {
            val = yyjson_mut_sint(doc, v.toLongLong());
        } else if (v.typeId() == QMetaType::ULongLong || v.typeId() == QMetaType::UInt) {
            val = yyjson_mut_uint(doc, v.toULongLong());
        } else {
            val = yyjson_mut_real(doc, v.toDouble());
        }
        break;
    }
    case JsonNodeType::Boolean: {
        val = yyjson_mut_bool(doc, item->value().toBool());
        break;
    }
    case JsonNodeType::Null: {
        val = yyjson_mut_null(doc);
        break;
    }
    default:
        val = yyjson_mut_null(doc);
        break;
    }

    return val;
}

void JsonTreeModel::serializeItem(JsonTreeItem *item, yyjson_mut_doc *doc, yyjson_mut_val *parent) const {
    yyjson_mut_val *val = createMutVal(item, doc);

    if (yyjson_mut_is_obj(parent)) {
        QByteArray keyData = item->key().toUtf8();
        yyjson_mut_val *key = yyjson_mut_strncpy(doc, keyData.constData(), size_t(keyData.size()));
        yyjson_mut_obj_add(parent, key, val);
    } else if (yyjson_mut_is_arr(parent)) {
        yyjson_mut_arr_append(parent, val);
    }
}

void JsonTreeModel::setMaxTextLength(int maxLength) {
    m_maxTextLength = maxLength;
    // 仅刷新显示，不触发编辑脏标记，并覆盖嵌套节点。
    QList<QModelIndex> pending{QModelIndex()};
    while (!pending.isEmpty()) {
        const QModelIndex parent = pending.takeLast();
        const int count = rowCount(parent);
        if (!count) continue;
        emit dataChanged(index(0, 1, parent), index(count - 1, 1, parent), {Qt::DisplayRole});
        for (int row = 0; row < count; ++row) {
            const auto child = index(row, 0, parent);
            if (rowCount(child)) pending.append(child);
        }
    }
}

int JsonTreeModel::maxTextLength() const {
    return m_maxTextLength;
}

void JsonTreeModel::setPrettyPrint(bool pretty) {
    m_prettyPrint = pretty;
}

bool JsonTreeModel::prettyPrint() const {
    return m_prettyPrint;
}

bool JsonTreeModel::addItem(const QModelIndex &parent, JsonTreeItem *item) {
    JsonTreeItem *parentItem = parent.isValid() ? static_cast<JsonTreeItem*>(parent.internalPointer()) : m_rootItem;

    if (!item || parent.column() > 0 || (parentItem->nodeType() != JsonNodeType::Array
        && parentItem->nodeType() != JsonNodeType::Object)) return false;
    if (parentItem->nodeType() == JsonNodeType::Object) {
        for (int i = 0; i < parentItem->childCount(); ++i)
            if (parentItem->child(i)->key() == item->key()) return false;
    }
    int row = parentItem->childCount();
    beginInsertRows(parent, row, row);
    parentItem->insertChild(row, item);
    endInsertRows();

    if (parentItem->nodeType() == JsonNodeType::Array) {
        item->setKey(QStringLiteral("[%1]").arg(row));
    }
    if (parent.isValid()) emit dataChanged(parent, parent.siblingAtColumn(1), {Qt::DisplayRole});

    return true;
}

void JsonTreeModel::updateArrayKeys(JsonTreeItem *arrayItem) {
    for (int i = 0; i < arrayItem->childCount(); ++i) {
        arrayItem->child(i)->setKey(QString("[%1]").arg(i));
    }
}

QModelIndex JsonTreeModel::indexForItem(JsonTreeItem *item) const {
    if (!item || item == m_rootItem)
        return QModelIndex();
    return createIndex(item->row(), 0, item);
}

bool JsonTreeModel::replaceItem(const QModelIndex &index, JsonTreeItem *newItem) {
    if (!index.isValid() || !newItem)
        return false;

    JsonTreeItem *oldItem = static_cast<JsonTreeItem *>(index.internalPointer());
    JsonTreeItem *parentItem = oldItem->parentItem();
    if (!parentItem)
        return false;

    const QModelIndex nodeIndex = index.siblingAtColumn(0);
    QModelIndex parentIndex = nodeIndex.parent();

    // 1. 删除 oldItem 的所有子节点，保留 oldItem 指针本身
    if (oldItem->childCount() > 0) {
        beginRemoveRows(nodeIndex, 0, oldItem->childCount() - 1);
        while (oldItem->childCount() > 0) {
            oldItem->removeChild(oldItem->childCount() - 1);
        }
        endRemoveRows();
    }

    // 2. 替换 oldItem 的数据
    oldItem->setKey(newItem->key());
    oldItem->setValue(newItem->value());
    oldItem->setNodeType(newItem->nodeType());

    // 3. 将 newItem 的子节点转移到 oldItem
    if (newItem->childCount() > 0) {
        beginInsertRows(nodeIndex, 0, newItem->childCount() - 1);
        const auto children = newItem->takeChildren();
        for (auto *child : children) oldItem->appendChild(child);
        endInsertRows();
    }

    // 4. 通知该节点数据变化
    emit dataChanged(nodeIndex, nodeIndex.siblingAtColumn(1), {Qt::DisplayRole, Qt::EditRole});

    if (parentItem->nodeType() == JsonNodeType::Array) {
        updateArrayKeys(parentItem);
        emit dataChanged(this->index(0, 0, parentIndex), this->index(parentItem->childCount() - 1, 1, parentIndex));
    }

    delete newItem;
    return true;
}
