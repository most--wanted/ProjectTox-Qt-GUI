/*
    Copyright (C) 2013 by Maxim Biro <nurupo.contributions@gmail.com>

    This file is part of Tox Qt GUI.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the COPYING file for more details.
*/

#include "mainwindow.hpp"

#include "aboutdialog.hpp"
#include "addfrienddialog.hpp"
#include "appinfo.hpp"
#include "closeapplicationdialog.hpp"
#include "friendrequestdialog.hpp"
#include "pageswidget.hpp"
#include "Settings/settings.hpp"

#include <QApplication>
#include <QDesktopWidget>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    const int screenWidth = QApplication::desktop()->width();
    const int screenHeight = QApplication::desktop()->height();
    const int appWidth = 500;
    const int appHeight = 300;

    setGeometry((screenWidth - appWidth) / 2, (screenHeight - appHeight) / 2, appWidth, appHeight);

    setObjectName("MainWindow");
    setWindowTitle("developers' test version, not for public use");
    setWindowIcon(QIcon(":/icons/icon64.png"));
    setContextMenuPolicy(Qt::PreventContextMenu);

    // install Unicode 6.1 supporting font
    QFontDatabase::addApplicationFont("://DejaVuSans.ttf");

    splitterWidget = new QSplitter(Qt::Horizontal, this);
    splitterWidget->setObjectName("FriendsSplitter");

    QWidget* friendsLayout = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(friendsLayout);
    layout->setMargin(0);
    layout->setSpacing(0);

    ourUserItem = new OurUserItemWidget(this);
    friendsWidget = new FriendsWidget(friendsLayout);

    // Create toolbar
    QToolBar *toolBar = new QToolBar(this);
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->setFocusPolicy(Qt::ClickFocus);

    QToolButton *addFriendButton = new QToolButton(toolBar);
    addFriendButton->setIcon(QIcon("://icons/user_add.png"));
    addFriendButton->setToolTip(tr("Add friend"));
    connect(addFriendButton, &QToolButton::clicked, this, &MainWindow::onAddFriendButtonClicked);

    QWidget *spacer = new QWidget(toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QToolButton *menuButton = new QToolButton(toolBar);
    menuButton->setIcon(QIcon("://icons/cog.png"));
    menuButton->setToolTip(tr("Menu"));
    menuButton->setPopupMode(QToolButton::InstantPopup);
    QMenu *menu = new QMenu(menuButton);
    settingsAction = menu->addAction(QIcon(":/icons/setting_tools.png"), tr("Settings"), this, SLOT(onSettingsActionTriggered()));
    menu->addSeparator();
    menu->addAction(tr("About %1").arg(AppInfo::name), this, SLOT(onAboutAppActionTriggered()));
    menu->addAction(tr("About Qt"), qApp, SLOT(aboutQt()));
    menu->addSeparator();
    menu->addAction(tr("Quit"), this, SLOT(onTrayMenuQuitApplicationActionTriggered()), QKeySequence::Quit);
    menuButton->setMenu(menu);

    toolBar->addWidget(addFriendButton);
    toolBar->addWidget(spacer);
    toolBar->addWidget(menuButton);
    // Create toolbar end

    layout->addWidget(ourUserItem);
    layout->addWidget(friendsWidget);
    layout->addWidget(toolBar);

    PagesWidget* pages = new PagesWidget(this);
    connect(friendsWidget, &FriendsWidget::friendAdded, pages, &PagesWidget::addPage);
    connect(friendsWidget, &FriendsWidget::friendSelectionChanged, pages, &PagesWidget::activatePage);

    //FIXME: start core in a separate function
    //all connections to `core` should be done after its creation because it registers some types
    core = new Core();

    coreThread = new QThread(this);
    core->moveToThread(coreThread);
    connect(coreThread, &QThread::started, core, &Core::start);

    qRegisterMetaType<Status>("Status");

    connect(core, &Core::connected, this, &MainWindow::onConnected);
    connect(core, &Core::disconnected, this, &MainWindow::onDisconnected);
    connect(core, &Core::friendRequestRecieved, this, &MainWindow::onFriendRequestRecieved);
    connect(core, SIGNAL(friendStatusChanged(int, Status)), friendsWidget, SLOT(setStatus(int, Status)));
    connect(core, &Core::friendAddressGenerated, ourUserItem, &OurUserItemWidget::setFriendAddress);
    connect(core, &Core::friendAdded, friendsWidget, &FriendsWidget::addFriend);
    connect(core, &Core::friendMessageRecieved, pages, &PagesWidget::messageReceived);
    connect(core, &Core::actionReceived, pages, &PagesWidget::actionReceived);
    connect(core, &Core::friendUsernameChanged, friendsWidget, &FriendsWidget::setUsername);
    connect(core, &Core::friendUsernameChanged, pages, &PagesWidget::usernameChanged);
    connect(core, &Core::friendRemoved, friendsWidget, &FriendsWidget::removeFriend);
    connect(core, &Core::friendRemoved, pages, &PagesWidget::removePage);
    connect(core, &Core::failedToRemoveFriend, this, &MainWindow::onFailedToRemoveFriend);
    connect(core, &Core::failedToAddFriend, this, &MainWindow::onFailedToAddFriend);
    connect(core, &Core::messageSentResult, pages, &PagesWidget::messageSentResult);
    connect(core, &Core::actionSentResult, pages, &PagesWidget::actionResult);
    connect(core, &Core::friendStatusMessageChanged, friendsWidget, &FriendsWidget::setStatusMessage);
    connect(core, &Core::friendStatusMessageChanged, pages, &PagesWidget::statusMessageChanged);

    connect(core, &Core::failedToStart, this, &MainWindow::onFailedToStartCore);

    connect(core, &Core::friendStatusChanged, pages, &PagesWidget::statusChanged);

    connect(this, &MainWindow::statusSet, core, &Core::setStatus);
    connect(core, &Core::statusSet, this, &MainWindow::onStatusSet);

    connect(core, &Core::friendLastSeenChanged, friendsWidget, &FriendsWidget::setLastSeen);

    connect(this, &MainWindow::friendRequested, core, &Core::requestFriendship);

    connect(this, &MainWindow::friendRequestAccepted, core, &Core::acceptFriendRequest);

    connect(ourUserItem, &OurUserItemWidget::usernameChanged, core, &Core::setUsername);
    connect(core, &Core::usernameSet, ourUserItem, &OurUserItemWidget::setUsername);

    connect(ourUserItem, &OurUserItemWidget::statusMessageChanged, core, &Core::setStatusMessage);
    connect(core, &Core::statusMessageSet, ourUserItem, &OurUserItemWidget::setStatusMessage);

    connect(ourUserItem, &OurUserItemWidget::statusSelected, core, &Core::setStatus);
    connect(core, &Core::statusSet, ourUserItem, &OurUserItemWidget::setStatus);

    connect(pages, &PagesWidget::sendMessage, core, &Core::sendMessage);
    connect(pages, &PagesWidget::sendAction,  core, &Core::sendAction);

    connect(friendsWidget, &FriendsWidget::friendRemoved, core, &Core::removeFriend);

    coreThread->start();

    splitterWidget->addWidget(friendsLayout);
    splitterWidget->addWidget(pages);
    splitterWidget->setStretchFactor(0, 0);
    splitterWidget->setStretchFactor(1, 1);
    setCentralWidget(splitterWidget);

    Settings::getInstance().restoreGeometryState(splitterWidget);
    Settings::getInstance().restoreGeometryState(this);

    trayIcon = new QSystemTrayIcon(QIcon(":/icons/icon64.png"), this);
    QMenu* trayMenu = new QMenu(this);
    trayMenuShowHideAction = trayMenu->addAction(tr("Hide"), this, SLOT(onShowHideWindow()));
    trayMenu->addSeparator();
    for (int i = 0; i <= StatusHelper::MAX_STATUS; i ++) {
        StatusHelper::Info statusInfo = StatusHelper::getInfo(i);
        QAction* statusAction = new QAction(QIcon(statusInfo.iconPath), statusInfo.name, trayMenu);
        statusAction->setData(i);
        connect(statusAction, &QAction::triggered, this, &MainWindow::onTrayMenuStatusActionTriggered);
        trayMenuStatusActions << statusAction;
    }
    trayMenu->addActions(QList<QAction*>() << trayMenuStatusActions);
    trayMenu->addSeparator();
    trayMenu->addActions(QList<QAction*>() << settingsAction);
    trayMenu->addSeparator();
    trayMenu->addAction(tr("Quit"), this, SLOT(onTrayMenuQuitApplicationActionTriggered()), QKeySequence::Quit);
    trayIcon->setContextMenu(trayMenu);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconClick);
    trayIcon->show();
}

