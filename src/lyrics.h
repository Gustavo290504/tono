#pragma once

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantList>

class QNetworkReply;

// Lyrics for the song that's playing, from LRCLIB (lrclib.net): a free,
// open lyrics database that needs no account or key, and has time-synced
// lyrics for many songs, which let the view follow the music line by line.
//
// Only the title, artist, album and length of the song are sent, and only
// when the lyrics view is open.
class Lyrics : public QObject {
    Q_OBJECT
    // idle, loading, found, instrumental, missing or failed.
    Q_PROPERTY(QString status READ status NOTIFY changed)
    // [{time: ms, text}] in order; time is -1 for unsynced lyrics.
    Q_PROPERTY(QVariantList lines READ lines NOTIFY changed)
    Q_PROPERTY(bool synced READ synced NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    struct Line {
        qint64 ms = -1;
        QString text;
    };
    struct Result {
        QString status = "missing";
        QList<Line> lines;
        bool synced = false;
    };

    explicit Lyrics(QObject *parent = nullptr, const QUrl &service = QUrl("https://lrclib.net"));

    QString status() const { return m_status; }
    QVariantList lines() const;
    bool synced() const { return m_result.synced; }
    QString error() const { return m_error; }

    // Looks the song up, unless it's the one already shown or loading.
    Q_INVOKABLE void lookup(const QString &title, const QString &artist, const QString &album, qint64 lengthMs);
    Q_INVOKABLE void retry();
    // The line being sung at `ms`, or -1 before the first one.
    Q_INVOKABLE int indexAt(qint64 ms) const { return indexAt(m_result.lines, ms); }

    static QList<Line> parseLrc(const QString &lrc);
    static QList<Line> parsePlain(const QString &text);
    static Result parseRecord(const QJsonObject &record);
    // The best of LRCLIB's search results for a song of `lengthMs`: synced
    // lyrics of about the same length first, then any lyrics at all.
    static Result pickSearchResult(const QJsonArray &records, qint64 lengthMs);
    static int indexAt(const QList<Line> &lines, qint64 ms);
    // "Song - 2011 Remaster" and "Song (feat. Someone)" are filed as "Song".
    static QString simplifyTitle(const QString &title);

signals:
    void changed();

private:
    void request(const QUrl &url, bool search);
    void finish(const Result &result);
    void fail(const QString &message);

    QUrl m_service;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QString m_key;
    QString m_title, m_artist, m_album;
    qint64 m_lengthMs = 0;
    QString m_status = "idle";
    QString m_error;
    Result m_result;
    QHash<QString, Result> m_cache;
};
