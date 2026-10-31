#pragma once

#include <QAbstractItemModel>
#include <memory>
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

    // 解析数据并建树；不依赖模型状态，可以放到工作线程执行。
    // 返回的树由调用方交给 setRootTree 接管，失败返回 nullptr。
    static JsonTreeItem *buildTree(const QByteArray &data, bool allowMessagePack);
    // 用外部建好的树替换当前文档，只能在界面线程调用。
    // 树的所有权转给模型，外部的 shared_ptr 随之失效。
    void setRootTree(std::shared_ptr<JsonTreeItem> tree);

    void setMaxTextLength(int maxLength);
    int maxTextLength() const;

    void setPrettyPrint(bool pretty);
    bool prettyPrint() const;

    bool addItem(const QModelIndex &parent, JsonTreeItem *item);
    QModelIndex indexForItem(JsonTreeItem *item) const;
    bool replaceItem(const QModelIndex &index, JsonTreeItem *newItem);
    // 顶层节点代表整份文档，删掉等于把内容整体换成 null，因此不允许。
    bool canRemove(const QModelIndex &index) const;
    JsonTreeItem *createItemFromJson(yyjson_val *node, const QString &key = QString());

private:
    JsonTreeItem *m_rootItem;
    // 当前文档树的所有权；树本身不随 m_rootItem 释放，由这里负责。
    std::shared_ptr<JsonTreeItem> m_documentTree;
    int m_maxTextLength;
    bool m_prettyPrint;

    // 建树过程不依赖模型状态，静态实现便于在工作线程复用。
    static JsonTreeItem *createItemFromJsonImpl(yyjson_val *node, const QString &key, int depth = 0);
    yyjson_mut_val *createMutVal(JsonTreeItem *item, yyjson_mut_doc *doc) const;
    void serializeItem(JsonTreeItem *item, yyjson_mut_doc *doc, yyjson_mut_val *parent) const;
    bool loadData(const QByteArray &data, bool allowMessagePack);
    void updateArrayKeys(JsonTreeItem *arrayItem);
};
