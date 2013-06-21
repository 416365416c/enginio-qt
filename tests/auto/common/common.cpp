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

#include <Enginio/enginioclient.h>
#include <Enginio/enginioreply.h>
#include <QtCore/qjsonarray.h>
#include <QtTest/QSignalSpy>
#include <QtTest/QtTest>

#include "common.h"

namespace EnginioTests {

static const QByteArray GET = QByteArrayLiteral("GET");
static const QByteArray POST = QByteArrayLiteral("POST");
static const QByteArray DELETE = QByteArrayLiteral("DELETE");

EnginioBackendManager::EnginioBackendManager(QObject *parent)
    : QObject(parent)
    , _email(qgetenv("ENGINIO_EMAIL_ADDRESS"))
    , _password(qgetenv("ENGINIO_LOGIN_PASSWORD"))
    , _url(EnginioTests::TESTAPP_URL)
{
    if (_email.isEmpty() || _password.isEmpty()) {
        qDebug("Needed environment variables ENGINIO_EMAIL_ADDRESS, ENGINIO_LOGIN_PASSWORD are not set. Backend setup failed!");
        return;
    }

    _headers["Accept"] = QStringLiteral("application/json");

    QObject::connect(&_client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    QObject::connect(&_client, SIGNAL(finished(EnginioReply *)), this, SLOT(finished(EnginioReply *)));

    if (!authenticate())
        qDebug("ERROR: Session authentication failed!");
}

EnginioBackendManager::~EnginioBackendManager()
{}

void EnginioBackendManager::finished(EnginioReply *reply)
{
    Q_ASSERT(reply);
    _responseData = reply->data();
}

void EnginioBackendManager::error(EnginioReply *reply)
{
    Q_ASSERT(reply);
    qDebug() << "\n\n### ERROR";
    qDebug() << reply->errorString();
    reply->dumpDebugInfo();
    qDebug() << "\n###\n";
}

bool EnginioBackendManager::synchronousRequest(const QByteArray &httpOperation, const QJsonObject &data)
{
    QSignalSpy finishedSpy(&_client, SIGNAL(finished(EnginioReply *)));
    QSignalSpy errorSpy(&_client, SIGNAL(error(EnginioReply *)));
    _responseData = QJsonObject();
    _client.customRequest(_url, httpOperation, data);
    return finishedSpy.wait(10000) && !errorSpy.count();
}

bool EnginioBackendManager::authenticate()
{
    QJsonObject credentials;
    credentials["email"] = _email;
    credentials["password"] = _password;
    QJsonObject obj;
    obj["payload"] = credentials;
    obj["headers"] = _headers;
    _url.setPath(QStringLiteral("/v1/account/auth/identity"));

    // Authenticate developer
    synchronousRequest(POST, obj);
    QString sessionToken = _responseData["sessionToken"].toString();
    _headers["Enginio-Backend-Session"] = sessionToken;

    return !sessionToken.isEmpty();
}

QString EnginioBackendManager::getAppId(const QString &backendName)
{
    QString appId = _backendEnvironments.value(backendName).first().toObject()["appId"].toString();

    if (!appId.isEmpty())
        return appId;

    QJsonArray results = getAllBackends();
    foreach (const QJsonValue& value, results) {
        QJsonObject data = value.toObject();
        if (data["name"].toString() == backendName) {
            appId = data["id"].toString();
            break;
        }
    }

    return appId;
}

QJsonArray EnginioBackendManager::getAllBackends()
{
    QJsonObject obj;
    obj["headers"] = _headers;
    _url.setPath("/v1/account/apps");

    synchronousRequest(GET, obj);
    return _responseData["results"].toArray();
}

QJsonArray EnginioBackendManager::getEnvironments(const QString &backendName)
{
    QJsonArray environments = _backendEnvironments.value(backendName);

    if (!environments.isEmpty())
        return environments;

    QString appPath = QStringLiteral("/v1/account/apps/");
    QString appId = getAppId(backendName);
    appPath.append(appId);
    QJsonObject obj;
    obj["headers"] = _headers;
    _url.setPath(appPath);

    if (synchronousRequest(GET, obj)) {
        environments = _responseData["environments"].toArray();
        _backendEnvironments[backendName] = environments;
    }

    return environments;
}

bool EnginioBackendManager::removeAppWithId(const QString &appId)
{
    if (appId.isEmpty())
        return false;

    QJsonObject obj;
    obj["headers"] = _headers;
    QString appsPath = QStringLiteral("/v1/account/apps/");
    appsPath.append(appId);
    _url.setPath(appsPath);
    return synchronousRequest(DELETE, obj);
}

bool EnginioBackendManager::createBackend(const QString &backendName)
{
    qDebug("## Creating backend: %s", backendName.toUtf8().data());

    QJsonObject obj;
    obj["headers"] = _headers;
    QJsonObject backend;
    backend["name"] = backendName;
    obj["payload"] = backend;
    _url.setPath("/v1/account/apps");

    if (!synchronousRequest(POST, obj))
        return false;

    _backendEnvironments[backendName] = _responseData["environments"].toArray();
    return true;
}

bool EnginioBackendManager::removeBackend(const QString &backendName)
{
    qDebug("## Deleting backend: %s", backendName.toUtf8().data());
    if (!removeAppWithId(getAppId(backendName)))
        return false;

    _backendEnvironments.remove(backendName);
    return true;
}

bool EnginioBackendManager::createObjectType(const QString &backendName, const QString &environment, const QJsonObject &schema)
{
    qDebug("## Creating new object type: %s on backend: %s", schema["name"].toString().toUtf8().data(), backendName.toUtf8().data());
    QJsonArray environments = getEnvironments(backendName);
    if (environments.isEmpty())
        return false;

    QString backendId;
    QString backendMasterKey;

    foreach (const QJsonValue &value, environments) {
        QJsonObject env = value.toObject();
        if (env["name"].toString() == environment) {
            backendId = env["id"].toString();
            QJsonArray masterKeys = env["masterKeys"].toArray();
            QJsonObject masterKey = masterKeys.first().toObject();
            backendMasterKey = masterKey["key"].toString();
            break;
        }
    }

    _headers["Enginio-Backend-Id"] = backendId;
    _headers["Enginio-Backend-MasterKey"] = backendMasterKey;

    QJsonObject obj;
    obj["headers"] = _headers;
    obj["payload"] = schema;

    _url.setPath("/v1/object_types");
    synchronousRequest(POST, obj);
    return !_responseData["properties"].toArray().isEmpty();
}

QJsonObject EnginioBackendManager::backendApiKeys(const QString &backendName, const QString &environment)
{
    QJsonArray environments = getEnvironments(backendName);

    if (environments.isEmpty())
        return QJsonObject();

    QString backendId;
    QString backendSecret;

    foreach (const QJsonValue &value, environments) {
        QJsonObject env = value.toObject();
        if (env["name"].toString() == environment) {
            backendId = env["id"].toString();
            QJsonArray keys = env["apiKeys"].toArray();
            QJsonObject apiKey = keys.first().toObject();
            backendSecret = apiKey["key"].toString();
            break;
        }
    }

    QJsonObject apiKeys;
    apiKeys["backendId"] = backendId;
    apiKeys["backendSecret"] = backendSecret;

    return apiKeys;
}

void EnginioBackendManager::createUsersAndUserGroups(const QByteArray& backendId, const QByteArray& backendSecret)
{
    // Create some users to be used in later tests
    EnginioClient client;
    QObject::connect(&client, SIGNAL(error(EnginioReply *)), this, SLOT(error(EnginioReply *)));
    client.setBackendId(backendId);
    client.setBackendSecret(backendSecret);
    client.setServiceUrl(EnginioTests::TESTAPP_URL);

    QSignalSpy spy(&client, SIGNAL(finished(EnginioReply*)));
    QSignalSpy spyError(&client, SIGNAL(error(EnginioReply*)));

    QJsonObject obj;
    int spyCount = spy.count();

    for (int i = 0; i < 5; ++i) {
        QJsonObject query;
        query["username"] = QStringLiteral("logintest") + (i ? QString::number(i) : "");
        obj["query"] = query;
        client.query(obj, EnginioClient::UserOperation);
        ++spyCount;
    }

    QTRY_COMPARE(spy.count(), spyCount);
    QCOMPARE(spyError.count(), 0);

    for (int i = 0; i < 5; ++i) {
        EnginioReply *reply = spy[i][0].value<EnginioReply*>();
        QVERIFY(reply);
        QVERIFY(!reply->data().isEmpty());
        QVERIFY(!reply->data()["query"].isUndefined());
        obj = reply->data()["query"].toObject();
        QVERIFY(!obj.isEmpty());
        obj = obj["query"].toObject();
        QVERIFY(!obj.isEmpty());
        QString identity = obj["username"].toString();
        QVERIFY(!identity.isEmpty());
        QVERIFY(!reply->data()["results"].isUndefined());
        QJsonArray data = reply->data()["results"].toArray();
        if (!data.count()) {
            QJsonObject query;
            query["username"] = identity;
            query["password"] = identity;
            client.create(query, EnginioClient::UserOperation);
            ++spyCount;
            qDebug() << "Creating " << query;
        }
    }

    QTRY_COMPARE(spy.count(), spyCount);
    QCOMPARE(spyError.count(), 0);


    // Create user groups for later tests if the backend does not have them yet.
    spy.clear();
    spyCount = 0;

    for (int i = 0; i < 5; ++i) {
        QJsonObject query;
        query["name"] = "usergroup" + (i ? QString::number(i) : "");
        obj["query"] = query;
        client.query(obj, EnginioClient::UsergroupOperation);
        ++spyCount;
    }

    QTRY_COMPARE(spy.count(), spyCount);
    QCOMPARE(spyError.count(), 0);

    for (int i = 0; i < 5; ++i) {
        EnginioReply *reply = spy[i][0].value<EnginioReply*>();
        QVERIFY(reply);
        QVERIFY(!reply->data().isEmpty());
        QVERIFY(!reply->data()["query"].isUndefined());
        obj = reply->data()["query"].toObject();
        QVERIFY(!obj.isEmpty());
        obj = obj["query"].toObject();
        QVERIFY(!obj.isEmpty());
        QString groupName = obj["name"].toString();
        QVERIFY(!groupName.isEmpty());
        QVERIFY(!reply->data()["results"].isUndefined());
        QJsonArray data = reply->data()["results"].toArray();
        if (!data.count()) {
            QJsonObject query;
            query["name"] = groupName;
            client.create(query, EnginioClient::UsergroupOperation);
            ++spyCount;
            qDebug() << "Creating " << query;
        }
    }

    QTRY_COMPARE(spy.count(), spyCount);
    QCOMPARE(spyError.count(), 0);
}
} // namespace EnginioTests
