/* This file is part of the appmenu-qt project
   Copyright 2011 Canonical
   Author: Aurelien Gateau <aurelien.gateau@canonical.com>

   appmenu-qt is free software; you can redistribute it and/or modify it under
   the terms of the GNU Lesser General Public License (LGPL) as published by
   the Free Software Foundation; either version 3 of the License.

   appmenu-qt is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
   for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with appmenu-qt.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "appmenuplatformmenubar.h"

// dbusmenu-qt
#include <dbusmenuexporter.h>

// Qt
#include <QActionEvent>
#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QMenuBar>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusServiceWatcher>
#include <QStyle>

QT_BEGIN_NAMESPACE

static const char* REGISTRAR_SERVICE = "com.canonical.AppMenu.Registrar";
static const char* REGISTRAR_PATH    = "/com/canonical/AppMenu/Registrar";
static const char* REGISTRAR_IFACE   = "com.canonical.AppMenu.Registrar";

#define LOG qDebug() << "appmenu-qt:" << __FUNCTION__ << __LINE__
#define LOG_VAR(x) qDebug() << "appmenu-qt:" << __FUNCTION__ << __LINE__ << #x ":" << x
#define WARN qWarning() << "appmenu-qt:" << __FUNCTION__ << __LINE__

#if 0
// Debug helper
static QString logMenuBar(QMenuBar* bar)
{
    QStringList lst;
    Q_FOREACH(QAction* action, bar->actions()) {
        lst << action->text();
    }
    return lst.join("|");
}
#endif

/**
 * The menubar adapter communicates over DBus with the menubar renderer.
 * It is responsible for registering windows to it and exposing their menubars
 * if they have one.
 */
class MenuBarAdapter
{
public:
    MenuBarAdapter(QMenuBar*, const QString&);
    ~MenuBarAdapter();
    void addAction(QAction*, QAction* before=0);
    void removeAction(QAction*);

    bool registerWindow();

    void popupAction(QAction*);

    void setAltPressed(bool pressed);

    void resetRegisteredWinId();

private:
    uint m_registeredWinId;
    DBusMenuExporter* m_exporter;
    QMenu* m_rootMenu;
    QMenuBar* m_menuBar;
    QString m_objectPath;
};

AppMenuPlatformMenuBar::~AppMenuPlatformMenuBar()
{
    destroyMenuBar();
}

void AppMenuPlatformMenuBar::init(QMenuBar* _menuBar)
{
    m_nativeMenuBar = NMB_Auto;
    m_altPressed = false;
    m_menuBar = _menuBar;

    static int menuBarId = 1;
    m_objectPath = QString(QLatin1String("/MenuBar/%1")).arg(menuBarId++);
    m_registrarWatcher = new QDBusServiceWatcher(
        REGISTRAR_SERVICE,
        QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForOwnerChange,
        m_menuBar);
    // m_adapter will be created in handleReparent()
    m_adapter = 0;

    connect(m_registrarWatcher, SIGNAL(serviceOwnerChanged(const QString&, const QString&, const QString&)),
            SLOT(slotMenuBarServiceChanged(const QString&, const QString&, const QString&)));
}

void AppMenuPlatformMenuBar::setVisible(bool visible)
{
    m_menuBar->QWidget::setVisible(visible);
}

void AppMenuPlatformMenuBar::actionEvent(QActionEvent* e)
{
    if (m_adapter) {
        if (e->type() == QEvent::ActionAdded) {
            m_adapter->addAction(e->action(), e->before());
        } else if (e->type() == QEvent::ActionRemoved) {
            m_adapter->removeAction(e->action());
        }
    }
}

void AppMenuPlatformMenuBar::handleReparent(QWidget* oldParent, QWidget* newParent, QWidget* oldWindow, QWidget* newWindow)
{
    Q_UNUSED(oldParent)
    Q_UNUSED(newParent)
    if (isNativeMenuBar()) {
        if (m_adapter) {
            if (oldWindow != newWindow) {
                if (checkForOtherMenuBars(newWindow, m_menuBar)) {
                    m_adapter->registerWindow();
                }
            }
        } else {
            createMenuBar();
        }
    }
}

bool AppMenuPlatformMenuBar::allowCornerWidgets() const
{
    return !isNativeMenuBar();
}

void AppMenuPlatformMenuBar::popupAction(QAction* act)
{
    if (act && act->menu()) {
        m_adapter->popupAction(act);
    }
}

void AppMenuPlatformMenuBar::setNativeMenuBar(bool native)
{
    if (m_nativeMenuBar == NMB_DisabledByEnv) {
        WARN << "native menubar disabled by environment variable";
        return;
    }
    NativeMenuBar newValue = native ? NMB_Enabled : NMB_Disabled;
    if (m_nativeMenuBar == NMB_Auto || m_nativeMenuBar != newValue) {
        m_nativeMenuBar = newValue;
        if (m_nativeMenuBar == NMB_Disabled) {
            destroyMenuBar();
        }
    }
}

