/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QPLATFORMWINDOW_H
#define QPLATFORMWINDOW_H

//
//  W A R N I N G
//  -------------
//
// This file is part of the QPA API and is not meant to be used
// in applications. Usage of this API may make your code
// source and binary incompatible with future versions of Qt.
//

#include <QtCore/qscopedpointer.h>
#include <QtCore/qrect.h>
#include <QtCore/qmargins.h>
#include <QtCore/qstring.h>
#include <QtGui/qwindowdefs.h>
#include <QtGui/qwindow.h>
#include <qpa/qplatformopenglcontext.h>
#include <qpa/qplatformsurface.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE


class QPlatformScreen;
class QPlatformWindowPrivate;
class QWindow;
class QIcon;
class QRegion;

class Q_GUI_EXPORT QPlatformWindow : public QPlatformSurface
{
    Q_DECLARE_PRIVATE(QPlatformWindow)
public:
    explicit QPlatformWindow(QWindow *window);
    virtual ~QPlatformWindow();

    QWindow *window() const;
    QPlatformWindow *parent() const;

    QPlatformScreen *screen() const;

    virtual QSurfaceFormat format() const;

    virtual void setGeometry(const QRect &rect);
    virtual QRect geometry() const;

    virtual QMargins frameMargins() const;

    virtual void setVisible(bool visible);
    virtual Qt::WindowFlags setWindowFlags(Qt::WindowFlags flags);
    virtual Qt::WindowState setWindowState(Qt::WindowState state);

    virtual WId winId() const;
    virtual void setParent(const QPlatformWindow *window);

    virtual void setWindowTitle(const QString &title);
    virtual void setWindowIcon(const QIcon &icon);
    virtual void raise();
    virtual void lower();

    virtual bool isExposed() const;

    virtual void propagateSizeHints();

    virtual void setOpacity(qreal level);
    virtual void setMask(const QRegion &region);
    virtual void requestActivateWindow();

    virtual void handleContentOrientationChange(Qt::ScreenOrientation orientation);
    virtual Qt::ScreenOrientation requestWindowOrientation(Qt::ScreenOrientation orientation);

    virtual bool setKeyboardGrabEnabled(bool grab);
    virtual bool setMouseGrabEnabled(bool grab);

    virtual bool setWindowModified(bool modified);

    virtual void windowEvent(QEvent *event);

    virtual bool startSystemResize(const QPoint &pos, Qt::Corner corner);

protected:
    QScopedPointer<QPlatformWindowPrivate> d_ptr;
private:
    Q_DISABLE_COPY(QPlatformWindow)
};

QT_END_NAMESPACE

QT_END_HEADER
#endif //QPLATFORMWINDOW_H
