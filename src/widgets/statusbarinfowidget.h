/*
 * Copyright (C) 2016  Marco Gulino <marco@gulinux.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef STATUSBARINFOWIDGET_H
#define STATUSBARINFOWIDGET_H

#include <QWidget>
#include "dptr.h"

class StatusBarInfoWidget : public QWidget
{
  Q_OBJECT
public:
    ~StatusBarInfoWidget();
    StatusBarInfoWidget(QWidget* parent = 0, Qt::WindowFlags = 0);
public slots:
  void deviceConnected(const QString &name);
  void deviceDisconnected();
  void captureFPS(double fps);
  void displayFPS(double fps);
  void zoom(double ratio);
  void temperature(double celsius, bool hide = false);
  void showMessage(const QString &message, long timeout_ms = 5000); // <=0 means forever
  void clearMessage();
private:
  DPTR
};

#endif // STATUSBARINFOWIDGET_H
