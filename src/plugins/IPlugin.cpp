#include "IPlugin.h"
#include <QWidget>

QWidget *IPlugin::createView(QWidget *parent)
{
    QWidget *view = viewUrl();
    if (view && view->parentWidget() != parent) view->setParent(parent);
    return view;
}
