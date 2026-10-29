#include "AddItemDialog.h"
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <yyjson.h>

AddItemDialog::AddItemDialog(bool isArrayParent, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Add Item"));

    auto *layout = new QFormLayout(this);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("String"), static_cast<int>(JsonNodeType::String));
    m_typeCombo->addItem(tr("Number"), static_cast<int>(JsonNodeType::Number));
    m_typeCombo->addItem(tr("Boolean"), static_cast<int>(JsonNodeType::Boolean));
    m_typeCombo->addItem(tr("Object"), static_cast<int>(JsonNodeType::Object));
    m_typeCombo->addItem(tr("Array"), static_cast<int>(JsonNodeType::Array));
    m_typeCombo->addItem(tr("Null"), static_cast<int>(JsonNodeType::Null));
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AddItemDialog::onTypeChanged);
    layout->addRow(tr("Type:"), m_typeCombo);

    m_keyEdit = new QLineEdit(this);
    if (isArrayParent) {
        m_keyEdit->setEnabled(false);
        m_keyEdit->setPlaceholderText(tr("Auto"));
    }
    layout->addRow(tr("Key:"), m_keyEdit);

    m_valueEdit = new QLineEdit(this);
    m_boolCheck = new QCheckBox(this);
    m_boolCheck->setText(tr("true"));

    m_valueStack = new QStackedWidget(this);
    m_valueStack->addWidget(m_valueEdit);
    m_valueStack->addWidget(m_boolCheck);
    layout->addRow(tr("Value:"), m_valueStack);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (nodeType() == JsonNodeType::Number) {
            const QByteArray data = m_valueEdit->text().toUtf8();
            yyjson_doc *doc = yyjson_read(data.constData(), size_t(data.size()), 0);
            const bool valid = doc && yyjson_is_num(yyjson_doc_get_root(doc));
            if (doc) yyjson_doc_free(doc);
            if (!valid) { QMessageBox::warning(this, tr("Warning"), tr("Enter a valid JSON number.")); return; }
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    onTypeChanged(0);
}

void AddItemDialog::onTypeChanged(int index) {
    Q_UNUSED(index)
    auto type = static_cast<JsonNodeType>(m_typeCombo->currentData().toInt());
    if (type == JsonNodeType::Object || type == JsonNodeType::Array || type == JsonNodeType::Null) {
        m_valueStack->setCurrentWidget(m_valueEdit);
        m_valueEdit->setEnabled(false);
        m_valueEdit->clear();
    } else if (type == JsonNodeType::Boolean) {
        m_valueEdit->setEnabled(true);
        m_valueStack->setCurrentWidget(m_boolCheck);
    } else {
        m_valueEdit->setEnabled(true);
        m_valueStack->setCurrentWidget(m_valueEdit);
        m_valueEdit->setPlaceholderText("");
    }
}

QString AddItemDialog::key() const {
    return m_keyEdit->text();
}

JsonNodeType AddItemDialog::nodeType() const {
    return static_cast<JsonNodeType>(m_typeCombo->currentData().toInt());
}

QVariant AddItemDialog::value() const {
    auto type = nodeType();
    switch (type) {
    case JsonNodeType::String:
        return m_valueEdit->text();
    case JsonNodeType::Number: {
        bool ok;
        qlonglong ll = m_valueEdit->text().toLongLong(&ok);
        if (ok) return QVariant(ll);
        qulonglong ull = m_valueEdit->text().toULongLong(&ok);
        if (ok) return QVariant::fromValue(ull);
        return m_valueEdit->text().toDouble();
    }
    case JsonNodeType::Boolean:
        return m_boolCheck->isChecked();
    case JsonNodeType::Null:
    case JsonNodeType::Object:
    case JsonNodeType::Array:
        return QVariant();
    default:
        return QVariant();
    }
}
