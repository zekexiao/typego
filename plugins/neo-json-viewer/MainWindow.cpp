#include "MainWindow.h"
#include "AddItemDialog.h"
#include "AboutDialog.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QCloseEvent>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QRegularExpression>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <functional>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
constexpr qint64 kAutoExpandThresholdBytes = 8LL * 1024 * 1024;   // 8 MiB
constexpr int kLargeFileKeyColumnWidth = 260;
// 打开一个 JSON 的内存放大倍数：文件缓冲 + yyjson 解析树 + 物化后的节点树。
constexpr qint64 kMemoryAmplification = 5;
// 单次搜索的最长耗时：树可能很大，正则还可能触发回溯，不能让界面一直卡着。
constexpr int kSearchTimeBudgetMs = 2000;
// 正则匹配的主体长度上限，避免在超长文本上做无意义的匹配。
constexpr int kRegexSubjectLimit = 64 * 1024;

// 可用物理内存；取不到时返回 0，表示不做判断。
qint64 availablePhysicalMemory()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) return 0;
    return qint64(status.ullAvailPhys);
#else
    return 0;
#endif
}

// 后台线程读文件并建树；不碰界面线程的对象。
std::shared_ptr<JsonTreeItem> loadTreeInBackground(const QString &filePath, qint64 *parseMs)
{
    QElapsedTimer timer;
    timer.start();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray data = file.readAll();
    if (file.error() != QFile::NoError) return {};
    std::shared_ptr<JsonTreeItem> tree(JsonTreeModel::buildTree(data, true));
    if (parseMs) *parseMs = timer.elapsed();
    return tree;
}

// 后台加载的结果；用 shared_ptr 兜底，窗口提前销毁时树也会被释放。
struct LoadOutcome {
    std::shared_ptr<JsonTreeItem> tree;
    qint64 parseMs = 0;
};
}

MainWindow::MainWindow(QWidget *parent, const QString &settingsPath)
    : QMainWindow(parent),
      m_searchDialog(nullptr),
      m_settings(new QSettings(settingsPath, QSettings::IniFormat, this)),
      m_hasLastSearch(false),
      m_isDirty(false) {
    setupUI();
    setupMenus();
    loadSettings();

    m_searchDialog = new SearchDialog(this);
    connect(m_searchDialog, &SearchDialog::findRequested, this, &MainWindow::onSearchRequested);

    connect(m_model, &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex &, const QModelIndex &, const QList<int> &roles) {
            if (roles.isEmpty() || roles.contains(Qt::EditRole)) setDirty(true);
        });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this]{ setDirty(true); });
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this]{ setDirty(true); });
}

MainWindow::~MainWindow() {
    // 加载途中窗口被销毁时，光标状态要还原。
    if (m_loading) QApplication::restoreOverrideCursor();
}

