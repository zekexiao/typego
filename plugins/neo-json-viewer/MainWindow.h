#pragma once

#include <QMainWindow>
#include <QTreeView>
#include <QLabel>
#include <QSettings>
#include <QVector>
#include <memory>
#include "JsonTreeModel.h"
#include "SettingsDialog.h"
#include "SearchDialog.h"

class QRegularExpression;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr, const QString &settingsPath = {});
    bool confirmLeave();
    ~MainWindow() override;

private slots:
    void onOpen();
    void onSave();
    void onSaveAs();
    void onSettings();
    void onSearch();
    void onFindNext();
    void onFindPrevious();
    void onSearchRequested(const SearchOptions &options);
    void onExpandAll();
    void onCollapseAll();
    void onCustomContextMenu(const QPoint &pos);
    void onDuplicateArrayItem();
    void onAddItem();
    void onDeleteItem();
    void onCopy();
    void onCopyKey();
    void onCopyValue();
    void onPaste();
    void onAbout();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();
    void setupMenus();
    void loadSettings();
    void saveSettings();
    void setDirty(bool dirty);
    void updateWindowTitle();
    void pasteKeyValueIntoObject(const QModelIndex &parentIdx, JsonTreeItem *parentItem, const QString &key, yyjson_val *root);
    bool tryParseKeyValue(const QString &text, QString &outKey, QString &outValue);
    void selectIndex(const QModelIndex &index);
    bool indexMatchesSearch(const QModelIndex &baseIndex, const SearchOptions &options, QModelIndex *matchedIndex, const QRegularExpression *regex) const;
    bool findAndSelect(const SearchOptions &options);
    // 后台加载：解析完成后在界面线程接管结果。
    void finishLoad(const QString &filePath, std::shared_ptr<JsonTreeItem> tree, qint64 parseMs, qint64 totalMs);
    void setBusy(bool busy);

    QTreeView *m_treeView;
    JsonTreeModel *m_model;
    SearchDialog *m_searchDialog;
    QLabel *m_statusLabel;
    QString m_currentFile;
    QSettings *m_settings;
    QModelIndex m_contextIndex;
    SearchOptions m_lastSearch;
    bool m_hasLastSearch;
    bool m_isDirty;
    bool m_loading = false;
    // 后台加载的批次号，只有最新一次的结果会被采用。
    quint64 m_loadGeneration = 0;
};
