#pragma once

#include <QAbstractItemModel>
#include "JsonTreeItem.h"

extern "C" {
#include <yyjson.h>
}

class JsonTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit JsonTreeModel(QObject *parent = nullptr);
    ~JsonTreeModel() override;

    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    bool loadJson(const QString &filePath);
    bool saveJson(const QString &filePath) const;
    bool loadFromString(const QString &jsonStr);
    QString toJsonString() const;
    QString itemToJsonString(const QModelIndex &index) const;

    void setMaxTextLength(int maxLength);
    int maxTextLength() const;

    void setPrettyPrint(bool pretty);
    bool prettyPrint() const;

    bool addItem(const QModelIndex &parent, JsonTreeItem *item);
    QModelIndex indexForItem(JsonTreeItem *item) const;
    bool replaceItem(const QModelIndex &index, JsonTreeItem *newItem);
    JsonTreeItem *createItemFromJson(yyjson_val *node, const QString &key = QString());

private:
    JsonTreeItem *m_rootItem;
    int m_maxTextLength;
    bool m_prettyPrint;

    JsonTreeItem *createItemFromJsonImpl(yyjson_val *node, const QString &key, int depth = 0);
    yyjson_mut_val *createMutVal(JsonTreeItem *item, yyjson_mut_doc *doc) const;
    void serializeItem(JsonTreeItem *item, yyjson_mut_doc *doc, yyjson_mut_val *parent) const;
    bool loadData(const QByteArray &data, bool allowMessagePack);
    void updateArrayKeys(JsonTreeItem *arrayItem);
};