void MainWindow::setupUI() {
    updateWindowTitle();
    resize(900, 700);

    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *layout = new QVBoxLayout(centralWidget);


    m_treeView = new QTreeView(this);
    m_treeView->setObjectName(QStringLiteral("jsonTreeView"));
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->header()->setStretchLastSection(true);
    m_treeView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onCustomContextMenu);
    layout->addWidget(m_treeView);

    m_model = new JsonTreeModel(this);
    m_treeView->setModel(m_model);
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setupMenus() {
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    auto *openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);
    fileMenu->addAction(openAct);

    auto *saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::onSave);
    fileMenu->addAction(saveAct);

    auto *saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveAs);
    fileMenu->addAction(saveAsAct);

    fileMenu->addSeparator();

    auto *exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAct);

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    auto *findAct = new QAction(tr("&Find..."), this);
    findAct->setShortcut(QKeySequence::Find);
    connect(findAct, &QAction::triggered, this, &MainWindow::onSearch);
    editMenu->addAction(findAct);

    auto *findNextAct = new QAction(tr("Find &Next"), this);
    findNextAct->setShortcut(QKeySequence::FindNext);
    connect(findNextAct, &QAction::triggered, this, &MainWindow::onFindNext);
    editMenu->addAction(findNextAct);

    auto *findPreviousAct = new QAction(tr("Find &Previous"), this);
    findPreviousAct->setShortcut(QKeySequence::FindPrevious);
    connect(findPreviousAct, &QAction::triggered, this, &MainWindow::onFindPrevious);
    editMenu->addAction(findPreviousAct);

    editMenu->addSeparator();

    auto *copyAct = new QAction(tr("&Copy"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    connect(copyAct, &QAction::triggered, this, &MainWindow::onCopy);
    editMenu->addAction(copyAct);

    auto *copyKeyAct = new QAction(tr("Copy &Key"), this);
    connect(copyKeyAct, &QAction::triggered, this, &MainWindow::onCopyKey);
    editMenu->addAction(copyKeyAct);

    auto *copyValueAct = new QAction(tr("Copy &Value"), this);
    connect(copyValueAct, &QAction::triggered, this, &MainWindow::onCopyValue);
    editMenu->addAction(copyValueAct);

    auto *pasteAct = new QAction(tr("&Paste"), this);
    pasteAct->setShortcut(QKeySequence::Paste);
    connect(pasteAct, &QAction::triggered, this, &MainWindow::onPaste);
    editMenu->addAction(pasteAct);

    editMenu->addSeparator();

    auto *settingsAct = new QAction(tr("&Settings..."), this);
    connect(settingsAct, &QAction::triggered, this, &MainWindow::onSettings);
    editMenu->addAction(settingsAct);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    auto *expandAct = new QAction(tr("&Expand All"), this);
    connect(expandAct, &QAction::triggered, this, &MainWindow::onExpandAll);
    viewMenu->addAction(expandAct);

    auto *collapseAct = new QAction(tr("&Collapse All"), this);
    connect(collapseAct, &QAction::triggered, this, &MainWindow::onCollapseAll);
    viewMenu->addAction(collapseAct);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    auto *aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
    helpMenu->addAction(aboutAct);
}

void MainWindow::loadSettings() {
    int maxLen = m_settings->value("maxTextLength", 1024).toInt();
    bool pretty = m_settings->value("prettyPrint", true).toBool();
    m_model->setMaxTextLength(maxLen);
    m_model->setPrettyPrint(pretty);
}

void MainWindow::saveSettings() {
    m_settings->setValue("maxTextLength", m_model->maxTextLength());
    m_settings->setValue("prettyPrint", m_model->prettyPrint());
}

void MainWindow::onOpen() {
    if (!confirmLeave()) return;
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Data File"), QString(),
        tr("Data Files (*.json *.msgpack *.mpk);;JSON Files (*.json);;MessagePack Files (*.msgpack *.mpk);;All Files (*)"));
    if (filePath.isEmpty())
        return;

    const QFileInfo info(filePath);
    // 不设硬上限：yyjson 本来就是为兼容大 JSON 准备的，大文件照常打开。
    // 只有估算要用的内存超过可用内存时才提醒一句。
    const qint64 available = availablePhysicalMemory();
    if (available > 0) {
        const qint64 needed = info.size() * kMemoryAmplification;
        if (needed > available) {
            QMessageBox::warning(this, tr("Warning"),
                tr("This file may not fit in memory: about %1 MB is needed, %2 MB is available.")
                    .arg(needed / 1024 / 1024)
                    .arg(available / 1024 / 1024));
        }
    }

    // 读取和解析都放到后台线程，大文件不会把界面卡住。
    setBusy(true);
    const quint64 generation = ++m_loadGeneration;
    QElapsedTimer timer;
    timer.start();
    auto *watcher = new QFutureWatcher<LoadOutcome>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, filePath, timer, generation]() {
        const LoadOutcome outcome = watcher->result();
        watcher->deleteLater();
        // 期间又打开了别的文件，这次结果作废。
        if (generation != m_loadGeneration) return;
        finishLoad(filePath, outcome.tree, outcome.parseMs, timer.elapsed());
    });
    watcher->setFuture(QtConcurrent::run([filePath]() {
        LoadOutcome outcome;
        // 异常不能带到界面线程，失败按解析不成功处理。
        try {
            outcome.tree = loadTreeInBackground(filePath, &outcome.parseMs);
        } catch (...) {
            outcome.tree.reset();
        }
        return outcome;
    }));
}

