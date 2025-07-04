// SPDX-FileCopyrightText: Copyright (c) Kitware Inc.
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause

#include "pqEventPlayer.h"

#include "pq3DViewEventPlayer.h"
#include "pqAbstractActivateEventPlayer.h"
#include "pqAbstractBooleanEventPlayer.h"
#include "pqAbstractDoubleEventPlayer.h"
#include "pqAbstractIntEventPlayer.h"
#include "pqAbstractItemViewEventPlayer.h"
#include "pqAbstractMiscellaneousEventPlayer.h"
#include "pqAbstractStringEventPlayer.h"
#include "pqBasicWidgetEventPlayer.h"
#include "pqComboBoxEventPlayer.h"
#include "pqCommentEventPlayer.h"
#include "pqEventTypes.h"
#include "pqListViewEventPlayer.h"
#include "pqNativeFileDialogEventPlayer.h"
#include "pqObjectNaming.h"
#include "pqTabBarEventPlayer.h"
#include "pqTableViewEventPlayer.h"
#include "pqTreeViewEventPlayer.h"

#include <QApplication>
#include <QDebug>
#include <QWidget>

// ----------------------------------------------------------------------------
pqEventPlayer::pqEventPlayer() {}

// ----------------------------------------------------------------------------
pqEventPlayer::~pqEventPlayer() {}

// ----------------------------------------------------------------------------
void pqEventPlayer::addDefaultWidgetEventPlayers(pqTestUtility* util)
{
  addWidgetEventPlayer(new pqCommentEventPlayer(util));
  addWidgetEventPlayer(new pqBasicWidgetEventPlayer());
  addWidgetEventPlayer(new pqAbstractActivateEventPlayer());
  addWidgetEventPlayer(new pqAbstractBooleanEventPlayer());
  addWidgetEventPlayer(new pqAbstractDoubleEventPlayer());
  addWidgetEventPlayer(new pqAbstractIntEventPlayer());
  addWidgetEventPlayer(new pqAbstractItemViewEventPlayer());
  addWidgetEventPlayer(new pqAbstractStringEventPlayer());
  addWidgetEventPlayer(new pqTabBarEventPlayer());
  addWidgetEventPlayer(new pqComboBoxEventPlayer());
  addWidgetEventPlayer(new pqTreeViewEventPlayer());
  addWidgetEventPlayer(new pqTableViewEventPlayer());
  addWidgetEventPlayer(new pqListViewEventPlayer());
  addWidgetEventPlayer(new pqAbstractMiscellaneousEventPlayer(util));
  addWidgetEventPlayer(new pq3DViewEventPlayer("QGLWidget"));
  addWidgetEventPlayer(new pqNativeFileDialogEventPlayer(util));
}

// ----------------------------------------------------------------------------
QList<pqWidgetEventPlayer*> pqEventPlayer::players() const
{
  return this->Players;
}

// ----------------------------------------------------------------------------
void pqEventPlayer::addWidgetEventPlayer(pqWidgetEventPlayer* Player)
{
  if (Player)
  {
    // We Check if the Player has already been added previously
    int index = this->getWidgetEventPlayerIndex(QString(Player->metaObject()->className()));
    if (index != -1)
    {
      return;
    }

    this->Players.push_front(Player);
    Player->setParent(this);
  }
}

// ----------------------------------------------------------------------------
bool pqEventPlayer::removeWidgetEventPlayer(const QString& className)
{
  int index = this->getWidgetEventPlayerIndex(className);
  if (index == -1)
  {
    return false;
  }

  this->Players.removeAt(index);
  return true;
}

// ----------------------------------------------------------------------------
pqWidgetEventPlayer* pqEventPlayer::getWidgetEventPlayer(const QString& className)
{
  int index = this->getWidgetEventPlayerIndex(className);
  if (index == -1)
  {
    return 0;
  }

  return this->Players.at(index);
}

// ----------------------------------------------------------------------------
int pqEventPlayer::getWidgetEventPlayerIndex(const QString& className)
{
  for (int i = 0; i < this->Players.count(); ++i)
  {
    if (this->Players.at(i)->metaObject()->className() == className)
    {
      return i;
    }
  }
  return -1;
}

// ----------------------------------------------------------------------------
void pqEventPlayer::playEvent(
  const QString& objectString, const QString& command, const QString& arguments, bool& error)
{
  this->playEvent(objectString, command, arguments, pqEventTypes::ACTION_EVENT, error);
}

// ----------------------------------------------------------------------------
void pqEventPlayer::playEvent(const QString& objectString, const QString& command,
  const QString& arguments, int eventType, bool& error)
{
  Q_EMIT this->eventAboutToBePlayed(objectString, command, arguments);
  // If we can't find an object with the right name, we're done ...
  QObject* const object = pqObjectNaming::GetObject(objectString);

  // Scroll bar depends on monitor's resolution
  if (!object && objectString.contains(QString("QScrollBar")))
  {
    Q_EMIT this->eventPlayed(objectString, command, arguments);
    error = false;
    return;
  }

  if (!object && !command.startsWith("comment"))
  {
    QString errorMsg = QString("In event 'object=%1' 'command=%2' 'arguments=%3':\n%4")
                         .arg(objectString, command, arguments, pqObjectNaming::lastErrorMessage());
    qCritical() << qUtf8Printable(errorMsg);
    Q_EMIT this->errorMessage(errorMsg);
    error = true;
    return;
  }

  // Loop through players until the event gets handled ...
  bool accepted = false;
  bool tmpError = false;
  if (command.startsWith("comment"))
  {
    pqWidgetEventPlayer* widgetPlayer = this->getWidgetEventPlayer(QString("pqCommentEventPlayer"));
    pqCommentEventPlayer* commentPlayer = qobject_cast<pqCommentEventPlayer*>(widgetPlayer);
    if (commentPlayer)
    {
      accepted = commentPlayer->playEvent(object, command, arguments, tmpError);
    }
  }
  else
  {
    for (int i = 0; i != this->Players.size(); ++i)
    {
      accepted = this->Players[i]->playEvent(object, command, arguments, eventType, tmpError);
      if (accepted)
      {
        break;
      }
    }
  }

  // The event wasn't handled at all ...
  if (!accepted)
  {
    QString errorMessage =
      QString("In event 'object=%1' 'command=%2' 'arguments=%3':\nUnhandled event.\n%4")
        .arg(object ? object->objectName() : objectString, command, arguments,
          pqObjectNaming::lastErrorMessage());
    qCritical() << qUtf8Printable(errorMessage);
    Q_EMIT this->errorMessage(errorMessage);
    error = true;
    return;
  }

  // The event was handled, but there was a problem ...
  if (accepted && tmpError)
  {
    QString errorMessage =
      QString("In event 'object=%1' 'command=%2' 'arguments=%3':\nUnhandled error.")
        .arg(object ? object->objectName() : objectString, command, arguments);
    qCritical() << qUtf8Printable(errorMessage);
    Q_EMIT this->errorMessage(errorMessage);
    error = true;
    return;
  }

  // The event was handled successfully ...
  Q_EMIT this->eventPlayed(objectString, command, arguments);
  error = false;
}