bool AppMenuPlatformMenuBar::isNativeMenuBar() const
{
    if (m_nativeMenuBar == NMB_DisabledByEnv) {
        return false;
    }
    if (m_nativeMenuBar == NMB_Auto) {
        return !QApplication::instance()->testAttribute(Qt::AA_DontUseNativeMenuBar);
    }
    return m_nativeMenuBar == NMB_Enabled;
}

bool AppMenuPlatformMenuBar::shortcutsHandledByNativeMenuBar() const
{
    return false;
}

bool AppMenuPlatformMenuBar::menuBarEventFilter(QObject*, QEvent* event)
{
    if (event->type() == QEvent::WinIdChange || event->type() == QEvent::Show) {
        if (isNativeMenuBar() && m_adapter) {
            QMetaObject::invokeMethod(this, "registerWindow", Qt::QueuedConnection);
        }
    }

    if (event->type() == QEvent::Hide) {
        if (isNativeMenuBar() && m_adapter) {
            m_adapter->resetRegisteredWinId();
        }
    }

    if (event->type() == QEvent::ShortcutOverride) {
        QKeyEvent *kev = static_cast<QKeyEvent*>(event);
        if ((kev->key() == Qt::Key_Alt || kev->key() == Qt::Key_Meta)
            && kev->modifiers() == Qt::AltModifier) {
            setAltPressed(true);
        }
    }
    return false;
}

bool AppMenuPlatformMenuBar::eventFilter(QObject*, QEvent* event)
{
    if (!m_altPressed) {
        WARN << "called with m_altPressed=false. Should not happen.";
        return false;
    }
    switch (event->type()) {
    case QEvent::KeyRelease:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::ActivationChange:
        setAltPressed(false);
        break;
    default:
        break;
    }
    return false;
}

void AppMenuPlatformMenuBar::setAltPressed(bool pressed)
{
    m_altPressed = pressed;
    if (pressed) {
        qApp->installEventFilter(this);
    } else {
        qApp->removeEventFilter(this);
    }
    if (m_adapter) {
        m_adapter->setAltPressed(pressed);
    }
}

void AppMenuPlatformMenuBar::slotMenuBarServiceChanged(const QString &/*serviceName*/, const QString &/*oldOwner*/, const QString& newOwner)
{
    if (m_nativeMenuBar == NMB_DisabledByEnv || m_nativeMenuBar == NMB_Disabled) {
        return;
    }

    if (newOwner.isEmpty()) {
        destroyMenuBar();
        QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar, true);
        m_menuBar->updateGeometry();
        m_menuBar->setVisible(false);
        m_menuBar->setVisible(true);
        return;
    }

    // We reach this point if there is a registrar
    QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar, false);
    m_menuBar->updateGeometry();
    m_menuBar->setVisible(true);
    m_menuBar->setVisible(false);
    delete m_adapter;
    m_adapter = 0;
    createMenuBar();
}

void AppMenuPlatformMenuBar::registerWindow()
{
    if (m_adapter) {
        m_adapter->registerWindow();
    }
}

void AppMenuPlatformMenuBar::createMenuBar()
{
    static bool firstCall = true;
    static bool envSaysNo = !qgetenv("QT_X11_NO_NATIVE_MENUBAR").isEmpty();
    static bool envSaysBoth = qgetenv("APPMENU_DISPLAY_BOTH") == "1";

    if (!m_menuBar->parentWidget()) {
        return;
    }

    m_adapter = 0;

    if (!firstCall && !envSaysBoth && QApplication::testAttribute(Qt::AA_DontUseNativeMenuBar)) {
        return;
    }

    if (envSaysNo) {
        if (firstCall) {
            firstCall = false;
            m_nativeMenuBar = NMB_DisabledByEnv;
            QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar, true);
        }
        return;
    }

    if (!checkForOtherMenuBars(m_menuBar->window(), m_menuBar)) {
        return;
    }

    m_adapter = new MenuBarAdapter(m_menuBar, m_objectPath);
    if (!m_adapter->registerWindow()) {
        destroyMenuBar();
    }

    if (firstCall) {
        firstCall = false;
        bool dontUseNativeMenuBar = !m_adapter;
        if (envSaysBoth) {
            // Make the rest of Qt think we do not use the native menubar, so
            // that space for the menubar widget is correctly allocated
            dontUseNativeMenuBar = true;
        }
        QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar, dontUseNativeMenuBar);
    }
}

