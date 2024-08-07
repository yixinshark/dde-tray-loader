// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sniprotocolhandler.h"
#include "abstracttrayprotocol.h"
#include "statusnotifieriteminterface.h"

#include "util.h"
#include "plugin.h"

#include <dbusmenu-qt5/dbusmenuimporter.h>

#include <QMouseEvent>
#include <QWindow>

// Q_LOGGING_CATEGORY(sniTrayLod, "dde.shell.tray.sni")

namespace tray {
static QString sniPfrefix = QStringLiteral("SNI:");

SniTrayProtocol::SniTrayProtocol(QObject *parent)
    : AbstractTrayProtocol(parent)
    , m_trayManager(new OrgKdeStatusNotifierWatcherInterface("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", QDBusConnection::sessionBus(), this))
{
    connect(m_trayManager, &OrgKdeStatusNotifierWatcherInterface::StatusNotifierItemRegistered, this, &SniTrayProtocol::registedItemChanged);
    connect(m_trayManager, &OrgKdeStatusNotifierWatcherInterface::StatusNotifierItemUnregistered, this, &SniTrayProtocol::registedItemChanged);
    QMetaObject::invokeMethod(this, &SniTrayProtocol::registedItemChanged, Qt::QueuedConnection);
}

SniTrayProtocol::~SniTrayProtocol()
{
    m_registedItem.clear();
}

void SniTrayProtocol::registedItemChanged()
{
    auto currentRegistedItems = m_trayManager->registeredStatusNotifierItems();

    if (currentRegistedItems == m_registedItem.keys()) {
        return;
    }

    for (auto currentRegistedItem : currentRegistedItems) {
        if (!m_registedItem.contains(currentRegistedItem)) {
            auto trayHandler = QSharedPointer<SniTrayProtocolHandler>(new SniTrayProtocolHandler(currentRegistedItem));
            m_registedItem.insert(currentRegistedItem, trayHandler);
            Q_EMIT AbstractTrayProtocol::trayCreated(trayHandler.get());
        }
    }

    for (auto alreadyRegistedItem : m_registedItem.keys()) {
        if (!currentRegistedItems.contains(alreadyRegistedItem)) {
            if (auto value = m_registedItem.value(alreadyRegistedItem, nullptr)) {
                m_registedItem.remove(alreadyRegistedItem);
            }
        }
    }
}

SniTrayProtocolHandler::SniTrayProtocolHandler(const QString &sniServicePath, QObject *parent)
    : AbstractTrayProtocolHandler(parent)
    , m_tooltip (new QLabel())
{
    auto pair = serviceAndPath(sniServicePath);
    m_sniInter = new StatusNotifierItem(pair.first, pair.second, QDBusConnection::sessionBus(), this);
    m_dbusMenuImporter = new DBusMenuImporter(pair.first, m_sniInter->menu().path(), ASYNCHRONOUS, this);

    connect(m_sniInter, &StatusNotifierItem::NewIcon, this, &SniTrayProtocolHandler::iconChanged);
    connect(m_sniInter, &StatusNotifierItem::NewOverlayIcon, this, &SniTrayProtocolHandler::overlayIconChanged);
    connect(m_sniInter, &StatusNotifierItem::NewAttentionIcon, this, &SniTrayProtocolHandler::attentionIconChanged);

    connect(m_sniInter, &StatusNotifierItem::NewTitle, this, &SniTrayProtocolHandler::titleChanged);
    connect(m_sniInter, &StatusNotifierItem::NewStatus, this, &SniTrayProtocolHandler::statusChanged);
    connect(m_sniInter, &StatusNotifierItem::NewToolTip, this, &SniTrayProtocolHandler::tooltiChanged);
}

SniTrayProtocolHandler::~SniTrayProtocolHandler()
{
}

uint32_t SniTrayProtocolHandler::windowId() const
{
    return m_sniInter->windowId();
}

QString SniTrayProtocolHandler::id() const
{
    return sniPfrefix + UTIL->getProcExe(QDBusConnection::sessionBus().interface()->servicePid(m_sniInter->service())) + QString("-%1").arg(windowId());
}
    
QString SniTrayProtocolHandler::title() const
{
    return m_sniInter->title();
}

QString SniTrayProtocolHandler::status() const
{
    return m_sniInter->status();
}

QWidget* SniTrayProtocolHandler::tooltip() const
{
    if (!m_sniInter->toolTip().title.isEmpty()) {
        m_tooltip->setText(m_sniInter->toolTip().title);
        return m_tooltip;
    }

    return nullptr;
}

QString SniTrayProtocolHandler::category() const
{
    return m_sniInter->category();
}

QIcon SniTrayProtocolHandler::overlayIcon() const
{
    auto iconName = m_sniInter->overlayIconName();
    if (!iconName.isEmpty()) {
        return QIcon::fromTheme(iconName);
    }

    auto icon = dbusImageList2QIcon(m_sniInter->overlayIconPixmap());
    return icon;
}

QIcon SniTrayProtocolHandler::attentionIcon() const
{
    auto iconName = m_sniInter->attentionIconName();
    if (!iconName.isEmpty()) {
        return QIcon::fromTheme(iconName);
    }

    auto icon = dbusImageList2QIcon(m_sniInter->attentionIconPixmap());
    return icon;
}

QIcon SniTrayProtocolHandler::icon() const
{
    const QString iconName = m_sniInter->iconName();
    const QString iconThemePath = m_sniInter->iconThemePath();
    QIcon resIcon;

    if (!iconName.isEmpty()) {
        resIcon = QIcon::fromTheme(iconName);
        if (!resIcon.isNull()) {
            return resIcon;
        }
    }

    if (!iconThemePath.isEmpty()) {
        resIcon = QIcon(iconThemePath + QDir::separator() + iconName);
        if (!resIcon.isNull()) {
            return resIcon;
        }
    }

    return dbusImageList2QIcon(m_sniInter->iconPixmap());
}

bool SniTrayProtocolHandler::enabled() const
{
    return m_sniInter->isValid();
}

bool SniTrayProtocolHandler::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parent()) {
        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_sniInter->Activate(0, 0);
            } else if (mouseEvent->button() == Qt::RightButton) {
                auto menu = m_dbusMenuImporter->menu();
                if (!menu) return false;
                menu->setFixedSize(menu->sizeHint());
                auto pa = menu->palette();
                pa.setColor(QPalette::ColorRole::Window, Qt::transparent);
                menu->setPalette(pa);
                menu->winId();

                auto widget = static_cast<QWidget*>(parent());
                auto geometry = widget->window()->windowHandle()->geometry();
                auto pluginPopup = Plugin::PluginPopup::get(menu->windowHandle());
                pluginPopup->setPluginId("application-tray");
                pluginPopup->setItemKey(id());
                pluginPopup->setPopupType(Plugin::PluginPopup::PopupTypeMenu);
                const auto offset = mouseEvent->pos();
                pluginPopup->setX(geometry.x() + offset.x());
                pluginPopup->setY(geometry.y() + offset.y());
                // FIXME: show() will not get inputfocus while exec() case a ui issue
                menu->exec();
            }
        }
    }

    return false;
}

QPair<QString, QString> SniTrayProtocolHandler::serviceAndPath(const QString &servicePath)
{
    QStringList list = servicePath.split("/");
    QPair<QString, QString> pair;
    pair.first = list.takeFirst();

    for (auto i : list) {
        pair.second.append("/");
        pair.second.append(i);
    }

    return pair;
}

QIcon SniTrayProtocolHandler::dbusImageList2QIcon(const DBusImageList &dbusImageList)
{
    QIcon res;
    if (!dbusImageList.isEmpty() && !dbusImageList.first().pixels.isEmpty()) {
        for (auto image = dbusImageList.begin(); image < dbusImageList.end(); image++) {
            const char *image_data = image->pixels.data();
            if (QSysInfo::ByteOrder == QSysInfo::LittleEndian) {
                for (int i = 0; i < image->pixels.size(); i += 4) {
                    *(qint32 *)(image_data + i) = qFromBigEndian(*(qint32 *)(image_data + i));
                }
            }

            QImage qimage((const uchar *)image->pixels.constData(), image->width, image->height, QImage::Format_ARGB32);
            res.addPixmap(QPixmap::fromImage(qimage));
        }
    }

    return res;
}


}
