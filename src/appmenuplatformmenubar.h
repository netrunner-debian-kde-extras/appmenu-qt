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
#ifndef APPMENUPLATFORMMENUBAR_X11_H
#define APPMENUPLATFORMMENUBAR_X11_H

#include <QObject>


#include <private/qabstractplatformmenubar_p.h>

QT_BEGIN_NAMESPACE

class QMenuBar;
class MenuBarAdapter;
class QDBusServiceWatcher;

class AppMenuPlatformMenuBar : public QObject, public QAbstractPlatformMenuBar
{
    Q_OBJECT
public:
    ~AppMenuPlatformMenuBar();

    virtual void init(QMenuBar*);

    virtual void setVisible(bool visible);

    virtual void actionEvent(QActionEvent*);

    virtual void handleReparent(QWidget* oldParent, QWidget* newParent, QWidget* oldWindow, QWidget* newWindow);

    virtual bool allowCornerWidgets() const;

    virtual void popupAction(QAction*);

    virtual void setNativeMenuBar(bool);
    virtual bool isNativeMenuBar() const;

    virtual bool shortcutsHandledByNativeMenuBar() const;
    virtual bool menuBarEventFilter(QObject*, QEvent* event);

    virtual bool eventFilter(QObject*, QEvent*);

private:
    QMenuBar* m_menuBar;
    MenuBarAdapter* m_adapter;
    enum NativeMenuBar {
        NMB_DisabledByEnv,
        NMB_Disabled,
        NMB_Auto,
        NMB_Enabled
    };
    NativeMenuBar m_nativeMenuBar;

    QDBusServiceWatcher* m_registrarWatcher;
    QString m_objectPath;

    bool m_altPressed;

    void createMenuBar();
    void destroyMenuBar();
    void setAltPressed(bool);

private Q_SLOTS:
    void slotMenuBarServiceChanged(const QString&, const QString&, const QString&);
    void registerWindow();
};

class Q_GUI_EXPORT AppMenuPlatformMenuBarFactory : public QObject, public QPlatformMenuBarFactoryInterface
{
    Q_OBJECT
    Q_INTERFACES(QPlatformMenuBarFactoryInterface:QFactoryInterface)
public:
    virtual QAbstractPlatformMenuBar* create();
    QStringList keys() const {
        return QStringList() << QLatin1String("default");
    }
};

QT_END_NAMESPACE

#endif // APPMENUPLATFORMMENUBAR_X11_H