MainWindow::~MainWindow()
{
    coreThread->quit();
    coreThread->wait();
    delete core;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    bool minimize = Settings::getInstance().isMinimizeOnCloseEnabled();
    if (isVisible() && minimize) {
        hide();
        event->ignore();
    } else {
        Settings::getInstance().saveGeometryState(this);
        Settings::getInstance().saveGeometryState(splitterWidget);
        QMainWindow::closeEvent(event);
        qApp->quit();
    }
}

void MainWindow::onFriendRequestRecieved(const QString& userId, const QString& message)
{
    FriendRequestDialog dialog(this, userId, message);

    if (dialog.exec() == QDialog::Accepted) {
        emit friendRequestAccepted(userId);
    }
}

void MainWindow::onAddFriendButtonClicked()
{
    AddFriendDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        emit friendRequested(dialog.getFriendAddress(), dialog.getMessage());
    }
}

void MainWindow::onConnected()
{
    emit statusSet(Status::Online);
}

void MainWindow::onDisconnected()
{
    emit statusSet(Status::Offline);
}

void MainWindow::onFailedToRemoveFriend(int friendId)
{
    QMessageBox critical(this);
    critical.setText(QString("Couldn't remove friend \"%1\"").arg(friendsWidget->getUsername(friendId)));
    critical.setIcon(QMessageBox::Critical);
    critical.exec();
}

