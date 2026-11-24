#include "lyrics.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>
#include <algorithm>

namespace {
// LRCLIB asks clients to say who they are. This names the app only, never
// the person using it.
const QByteArray UserAgent = "Tono/0.3.0 (Omarchy music app)";

// Two recordings whose lengths differ by more than this are different cuts
// (a radio edit, a live take), and synced timings from one drift on the other.
constexpr qint64 LengthTolerance = 3000;
}

Lyrics::Lyrics(QObject *parent, const QUrl &service) : QObject(parent), m_service(service) {}

QVariantList Lyrics::lines() const {
    QVariantList list;
    list.reserve(m_result.lines.size());
    for (const auto &line : m_result.lines)
        list << QVariantMap{{"time", m_result.synced ? line.ms : -1}, {"text", line.text}};
    return list;
}

QList<Lyrics::Line> Lyrics::parseLrc(const QString &lrc) {
    // A line may carry several stamps when it repeats ("[00:12.00][01:30.00]
    // Chorus"); tags like [ar:Artist] have no digits and are skipped.
    static const QRegularExpression stamp(R"(\[(\d{1,3}):(\d{1,2})(?:[.:](\d{1,3}))?\])");
    QList<Line> lines;
    for (const QString &raw : lrc.split('\n')) {
        auto it = stamp.globalMatch(raw);
        QList<qint64> times;
        qsizetype textStart = 0;
        while (it.hasNext()) {
            const auto match = it.next();
            if (match.capturedStart() != textStart)
                break;
            const QString fraction = match.captured(3);
            const qint64 fractionMs = fraction.isEmpty() ? 0
                                    : fraction.size() == 1 ? fraction.toInt() * 100
                                    : fraction.size() == 2 ? fraction.toInt() * 10 : fraction.toInt();
            times << match.captured(1).toLongLong() * 60000 + match.captured(2).toLongLong() * 1000 + fractionMs;
            textStart = match.capturedEnd();
        }
        const QString text = raw.mid(textStart).trimmed();
        for (const qint64 ms : std::as_const(times))
            lines << Line{ms, text};
    }
    std::stable_sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) { return a.ms < b.ms; });
    return lines;
}

QList<Lyrics::Line> Lyrics::parsePlain(const QString &text) {
    QList<Line> lines;
    for (const QString &raw : text.split('\n'))
        lines << Line{-1, raw.trimmed()};
    while (!lines.isEmpty() && lines.last().text.isEmpty())
        lines.removeLast();
    return lines;
}

Lyrics::Result Lyrics::parseRecord(const QJsonObject &record) {
    Result result;
    if (record.value("instrumental").toBool()) {
        result.status = "instrumental";
        return result;
    }
    const auto synced = parseLrc(record.value("syncedLyrics").toString());
    if (!synced.isEmpty()) {
        result.status = "found";
        result.lines = synced;
        result.synced = true;
        return result;
    }
    const auto plain = parsePlain(record.value("plainLyrics").toString());
    if (!plain.isEmpty()) {
        result.status = "found";
        result.lines = plain;
    }
    return result;
}

Lyrics::Result Lyrics::pickSearchResult(const QJsonArray &records, qint64 lengthMs) {
    auto close = [&](const QJsonObject &record) {
        return lengthMs <= 0 || qAbs(qint64(record.value("duration").toDouble() * 1000) - lengthMs) <= LengthTolerance;
    };
    // Same cut with synced lyrics, then the same cut with any lyrics.
    for (const bool wantSynced : {true, false})
        for (const auto &value : records) {
            const auto record = value.toObject();
            const Result result = parseRecord(record);
            if (close(record) && result.status == "found" && (result.synced || !wantSynced))
                return result;
        }
    // Another cut's words still read right, but not its timings.
    for (const auto &value : records) {
        Result result = parseRecord(value.toObject());
        if (result.status == "found") {
            for (auto &line : result.lines)
                line.ms = -1;
            result.synced = false;
            return result;
        }
    }
    for (const auto &value : records)
        if (value.toObject().value("instrumental").toBool())
            return parseRecord(value.toObject());
    return {};
}

