/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://qt.digia.com/contact-us
**
** This file is part of the Enginio Qt Client Library.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file. Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
****************************************************************************/

#include "enginioidentity.h"
#include "enginioclient_p.h"
#include "enginioreply.h"

#include <QtCore/qjsonobject.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qstring.h>
#include <QtNetwork/qnetworkreply.h>

/*!
  \class EnginioIdentity
  \inmodule enginio-qt
  \ingroup enginio-client
  \brief Represents a user that is authenticated with the backend
  This class is an abstract base class for the different authentication methods and is never used directly.
*/

/*!
  \fn EnginioIdentity::dataChanged()
  \internal
*/

/*!
    Constructs a new EnginioIdentity with \a parent as QObject parent.
*/
EnginioIdentity::EnginioIdentity(QObject *parent) :
    QObject(parent)
{
}

class EnginioBasicAuthenticationPrivate
{
public:
    class SessionSetterFunctor
    {
        EnginioClientPrivate *_enginio;
        QNetworkReply *_reply;
    public:
        SessionSetterFunctor(EnginioClientPrivate *enginio, QNetworkReply *reply)
            : _enginio(enginio)
            , _reply(reply)
        {}
        void operator ()()
        {
            EnginioReply *ereply = new EnginioReply(_enginio, _reply);
            if (_reply->error() != QNetworkReply::NoError) {
                emit _enginio->q_ptr->sessionAuthenticationError(ereply);
                // TODO does ereply leak? Yes potentially. We need to think about the ownership
            } else {
                _enginio->setIdentityToken(ereply);
                _reply->deleteLater();
            }
        }
    };

    EnginioBasicAuthenticationPrivate(EnginioBasicAuthentication *q)
        : q_ptr(q)
    {}
    EnginioBasicAuthentication *q_ptr;
    QString _user;
    QString _pass;
};

/*!
  \class EnginioBasicAuthentication
  \inmodule enginio-qt
  \ingroup enginio-client
  \brief Represents a user that is authenticated directly by the backend.

  This class can authenticate a user by verifying the user's login and password.
  The user has to exist in the backend already.
*/

/*!
  \property EnginioBasicAuthentication::user
  This property contains the user name used for authentication.
*/

/*!
  \property EnginioBasicAuthentication::password
  This property contains the password used for authentication.
*/

/*!
  Constructs a EnginioBasicAuthentication instance with \a parent as QObject parent.
*/
EnginioBasicAuthentication::EnginioBasicAuthentication(QObject *parent)
    : EnginioIdentity(parent)
    , d_ptr(new EnginioBasicAuthenticationPrivate(this))
{
    connect(this, &EnginioBasicAuthentication::userChanged, this, &EnginioIdentity::dataChanged);
    connect(this, &EnginioBasicAuthentication::passwordChanged, this, &EnginioIdentity::dataChanged);
}

/*!
  Destructs this EnginioBasicAuthentication instance.
*/
EnginioBasicAuthentication::~EnginioBasicAuthentication()
{}

QString EnginioBasicAuthentication::user() const
{
    return d_ptr->_user;
}

void EnginioBasicAuthentication::setUser(const QString &user)
{
    if (d_ptr->_user == user)
        return;
    d_ptr->_user = user;
    emit userChanged(user);
}

QString EnginioBasicAuthentication::password() const
{
    return d_ptr->_pass;
}

void EnginioBasicAuthentication::setPassword(const QString &password)
{
    if (d_ptr->_pass == password)
        return;
    d_ptr->_pass = password;
    emit userChanged(password);
}

struct DisconnectConnection
{
    QMetaObject::Connection _connection;

    DisconnectConnection(const QMetaObject::Connection& connection)
        : _connection(connection)
    {}

    void operator ()() const
    {
        QObject::disconnect(_connection);
    }
};

/*!
  \internal
*/
void EnginioBasicAuthentication::prepareSessionToken(EnginioClientPrivate *enginio)
{
    Q_ASSERT(enginio);
    Q_ASSERT(enginio->identity());

    QJsonObject data;
    data[EnginioString::username] = d_ptr->_user;
    data[EnginioString::password] = d_ptr->_pass;
    QNetworkReply *reply = enginio->identify(data);
    QMetaObject::Connection requestFinished = QObject::connect(reply, &QNetworkReply::finished, EnginioBasicAuthenticationPrivate::SessionSetterFunctor(enginio, reply));
    QObject::connect(enginio->q_ptr, &EnginioClient::destroyed, DisconnectConnection(requestFinished));
}

