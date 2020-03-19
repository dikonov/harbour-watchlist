/*
 * harbour-watchlist - Sailfish OS Version
 * Copyright © 2019 Andreas Wüst (andreas.wuest.freelancer@gmail.com)
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "moscowexchangebackend.h"

#include <QDebug>
#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QVariantMap>
#include <QJsonDocument>

// const char MMIME_TYPE_JSON[] = "application/json";
// const char MIME_TYPE_JSON[] = "application/json";

const QString MoscowExchangeBackend::MIME_TYPE_JSON = QString("application/json");

MoscowExchangeBackend::MoscowExchangeBackend(QNetworkAccessManager *manager, const QString &applicationName, const QString applicationVersion, QObject *parent) : QObject(parent) {
    qDebug() << "Initializing Euroinvestor Backend...";
    this->manager = manager;
    this->applicationName = applicationName;
    this->applicationVersion = applicationVersion;
}

MoscowExchangeBackend::~MoscowExchangeBackend() {
    qDebug() << "Shutting down Euroinvestor Backend...";
}

void MoscowExchangeBackend::searchName(const QString &searchString) {
    qDebug() << "MoscowExchangeBackend::searchName";
    QNetworkReply *reply = executeGetRequest(QUrl(MAPI_SEARCH + searchString));

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(handleRequestError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(handleSearchNameFinished()));
}

void MoscowExchangeBackend::searchQuoteForNameSearch(const QString &searchString) {
    qDebug() << "MoscowExchangeBackend::searchQuote";
    QNetworkReply *reply = executeGetRequest(QUrl(MAPI_QUOTE + searchString));

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(handleRequestError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(handleSearchQuoteForNameFinished()));
}

void MoscowExchangeBackend::fetchPricesForChart(const QString &extRefId, const int chartType) {
    qDebug() << "MoscowExchangeBackend::fetchClosePrices";

    QDate today = QDate::currentDate();
    QDate startDate;

    // TODO use constants as well
    switch(chartType) {
        case ChartType::INTRADAY: break;
        case ChartType::MONTH: startDate = today.addMonths(-1); break;
        case ChartType::YEAR: startDate = today.addYears(-1); break;
        case ChartType::THREE_YEARS: startDate = today.addYears(-3); break;
        case ChartType::FIVE_YEARS: startDate = today.addYears(-5); break;
    }

    QString startDateString = startDate.toString("yyyy-MM-dd");

    QNetworkReply *reply;
    if (chartType > 0) {
        reply = executeGetRequest(QUrl(QString(MAPI_CLOSE_PRICES).arg(extRefId).arg(startDateString)));
    } else {
        reply = executeGetRequest(QUrl(QString(MAPI_INTRADAY_PRICES).arg(extRefId)));
    }

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(handleRequestError(QNetworkReply::NetworkError)));
   //  connect(reply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error), this, &MoscowExchangeBackend::handleRequestError);
//    connect(reply, SIGNAL(finished()), this, SLOT(handleFetchPricesForChartFinished()));
    connect(reply, &QNetworkReply::finished, this, &MoscowExchangeBackend::handleFetchPricesForChartFinished);

    reply->setProperty("type", chartType);
}

void MoscowExchangeBackend::searchQuote(const QString &searchString) {
    qDebug() << "MoscowExchangeBackend::searchQuote";
    QNetworkReply *reply = executeGetRequest(QUrl(MAPI_QUOTE + searchString));

    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(handleRequestError(QNetworkReply::NetworkError)));
    connect(reply, SIGNAL(finished()), this, SLOT(handleSearchQuoteFinished()));
}

QNetworkReply *MoscowExchangeBackend::executeGetRequest(const QUrl &url) {
    qDebug() << "MoscowExchangeBackend::executeGetRequest " << url;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, MoscowExchangeBackend::MIME_TYPE_JSON);
    request.setHeader(QNetworkRequest::UserAgentHeader, MUSER_AGENT);

    return manager->get(request);
}

void MoscowExchangeBackend::handleRequestError(QNetworkReply::NetworkError error) {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    qWarning() << "MoscowExchangeBackend::handleRequestError:" << (int)error << reply->errorString() << reply->readAll();

    emit requestError("Return code: " + QString::number((int)error) + " - " + reply->errorString());
}

void MoscowExchangeBackend::handleSearchNameFinished() {
    qDebug() << "MoscowExchangeBackend::handleSearchNameFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        return;
    }

    QByteArray searchReply = reply->readAll();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(searchReply);
    if (jsonDocument.isArray()) {
        QJsonArray responseArray = jsonDocument.array();
        qDebug() << "array size : " << responseArray.size();

        QStringList idList;

        foreach (const QJsonValue & value, responseArray) {
            QJsonObject rootObject = value.toObject();
            QJsonObject sourceObject = rootObject["_source"].toObject();
            idList.append(QString::number(sourceObject.value("id").toInt()));
        }

        QString quoteQueryIds = idList.join(",");

        qDebug() << "MoscowExchangeBackend::handleSearchNameFinished - quoteQueryIds : " << quoteQueryIds;

        searchQuoteForNameSearch(quoteQueryIds);

    } else {
        qDebug() << "not a json object !";
    }
}

void MoscowExchangeBackend::handleSearchQuoteForNameFinished() {
    qDebug() << "MoscowExchangeBackend::handleSearchQuoteForNameFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        return;
    }

    emit searchResultAvailable(processQuoteSearchResult(reply->readAll()));
}

void MoscowExchangeBackend::handleSearchQuoteFinished() {
    qDebug() << "MoscowExchangeBackend::handleSearchQuoteForNameFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        return;
    }

    emit quoteResultAvailable(processQuoteSearchResult(reply->readAll()));
}

void MoscowExchangeBackend::handleFetchPricesForChartFinished() {
    qDebug() << "MoscowExchangeBackend::handleFetchPricesForChartFinished";
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        return;
    }

    QByteArray resultByteArray = reply->readAll();
    QString result = QString(resultByteArray);

    qDebug() << "MoscowExchangeBackend::handleFetchPricesForChartFinished result " << result;

    QString jsonResponseString = parsePriceResponse(resultByteArray);

    if (!jsonResponseString.isNull()) {
        emit fetchPricesForChartAvailable(jsonResponseString, reply->property("type").toInt());
    }
}

QString MoscowExchangeBackend::parsePriceResponse(QByteArray reply) {
    QJsonDocument jsonDocument = QJsonDocument::fromJson(reply);
    if (!jsonDocument.isArray()) {
        qDebug() << "not a json array!";
        return QString();
    }

    QJsonArray responseArray = jsonDocument.array();
    QJsonDocument resultDocument;
    QJsonArray resultArray;

    double min = -1;
    double max = -1;

    foreach (const QJsonValue & value, responseArray) {
        QJsonObject rootObject = value.toObject();
        QJsonObject resultObject;

        QJsonValue updatedAt = rootObject.value("timestamp");
        QDateTime dateTimeUpdatedAt = QDateTime::fromString(updatedAt.toString(), Qt::ISODate);

        double closeValue = rootObject.value("close").toDouble();

        if (min == -1) {
            min = closeValue;
        } else if (closeValue < min) {
            min = closeValue;
        }
        if (max == -1) {
            max = closeValue;
        } else if (closeValue > max) {
            max = closeValue;
        }

        resultObject.insert("x", dateTimeUpdatedAt.toMSecsSinceEpoch() / 1000);
        resultObject.insert("y", closeValue);

        resultArray.push_back(resultObject);
    }

    // top / bottom margin for chart - if the difference is too small - rounding makes no sense.
    double roundedMin = (max - min > 1.0) ? floor(min) : min;
    double roundedMax = (max - min > 1.0) ? ceil(max) : max;

    // determine how many fraction digits the y-axis is supposed to display
    int fractionsDigits = 1;
    if (max - min > 10.0) {
        fractionsDigits = 0;
    } else if (max - min < 2) {
        fractionsDigits = 2;
    }

    // resultDocument.setArray(resultArray);
    QJsonObject resultObject;
    resultObject.insert("min", roundedMin);
    resultObject.insert("max", roundedMax);
    resultObject.insert("fractionDigits", fractionsDigits);
    resultObject.insert("data", resultArray);

    resultDocument.setObject(resultObject);

    QString dataToString(resultDocument.toJson());
    return dataToString;
}

QString MoscowExchangeBackend::processQuoteSearchResult(QByteArray searchReply) {
    qDebug() << "MoscowExchangeBackend::processQuoteSearchResult";
    QJsonDocument jsonDocument = QJsonDocument::fromJson(searchReply);
    if (!jsonDocument.isArray()) {
        qDebug() << "not a json array!";
    }

    QJsonArray responseArray = jsonDocument.array();
    QJsonDocument resultDocument;
    QJsonArray resultArray;

    foreach (const QJsonValue & value, responseArray) {
        QJsonObject rootObject = value.toObject();
        QJsonObject exchangeObject = rootObject["exchange"].toObject();

        QJsonObject resultObject;
        resultObject.insert("extRefId", rootObject.value("id"));
        resultObject.insert("name", rootObject.value("name"));
        resultObject.insert("currency", rootObject.value("currency"));
        resultObject.insert("price", rootObject.value("last"));
        resultObject.insert("symbol1", rootObject.value("symbol"));
        resultObject.insert("isin", rootObject.value("isin"));
        resultObject.insert("stockMarketName", exchangeObject.value("name"));
        resultObject.insert("changeAbsolute", rootObject.value("change"));
        resultObject.insert("changeRelative", rootObject.value("changeInPercentage"));
        resultObject.insert("high", rootObject.value("high"));
        resultObject.insert("low", rootObject.value("low"));
        resultObject.insert("ask", rootObject.value("ask"));
        resultObject.insert("bid", rootObject.value("bid"));
        resultObject.insert("volume", rootObject.value("volume"));
        resultObject.insert("numberOfStocks", rootObject.value("numberOfStocks"));

        QJsonValue updatedAt = rootObject.value("updatedAt");
        // TODO move date formatting to a separate method
        QDateTime dateTimeUpdatedAt = QDateTime::fromString(updatedAt.toString(), Qt::ISODate);
        QString updateAtString = dateTimeUpdatedAt.toString("yyyy-MM-dd") + " " + dateTimeUpdatedAt.toString("hh:mm:ss");
        resultObject.insert("quoteTimestamp", updateAtString);

        QDateTime dateTimeNow = QDateTime::currentDateTime();
        QString nowString = dateTimeNow.toString("yyyy-MM-dd") + " " + dateTimeNow.toString("hh:mm:ss");
        resultObject.insert("lastChangeTimestamp", nowString);

        resultArray.push_back(resultObject);
    }

    resultDocument.setArray(resultArray);
    QString dataToString(resultDocument.toJson());

    return dataToString;
}