int Lyrics::indexAt(const QList<Line> &lines, qint64 ms) {
    if (lines.isEmpty() || lines.first().ms < 0 || ms < lines.first().ms)
        return -1;
    const auto after = std::upper_bound(lines.begin(), lines.end(), ms,
                                        [](qint64 value, const Line &line) { return value < line.ms; });
    return int(after - lines.begin()) - 1;
}

QString Lyrics::simplifyTitle(const QString &title) {
    QString simple = title;
    static const QRegularExpression suffix(R"(\s+-\s+.*$)");
    static const QRegularExpression extra(R"(\s*[\(\[](feat\.?|ft\.?|with|from|remaster|live|mono|stereo)[^\)\]]*[\)\]])",
                                          QRegularExpression::CaseInsensitiveOption);
    simple.remove(suffix);
    simple.remove(extra);
    simple = simple.trimmed();
    return simple.isEmpty() ? title : simple;
}

void Lyrics::lookup(const QString &title, const QString &artist, const QString &album, qint64 lengthMs) {
    if (title.trimmed().isEmpty()) {
        if (m_reply)
            m_reply->abort();
        m_key.clear();
        m_result = {};
        m_status = "idle";
        m_error.clear();
        emit changed();
        return;
    }

    const QString key = QStringList{title, artist, album, QString::number(lengthMs / 1000)}.join('\n').toLower();
    if (key == m_key && m_status != "failed")
        return;
    m_key = key;
    m_title = title;
    m_artist = artist;
    m_album = album;
    m_lengthMs = lengthMs;

    if (m_reply)
        m_reply->abort();
    if (m_cache.contains(key)) {
        finish(m_cache.value(key));
        return;
    }

    m_status = "loading";
    m_error.clear();
    m_result = {};
    emit changed();

    // An exact match needs the length; without one, search straight away.
    if (lengthMs <= 0) {
        request({}, true);
        return;
    }
    QUrl url = m_service.resolved(QUrl("/api/get"));
    QUrlQuery query;
    query.addQueryItem("track_name", title);
    query.addQueryItem("artist_name", artist);
    if (!album.isEmpty())
        query.addQueryItem("album_name", album);
    query.addQueryItem("duration", QString::number(qRound(lengthMs / 1000.0)));
    url.setQuery(query);
    request(url, false);
}

void Lyrics::retry() {
    m_key.clear();
    lookup(m_title, m_artist, m_album, m_lengthMs);
}

void Lyrics::request(const QUrl &exact, bool search) {
    QUrl url = exact;
    if (search) {
        url = m_service.resolved(QUrl("/api/search"));
        QUrlQuery query;
        query.addQueryItem("track_name", simplifyTitle(m_title));
        query.addQueryItem("artist_name", m_artist);
        url.setQuery(query);
    }

    QNetworkRequest httpRequest(url);
    httpRequest.setHeader(QNetworkRequest::UserAgentHeader, UserAgent);
    httpRequest.setTransferTimeout(10000);
    QNetworkReply *reply = m_network.get(httpRequest);
    m_reply = reply;
    const QString key = m_key;

    connect(reply, &QNetworkReply::finished, this, [this, reply, search, key] {
        reply->deleteLater();
        // A newer song was asked for meanwhile.
        if (key != m_key || reply->error() == QNetworkReply::OperationCanceledError)
            return;
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (!search && code == 404) {
            request({}, true);
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Couldn't reach LRCLIB: %1").arg(reply->errorString()));
            return;
        }

        const auto document = QJsonDocument::fromJson(body);
        if (!search) {
            const Result result = parseRecord(document.object());
            // A record without words may have a better twin in the search.
            if (result.status == "missing")
                request({}, true);
            else
                finish(result);
            return;
        }
        finish(pickSearchResult(document.array(), m_lengthMs));
    });
}

void Lyrics::finish(const Result &result) {
    if (m_cache.size() > 200)
        m_cache.clear();
    m_cache.insert(m_key, result);
    m_result = result;
    m_status = result.status;
    m_error.clear();
    emit changed();
}

void Lyrics::fail(const QString &message) {
    m_result = {};
    m_status = "failed";
    m_error = message;
    emit changed();
}