void MainWindow::finishLoad(const QString &filePath, std::shared_ptr<JsonTreeItem> tree, qint64 parseMs, qint64 totalMs) {
    setBusy(false);
    if (!tree) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load file. Check that it contains valid JSON or MessagePack data."));
        return;
    }
    // 所有权交给模型，之后由模型负责释放。
    m_model->setRootTree(std::move(tree));
    m_contextIndex = {};
    m_currentFile = filePath;
    setDirty(false);

    const QFileInfo info(filePath);
    // 大文件禁用自动展开和全列测量，避免布局阻塞。
    const bool enableAutoExpand = info.size() <= kAutoExpandThresholdBytes;
    if (enableAutoExpand) {
        m_treeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_treeView->expandToDepth(1);
    } else {
        m_treeView->collapseAll();
        m_treeView->header()->setSectionResizeMode(0, QHeaderView::Interactive);
        m_treeView->header()->resizeSection(0, kLargeFileKeyColumnWidth);
    }

    if (enableAutoExpand) {
        m_statusLabel->setText(tr("Loaded: %1 (%2 ms)").arg(filePath).arg(totalMs));
    } else {
        auto mb = info.size() / 1024/ 1024.0;
        m_statusLabel->setText(tr("Loaded: %1 (filesize: %2mb, parse %3 ms, total %4 ms)")
                                   .arg(filePath)
                                   .arg(mb, 0, 'f', 2)
                                   .arg(parseMs)
                                   .arg(totalMs));
    }
}

void MainWindow::setBusy(bool busy) {
    if (busy) {
        if (m_loading) return;
        m_loading = true;
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_statusLabel->setText(tr("Loading..."));
    } else {
        if (!m_loading) return;
        m_loading = false;
        QApplication::restoreOverrideCursor();
    }
}

void MainWindow::onSave() {
    if (m_currentFile.isEmpty() || !m_currentFile.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        onSaveAs();
        return;
    }

    if (m_model->saveJson(m_currentFile)) {
        setDirty(false);
        m_statusLabel->setText(tr("Saved: %1").arg(m_currentFile));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save JSON file."));
    }
}

void MainWindow::onSaveAs() {
    QString filePath = QFileDialog::getSaveFileName(this, tr("Save JSON File"), QString(), tr("JSON Files (*.json);;All Files (*)"));
    if (filePath.isEmpty())
        return;

    if (m_model->saveJson(filePath)) {
        m_currentFile = filePath;
        setDirty(false);
        m_statusLabel->setText(tr("Saved: %1").arg(filePath));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save JSON file."));
    }
}

void MainWindow::onSettings() {
    SettingsDialog dialog(m_model->maxTextLength(), m_model->prettyPrint(), this);
    if (dialog.exec() == QDialog::Accepted) {
        m_model->setMaxTextLength(dialog.maxTextLength());
        m_model->setPrettyPrint(dialog.prettyPrint());
        m_settings->setValue("maxTextLength", dialog.maxTextLength());
        m_settings->setValue("prettyPrint", dialog.prettyPrint());
    }
}

void MainWindow::onSearch() {
    if (m_searchDialog == nullptr)
        return;

    if (m_hasLastSearch) {
        m_searchDialog->setOptions(m_lastSearch);
    }

    m_searchDialog->show();
    m_searchDialog->raise();
    m_searchDialog->activateWindow();
    m_searchDialog->focusPattern();
}

void MainWindow::onFindNext() {
    if (!m_hasLastSearch || m_lastSearch.text.isEmpty()) {
        onSearch();
        return;
    }

    SearchOptions options = m_lastSearch;
    options.backward = false;
    if (m_searchDialog != nullptr) {
        m_searchDialog->setDirection(false);
    }
    findAndSelect(options);
}

void MainWindow::onFindPrevious() {
    if (!m_hasLastSearch || m_lastSearch.text.isEmpty()) {
        onSearch();
        return;
    }

    SearchOptions options = m_lastSearch;
    options.backward = true;
    if (m_searchDialog != nullptr) {
        m_searchDialog->setDirection(true);
    }
    findAndSelect(options);
}

void MainWindow::onSearchRequested(const SearchOptions &options) {
    if (options.text.isEmpty()) {
        m_statusLabel->setText(tr("Search text is empty."));
        return;
    }

    m_lastSearch = options;
    m_hasLastSearch = true;
    findAndSelect(options);
}

