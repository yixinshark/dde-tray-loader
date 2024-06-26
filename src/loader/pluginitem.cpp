// SPDX-FileCopyrightText: 2011 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "constants.h"
#include "pluginitem.h"
#include "../tray-wayland-integration/plugin.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <qglobal.h>
#include <qobject.h>
#include <QMenu>

PluginItem::PluginItem(PluginsItemInterface *pluginItemInterface, const QString &itemKey, QWidget *parent)
    : QWidget(parent)
    , m_pluginsItemInterface(pluginItemInterface)
    , m_pluginsItemInterfacev2(dynamic_cast<PluginsItemInterfaceV2*>(pluginItemInterface))
    , m_itemKey(itemKey)
    , m_menu(new QMenu(this))
{
}

PluginItem::~PluginItem() = default;

void PluginItem::mouseLeftButtonClicked()
{
    const QString command = m_pluginsItemInterface->itemCommand(m_itemKey);
    if (!command.isEmpty()) {
        qInfo() << "command: " << command;
        QProcess::startDetached(command, QStringList());
        return;
    }

    auto setPluginMsg = [this]  {
        auto pluginsItemInterfaceV2 = dynamic_cast<PluginsItemInterfaceV2 *>(m_pluginsItemInterface);
        if (!pluginsItemInterfaceV2)
            return;

        QJsonObject obj;
        obj[Dock::MSG_TYPE] = Dock::MSG_APPLET_CONTAINER;
        obj[Dock::MSG_DATA] = Dock::APPLET_CONTAINER_DOCK;

        QJsonDocument msg;
        msg.setObject(obj);

        pluginsItemInterfaceV2->message(msg.toJson());
    };

    if (auto popup = m_pluginsItemInterface->itemPopupApplet(m_itemKey)) {
        if (!m_isPanelPopupShow && popup->isVisible()) {
            popup->windowHandle()->hide();
        }

        if (m_isPanelPopupShow) {
            popup->hide();
            m_isPanelPopupShow = false;
            return;
        }

        setPluginMsg();
        popup->setParent(nullptr);
        popup->setAttribute(Qt::WA_TranslucentBackground);
        popup->winId();

        auto geometry = windowHandle()->geometry();
        bool hasCreated = Plugin::PluginPopup::contains(popup->windowHandle());
        auto pluginPopup = Plugin::PluginPopup::get(popup->windowHandle());
        if (!hasCreated) {
            connect(pluginPopup, &Plugin::PluginPopup::eventGeometry, this, &PluginItem::updatePopupSize);
        }

        pluginPopup->setPluginId(m_pluginsItemInterface->pluginName());
        pluginPopup->setItemKey(m_itemKey);
        pluginPopup->setPopupType(Plugin::PluginPopup::PopupTypePanel);
        pluginPopup->setX(geometry.x() + geometry.width() / 2), pluginPopup->setY(geometry.y() + geometry.height() / 2);
        m_isPanelPopupShow = true;
        popup->show();
    }
}

void PluginItem::mouseRightButtonClicked()
{
    if (m_menu->actions().isEmpty()) {
        initPluginMenu();
    }

    qDebug() << "mouseRightButtonClicked:" << m_itemKey << m_menu->actions().size();
    m_menu->setAttribute(Qt::WA_TranslucentBackground, true);
    // FIXME: qt5integration drawMenuItemBackground will draw a background event is's transparent
    auto pa = this->palette();
    pa.setColor(QPalette::ColorRole::Window, Qt::transparent);
    m_menu->setPalette(pa);
    m_menu->winId();

    auto geometry = windowHandle()->geometry();
    auto pluginPopup = Plugin::PluginPopup::get(m_menu->windowHandle());
    pluginPopup->setPluginId(m_pluginsItemInterface->pluginName());
    pluginPopup->setItemKey(m_itemKey);
    pluginPopup->setPopupType(Plugin::PluginPopup::PopupTypeMenu);
    pluginPopup->setX(geometry.x() + geometry.width() / 2);
    pluginPopup->setY(geometry.y() + geometry.height() / 2);
    m_menu->setFixedSize(m_menu->sizeHint());
    m_isPanelPopupShow = false;
    m_menu->exec();
}