void MainWindow::onFailedToAddFriend(const QString& userId)
{
    QMessageBox critical(this);
    critical.setText(QString("Couldn't add friend with User ID\n\"%1\"").arg(userId));
    critical.setIcon(QMessageBox::Critical);
    critical.exec();
}

void MainWindow::onFailedToStartCore()
{
    QMessageBox critical(this);
    critical.setText("If you see this message that means that something very bad has happened.\n\nYou could have reached a limit on how many instances of this program can be run on a single computer, or you could just run out of memory, or something else horrible has happened.\n\nWhichever is the case, the application will terminate after you close this message.");
    critical.setIcon(QMessageBox::Critical);
    critical.exec();
    qApp->quit();
}

void MainWindow::onSettingsActionTriggered()
{
    Settings::getInstance().executeSettingsDialog(this);
}

void MainWindow::onAboutAppActionTriggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onTrayMenuQuitApplicationActionTriggered()
{
    CloseApplicationDialog dialog(this);
    dialog.exec();
}

void MainWindow::onShowHideWindow()
{
    if (isVisible()) {
        hide();
        trayMenuShowHideAction->setText(tr("Show"));
    } else {
        show();
        setFocus();
        trayMenuShowHideAction->setText(tr("Hide"));
    }
}

void MainWindow::onTrayMenuStatusActionTriggered()
{
    QAction* statusAction = static_cast<QAction*>(sender());
    Status selectedStatus = static_cast<Status>(statusAction->data().toInt());

    if (selectedStatus == Status::Offline) {
        CloseApplicationDialog dialog(this);
        dialog.exec();
    } else {
        emit statusSet(selectedStatus);
    }
}

void MainWindow::onTrayIconClick(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        onShowHideWindow();
    }
}

void MainWindow::onStatusSet(Status status)
{
    int statusValue = static_cast<int>(status);
    for (int i = 0; i <= StatusHelper::MAX_STATUS; i ++) {
        if (trayMenuStatusActions[i]->data().toInt() == statusValue) {
            trayMenuStatusActions[i]->setEnabled(false);
        } else if (!trayMenuStatusActions[i]->isEnabled()) {
            trayMenuStatusActions[i]->setEnabled(true);
        }
    }
}