bool MainWindow::indexMatchesSearch(const QModelIndex &baseIndex, const SearchOptions &options, QModelIndex *matchedIndex, const QRegularExpression *regex) const {
    const QModelIndex keyIndex = baseIndex.siblingAtColumn(0);
    const QModelIndex valueIndex = baseIndex.siblingAtColumn(1);
    QString keyText = m_model->data(keyIndex, Qt::DisplayRole).toString();
    QString valueText = m_model->data(valueIndex, Qt::EditRole).toString();

    if (regex) {
        // 限制匹配主体长度：PCRE2 单次匹配有上限，但长文本上反复匹配仍会拖垮界面。
        const QString keySubject = keyText.left(kRegexSubjectLimit);
        if (regex->match(keySubject).hasMatch()) {
            if (matchedIndex != nullptr)
                *matchedIndex = keyIndex;
            return true;
        }
        if (options.searchInValue) {
            const QString valueSubject = valueText.left(kRegexSubjectLimit);
            if (regex->match(valueSubject).hasMatch()) {
                if (matchedIndex != nullptr)
                    *matchedIndex = valueIndex;
                return true;
            }
        }
        return false;
    }

    const Qt::CaseSensitivity cs = options.ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive;
    if (keyText.contains(options.text, cs)) {
        if (matchedIndex != nullptr)
            *matchedIndex = keyIndex;
        return true;
    }
    if (options.searchInValue && valueText.contains(options.text, cs)) {
        if (matchedIndex != nullptr)
            *matchedIndex = valueIndex;
        return true;
    }
    return false;
}

bool MainWindow::findAndSelect(const SearchOptions &options) {
    QRegularExpression regex;
    if (options.useRegex) {
        regex.setPattern(options.text);
        if (options.ignoreCase) regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        if (!regex.isValid()) {
            QMessageBox::warning(this, tr("Search"), tr("Invalid regular expression: %1").arg(regex.errorString()));
            return false;
        }
    }
    // 深度优先逐个查找，避免为整棵树分配临时索引。
    auto next = [this, &options](QModelIndex node) {
        if (!node.isValid()) {
            node = m_model->index(0, 0);
            if (options.backward) {
                while (m_model->rowCount(node))
                    node = m_model->index(m_model->rowCount(node) - 1, 0, node);
            }
            return node;
        }
        if (!options.backward && m_model->rowCount(node)) return m_model->index(0, 0, node);
        if (options.backward) {
            if (node.row() == 0) return node.parent();
            node = m_model->index(node.row() - 1, 0, node.parent());
            while (m_model->rowCount(node)) node = m_model->index(m_model->rowCount(node) - 1, 0, node);
            return node;
        }
        while (node.isValid()) {
            auto sibling = m_model->index(node.row() + 1, 0, node.parent());
            if (sibling.isValid()) return sibling;
            node = node.parent();
        }
        return QModelIndex();
    };
    QModelIndex current = m_treeView->currentIndex().siblingAtColumn(0);
    QModelIndex node = next(current);
    QModelIndex first;
    bool wrapped = false;
    // 搜索要遍历整棵树，大文件上耗时可能很长；到预算就停下来，别把界面锁死。
    QElapsedTimer budget;
    budget.start();
    while (true) {
        if (budget.elapsed() > kSearchTimeBudgetMs) {
            m_statusLabel->setText(tr("Search stopped: it took too long. Try a more specific keyword."));
            return false;
        }
        if (!node.isValid()) {
            if (!options.wrapAround || wrapped) break;
            wrapped = true;
            node = next({});
            if (!node.isValid()) break;
        }
        if (node == first) break;
        if (!first.isValid()) first = node;
        QModelIndex matched;
        if (indexMatchesSearch(node, options, &matched, options.useRegex ? &regex : nullptr)) {
            for (auto parent = matched.parent(); parent.isValid(); parent = parent.parent()) m_treeView->expand(parent);
            selectIndex(matched);
            m_treeView->scrollTo(matched);
            m_statusLabel->setText(tr("Match found."));
            return true;
        }
        if (node == current) break;
        node = next(node);
    }
    m_statusLabel->setText(tr("No matches found."));
    return false;
}

void MainWindow::onExpandAll() {
    m_treeView->expandAll();
}