void PluginItem::mousePressEvent(QMouseEvent *e)
{
    QWidget::mousePressEvent(e);
}

void PluginItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        mouseLeftButtonClicked();
    } else if (e->button() == Qt::RightButton) {
        mouseRightButtonClicked();
    }
    QWidget::mouseReleaseEvent(e);
}

void PluginItem::enterEvent(QEvent *event)
{
    showPluginTooltip();

    QWidget::enterEvent(event);
}

void PluginItem::leaveEvent(QEvent *event)
{
    auto tooltip = m_pluginsItemInterface->itemTipsWidget(m_itemKey);
    if (tooltip && tooltip->windowHandle())
        tooltip->windowHandle()->hide();
}

QWidget* PluginItem::centralWidget()
{
    return m_pluginsItemInterface->itemWidget(m_itemKey);
}

PluginsItemInterface * PluginItem::pluginsItemInterface()
{
    return m_pluginsItemInterface;
}

void PluginItem::updateItemWidgetSize(const QSize &size)
{
    centralWidget()->setFixedSize(size);
    update();
}

int PluginItem::pluginFlags() const
{
    return m_pluginFlags;
}

void PluginItem::setPluginFlags(int flags)
{
    m_pluginFlags = flags;
}

void PluginItem::updatePopupSize(const QRect &rect)
{
    if (auto popup = m_pluginsItemInterface->itemPopupApplet(m_itemKey)) {
        popup->setFixedSize(rect.size());
        popup->update();
    }
}

void PluginItem::init()
{
    winId();
    setAttribute(Qt::WA_TranslucentBackground);

    auto hLayout = new QHBoxLayout;
    hLayout->addWidget(centralWidget());
    hLayout->setMargin(0);
    hLayout->setSpacing(0);

    setLayout(hLayout);
}

void PluginItem::initPluginMenu()
{
    const QString menuJson = m_pluginsItemInterface->itemContextMenu(m_itemKey);
    if (menuJson.isEmpty()) {
        qWarning() << "itemContextMenu is empty!";
        return;
    }

    QJsonDocument jsonDocument = QJsonDocument::fromJson(menuJson.toLocal8Bit().data());
    if (jsonDocument.isNull()) {
        qWarning() << "jsonDocument is null!";
        return;
    }

    QJsonObject jsonMenu = jsonDocument.object();

    QJsonArray jsonMenuItems = jsonMenu.value("items").toArray();
    for (auto item : jsonMenuItems) {
        QJsonObject itemObj = item.toObject();
        QAction *action = new QAction(itemObj.value("itemText").toString());
        action->setCheckable(itemObj.value("isCheckable").toBool());
        action->setChecked(itemObj.value("checked").toBool());
        action->setData(itemObj.value("itemId").toString());
        action->setEnabled(itemObj.value("isActive").toBool());
        m_menu->addAction(action);
    }
}

void PluginItem::showPluginTooltip()
{
    auto popup = m_pluginsItemInterface->itemPopupApplet(m_itemKey);
    if (popup && popup->isVisible())
        popup->hide();

    showTooltip(m_itemKey);
}

void PluginItem::showTooltip(const QString &itemKey)
{
    QMetaObject::invokeMethod(this, [itemKey, this](){
        auto toolTip = m_pluginsItemInterface->itemTipsWidget(itemKey);
        if (!toolTip) {
            qDebug() << "no tooltip";
            return;
        }

        toolTip->setParent(nullptr);
        toolTip->setAttribute(Qt::WA_TranslucentBackground);
        toolTip->winId();

        auto geometry = windowHandle()->geometry();
        auto pluginPopup = Plugin::PluginPopup::get(toolTip->windowHandle());
        pluginPopup->setPluginId(m_pluginsItemInterface->pluginName());
        pluginPopup->setItemKey(itemKey);
        pluginPopup->setPopupType(Plugin::PluginPopup::PopupTypeTooltip);
        pluginPopup->setX(geometry.x() + geometry.width() / 2), pluginPopup->setY(geometry.y() + geometry.height() / 2);
        if (toolTip->sizeHint().width() > 0 && toolTip->sizeHint().height() > 0) {
            toolTip->setFixedSize(toolTip->sizeHint());
        }
        toolTip->show();
    });
}