void AppMenuPlatformMenuBar::destroyMenuBar()
{
    delete m_adapter;
    m_adapter = 0;
}


inline int computeWidgetDepth(QWidget* widget)
{
    int depth = 0;
    for (; widget; widget = widget->parentWidget(), ++depth) {}
    return depth;
}

/**
 * Check if the window contains other menubars. If it is the case, only allow
 * the one which has the shortest parent->child depth to be exposed.
 * Returns true if @a newMenuBar should be exposed.
 */
bool AppMenuPlatformMenuBar::checkForOtherMenuBars(QWidget* window, QMenuBar* newMenuBar)
{
    Q_ASSERT(window);
    Q_ASSERT(newMenuBar);
    QList<QMenuBar*> lst = window->findChildren<QMenuBar*>();
    Q_ASSERT(!lst.isEmpty());
    if (lst.count() == 1) {
        // Only one menubar, assume it is newMenuBar
        return true;
    }

    // Multiple menubars, compute their depths
    QMultiMap<int, QMenuBar*> depths;
    Q_FOREACH(QMenuBar* menuBar, lst) {
        const int depth = computeWidgetDepth(menuBar);
        depths.insert(depth, menuBar);
    }

    QMultiMap<int, QMenuBar*>::Iterator it = depths.begin();
    if (it.value() == newMenuBar) {
        // newMenuBar should be exposed
        QMultiMap<int, QMenuBar*>::Iterator end = depths.end();
        ++it;
        for (;it != end; ++it) {
            it.value()->setNativeMenuBar(false);
        }
        return true;
    } else {
        // newMenuBar should not be exposed
        setNativeMenuBar(false);
        return false;
    }
}

///////////////////////////////////////////////////////////
MenuBarAdapter::MenuBarAdapter(QMenuBar* _menuBar, const QString& _objectPath)
: m_registeredWinId(0)
, m_exporter(0)
, m_rootMenu(new QMenu)
, m_menuBar(_menuBar)
, m_objectPath(_objectPath)
{
}

MenuBarAdapter::~MenuBarAdapter()
{
    delete m_exporter;
    m_exporter = 0;
    delete m_rootMenu;
    m_rootMenu = 0;
}

bool MenuBarAdapter::registerWindow()
{
    if (!m_menuBar->window()) {
        WARN << "No parent for this menubar";
        return false;
    }

    uint winId = m_menuBar->window()->winId();
    if (winId == m_registeredWinId) {
        return true;
    }

    QDBusInterface host(REGISTRAR_SERVICE, REGISTRAR_PATH, REGISTRAR_IFACE);
    if (!host.isValid()) {
        return false;
    }

    Q_FOREACH(QAction *action, m_menuBar->actions()) {
        if (!action->isSeparator()) {
            m_rootMenu->addAction(action);
        }
    }

    if (m_rootMenu->actions().isEmpty()) {
        // Menubar is empty. We do not create an exporter because doing so
        // would cause the menubar renderer to show an empty menubar. Not
        // exporting the menubar makes it possible for the menubar renderer to
        // show a generic window menubar (for example the Plasma menubar widget
        // has a "File > Close" menubar)
        return true;
    }

    if (!m_exporter) {
        m_exporter = new DBusMenuExporter(m_objectPath, m_rootMenu);
    }

    m_registeredWinId = winId;
    QVariant path = QVariant::fromValue<QDBusObjectPath>(QDBusObjectPath(m_objectPath));
    host.asyncCall(QLatin1String("RegisterWindow"), QVariant(winId), path);
    return true;
}

void MenuBarAdapter::resetRegisteredWinId()
{
    m_registeredWinId = 0;
}

void MenuBarAdapter::addAction(QAction* action, QAction* before)
{
    if (!action->isSeparator()) {
        m_rootMenu->insertAction(before, action);
    }
    if (!m_registeredWinId) {
        // If this is the first action, the window is not registered yet. Do it
        // now.
        registerWindow();
    }
}

void MenuBarAdapter::removeAction(QAction* action)
{
    m_rootMenu->removeAction(action);
}

void MenuBarAdapter::popupAction(QAction* action)
{
    m_exporter->activateAction(action);
}

void MenuBarAdapter::setAltPressed(bool pressed)
{
    // m_exporter may be 0 if the menubar is empty.
    if (m_exporter) {
        m_exporter->setStatus(pressed ? "notice" : "normal");
    }
}


///////////////////////////////////////////////////////////
QAbstractPlatformMenuBar* AppMenuPlatformMenuBarFactory::create()
{
    return new AppMenuPlatformMenuBar;
}

Q_EXPORT_PLUGIN2(appmenuplatformmenubar, AppMenuPlatformMenuBarFactory)

QT_END_NAMESPACE

#include <appmenuplatformmenubar.moc>