void MainWindow::onCollapseAll() {
    m_treeView->collapseAll();
}

void MainWindow::onCustomContextMenu(const QPoint &pos) {
    QModelIndex index = m_treeView->indexAt(pos);
    if (!index.isValid())
        return;

    selectIndex(index);
    m_contextIndex = index.siblingAtColumn(0);
    auto *item = static_cast<JsonTreeItem*>(index.internalPointer());
    JsonTreeItem *parentItem = item->parentItem();

    QMenu menu(this);

    if (parentItem && parentItem->nodeType() == JsonNodeType::Array) {
        menu.addAction(tr("Duplicate"), this, &MainWindow::onDuplicateArrayItem);
    }

    if (item->nodeType() == JsonNodeType::Object || item->nodeType() == JsonNodeType::Array) {
        menu.addAction(tr("Add Item..."), this, &MainWindow::onAddItem);
    }

    menu.addAction(tr("Copy"), this, &MainWindow::onCopy);
    menu.addAction(tr("Copy Key"), this, &MainWindow::onCopyKey);
    menu.addAction(tr("Copy Value"), this, &MainWindow::onCopyValue);
    menu.addAction(tr("Paste"), this, &MainWindow::onPaste);

    // 顶层节点就是整个文档，删掉等于把内容换成 null，只能逐个删子项。
    const bool isRootNode = !parentItem || parentItem->nodeType() == JsonNodeType::Root;
    if (!isRootNode) menu.addAction(tr("Delete"), this, &MainWindow::onDeleteItem);

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

void MainWindow::onDuplicateArrayItem() {
    if (!m_contextIndex.isValid())
        return;

    JsonTreeItem *item = static_cast<JsonTreeItem*>(m_contextIndex.internalPointer());
    JsonTreeItem *cloned = item->clone();

    QModelIndex parentIndex = m_contextIndex.parent();
    m_model->addItem(parentIndex, cloned);
}

void MainWindow::onAddItem() {
    if (!m_contextIndex.isValid())
        return;

    JsonTreeItem *parentItem = static_cast<JsonTreeItem*>(m_contextIndex.internalPointer());
    bool isArray = parentItem->nodeType() == JsonNodeType::Array;

    AddItemDialog dialog(isArray, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString key = dialog.key();

    auto *item = new JsonTreeItem(key, dialog.value(), dialog.nodeType());
    if (!m_model->addItem(m_contextIndex, item)) {
        delete item;
        QMessageBox::warning(this, tr("Warning"), tr("Key already exists."));
    }
}

void MainWindow::onDeleteItem() {
    if (!m_contextIndex.isValid())
        return;

    // 顶层节点代表整个文档，替换成 null 会把内容整体清掉，这里直接拒绝。
    if (!m_model->canRemove(m_contextIndex)) {
        QMessageBox::warning(this, tr("Warning"),
                             tr("The root node cannot be deleted. Delete its child items instead."));
        return;
    }
    m_model->removeRows(m_contextIndex.row(), 1, m_contextIndex.parent());
}

void MainWindow::onCopy() {
    QModelIndex idx = m_treeView->currentIndex();
    if (!idx.isValid()) {
        m_statusLabel->setText(tr("No item selected to copy."));
        return;
    }

    auto *item = static_cast<JsonTreeItem*>(idx.internalPointer());
    auto *parentItem = item->parentItem();
    QString json = m_model->itemToJsonString(idx);
    if (json.isEmpty()) {
        m_statusLabel->setText(tr("Failed to copy item."));
        return;
    }

    // 根节点为单个 value（非 Object/Array）时不做处理
    if (!parentItem || parentItem->nodeType() == JsonNodeType::Root) {
        if (item->nodeType() != JsonNodeType::Object && item->nodeType() != JsonNodeType::Array) {
            QApplication::clipboard()->setText(json);
            return;
        }
        QApplication::clipboard()->setText(json);
        m_statusLabel->setText(tr("Copied to clipboard."));
        return;
    }

    // Array 中 → 复制 JSON value
    if (parentItem->nodeType() == JsonNodeType::Array) {
        QApplication::clipboard()->setText(json);
        m_statusLabel->setText(tr("Copied to clipboard."));
        return;
    }

    // Object 中 → 复制 "key": value
    const QByteArray key = item->key().toUtf8();
    yyjson_mut_doc *doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_doc_set_root(doc, yyjson_mut_strncpy(doc, key.constData(), size_t(key.size())));
    char *encoded = yyjson_mut_write(doc, 0, nullptr);
    const QString text = QString::fromUtf8(encoded) + QStringLiteral(": ") + json;
    free(encoded);
    yyjson_mut_doc_free(doc);
    QApplication::clipboard()->setText(text);
    m_statusLabel->setText(tr("Copied to clipboard."));
}

void MainWindow::onCopyKey() {
    QModelIndex idx = m_treeView->currentIndex();
    if (!idx.isValid()) {
        m_statusLabel->setText(tr("No item selected to copy."));
        return;
    }

    auto *item = static_cast<JsonTreeItem*>(idx.internalPointer());
    QApplication::clipboard()->setText(item->key());
    m_statusLabel->setText(tr("Copied key to clipboard."));
}

void MainWindow::onCopyValue() {
    QModelIndex idx = m_treeView->currentIndex();
    if (!idx.isValid()) {
        m_statusLabel->setText(tr("No item selected to copy."));
        return;
    }

    QString json = m_model->itemToJsonString(idx);
    if (!json.isEmpty()) {
        QApplication::clipboard()->setText(json);
        m_statusLabel->setText(tr("Copied value to clipboard."));
    } else {
        m_statusLabel->setText(tr("Failed to copy item."));
    }
}

void MainWindow::setDirty(bool dirty) {
    if (m_isDirty == dirty)
        return;
    m_isDirty = dirty;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    if (m_currentFile.isEmpty()) {
        setWindowTitle(tr("TypeGo.JsonView%1").arg(m_isDirty ? "*" : ""));
    } else {
        setWindowTitle(tr("TypeGo.JsonView - %1%2").arg(m_currentFile).arg(m_isDirty ? "*" : ""));
    }
}

void MainWindow::onPaste() {
    QModelIndex idx = m_treeView->currentIndex();
    if (!idx.isValid()) {
        m_statusLabel->setText(tr("No item selected to paste."));
        return;
    }

    QString text = QApplication::clipboard()->text().trimmed();
    if (text.isEmpty()) {
        m_statusLabel->setText(tr("Clipboard is empty."));
        return;
    }

    auto *item = static_cast<JsonTreeItem*>(idx.internalPointer());
    auto *targetItem = item;

    // 如果在 value 层，自动向上查找一层
    if (item->nodeType() != JsonNodeType::Object && item->nodeType() != JsonNodeType::Array) {
        targetItem = item->parentItem();
        if (!targetItem || (targetItem->nodeType() != JsonNodeType::Object && targetItem->nodeType() != JsonNodeType::Array)) {
            m_statusLabel->setText(tr("Can only paste into Object or Array."));
            return;
        }
        idx = m_model->indexForItem(targetItem);
    }

    // 1. 尝试解析为 "key": value 格式
    QString key, valueText;
    if (tryParseKeyValue(text, key, valueText)) {
        QByteArray valueData = valueText.toUtf8();
        yyjson_doc *valueDoc = yyjson_read(valueData.constData(), valueData.size(), 0);
        if (valueDoc) {
            yyjson_val *root = yyjson_doc_get_root(valueDoc);
            if (targetItem->nodeType() == JsonNodeType::Object) {
                pasteKeyValueIntoObject(idx, targetItem, key, root);
                m_statusLabel->setText(tr("Pasted key and value."));
            } else if (targetItem->nodeType() == JsonNodeType::Array) {
                int row = targetItem->childCount();
                JsonTreeItem *newItem = m_model->createItemFromJson(root, QString());
                m_model->addItem(idx, newItem);
                QModelIndex newIdx = m_model->index(row, 0, idx);
                selectIndex(newIdx);
                m_statusLabel->setText(tr("Pasted value into Array."));
            }
            yyjson_doc_free(valueDoc);
            setDirty(true);
            return;
        }
    }

    // 2. 尝试整体作为 JSON value 解析
    QByteArray utf8Data = text.toUtf8();
    yyjson_doc *doc = yyjson_read(utf8Data.constData(), utf8Data.size(), 0);
    if (doc) {
        yyjson_val *root = yyjson_doc_get_root(doc);
        if (targetItem->nodeType() == JsonNodeType::Array) {
            int row = targetItem->childCount();
            JsonTreeItem *newItem = m_model->createItemFromJson(root, QString());
            m_model->addItem(idx, newItem);
            QModelIndex newIdx = m_model->index(row, 0, idx);
            selectIndex(newIdx);
            m_statusLabel->setText(tr("Pasted JSON into Array."));
            setDirty(true);
        } else {
            m_statusLabel->setText(tr("Object can only paste \"key\": value format."));
        }
        yyjson_doc_free(doc);
        return;
    }

    // 3. 作为纯文本
    if (targetItem->nodeType() == JsonNodeType::Array) {
        int row = targetItem->childCount();
        auto *newItem = new JsonTreeItem(QString(), text, JsonNodeType::String);
        m_model->addItem(idx, newItem);
        QModelIndex newIdx = m_model->index(row, 0, idx);
        selectIndex(newIdx);
        m_statusLabel->setText(tr("Pasted text into Array."));
        setDirty(true);
    } else {
        m_statusLabel->setText(tr("Object can only paste \"key\": value format."));
    }
}

bool MainWindow::tryParseKeyValue(const QString &text, QString &outKey, QString &outValue) {
    const QByteArray wrapped = (QStringLiteral("{") + text + QStringLiteral("}")).toUtf8();
    yyjson_doc *doc = yyjson_read(wrapped.constData(), size_t(wrapped.size()), 0);
    if (!doc) return false;
    auto *object = yyjson_doc_get_root(doc);
    const bool valid = yyjson_obj_size(object) == 1;
    if (valid) {
        yyjson_obj_iter iter = yyjson_obj_iter_with(object);
        auto *key = yyjson_obj_iter_next(&iter);
        outKey = QString::fromUtf8(yyjson_get_str(key), qsizetype(yyjson_get_len(key)));
        char *json = yyjson_val_write(yyjson_obj_iter_get_val(key), 0, nullptr);
        outValue = QString::fromUtf8(json);
        free(json);
    }
    yyjson_doc_free(doc);
    return valid;
}

void MainWindow::pasteKeyValueIntoObject(const QModelIndex &parentIdx, JsonTreeItem *parentItem, const QString &key, yyjson_val *root) {
    JsonTreeItem *existing = nullptr;
    for (int i = 0; i < parentItem->childCount(); ++i) {
        if (parentItem->child(i)->key() == key) {
            existing = parentItem->child(i);
            break;
        }
    }
    if (existing) {
        int ret = QMessageBox::question(this, tr("Paste"),
                                        tr("Key \"%1\" already exists. Overwrite?").arg(key),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::No);
        if (ret != QMessageBox::Yes)
            return;
        QModelIndex existingIdx = m_model->indexForItem(existing);
        JsonTreeItem *newItem = m_model->createItemFromJson(root, key);
        m_model->replaceItem(existingIdx, newItem);
        selectIndex(existingIdx);
    } else {
        int row = parentItem->childCount();
        JsonTreeItem *newItem = m_model->createItemFromJson(root, key);
        m_model->addItem(parentIdx, newItem);
        QModelIndex newIdx = m_model->index(row, 0, parentIdx);
        selectIndex(newIdx);
    }
}

void MainWindow::selectIndex(const QModelIndex &index) {
    if (!index.isValid())
        return;
    const QModelIndex rowIndex = index.siblingAtColumn(0);
    m_treeView->setCurrentIndex(rowIndex);
    m_treeView->selectionModel()->select(rowIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (confirmLeave()) { saveSettings(); event->accept(); }
    else event->ignore();
}

void MainWindow::onAbout() {
    AboutDialog dialog(this);
    dialog.exec();
}

bool MainWindow::confirmLeave() {
    if (!m_isDirty) return true;
    const auto answer = QMessageBox::warning(this, tr("Save changes"), tr("Save changes before leaving?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) { onSave(); return !m_isDirty; }
    if (m_currentFile.isEmpty() || !m_model->loadJson(m_currentFile)) {
        m_model->loadFromString(QStringLiteral("{}"));
        m_currentFile.clear();
    }
    m_contextIndex = {};
    setDirty(false);
    updateWindowTitle();
    return true;
}
