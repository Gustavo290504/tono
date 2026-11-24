#pragma once

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

// One media player on the session bus, spoken to over MPRIS
// (org.mpris.MediaPlayer2). Spotify, browsers, mpv and most Linux players
// publish themselves this way, so Tono controls them without API keys.
//
// MPRIS never announces the playback position, only jumps, so the position
// runs on a local clock while playing and is resynced from the player.
class MprisPlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString service READ service CONSTANT)
    Q_PROPERTY(QString identity READ identity NOTIFY identityChanged)
    Q_PROPERTY(QUrl iconUrl READ iconUrl NOTIFY identityChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY statusChanged)
    Q_PROPERTY(QString title READ title NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QUrl artUrl READ artUrl NOTIFY trackChanged)
    Q_PROPERTY(qint64 length READ length NOTIFY trackChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canControl READ canControl NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasShuffle READ hasShuffle NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasLoop READ hasLoop NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasVolume READ hasVolume NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY shuffleChanged)
    Q_PROPERTY(QString loopStatus READ loopStatus NOTIFY loopStatusChanged)
    Q_PROPERTY(double volume READ volume NOTIFY volumeChanged)

public:
    struct Track {
        QString id;
        QString title;
        QString artist;
        QString album;
        QUrl art;
        qint64 lengthMs = 0;
        bool operator==(const Track &other) const {
            return id == other.id && title == other.title && artist == other.artist && album == other.album
                && art == other.art && lengthMs == other.lengthMs;
        }
    };

    // `bus` may be a disconnected connection, as in the tests: calls then
    // fail quietly and properties only change through apply*Properties().
    MprisPlayer(const QString &service, const QDBusConnection &bus, QObject *parent = nullptr);

    QString service() const { return m_service; }
    QString identity() const { return m_identity; }
    QUrl iconUrl() const { return m_iconUrl; }
    QString status() const { return m_status; }
    bool playing() const { return m_status == QLatin1String("Playing"); }
    QString title() const { return m_track.title; }
    QString artist() const { return m_track.artist; }
    QString album() const { return m_track.album; }
    QUrl artUrl() const { return m_track.art; }
    qint64 length() const { return m_track.lengthMs; }
    qint64 position() const;
    bool canGoNext() const { return m_canGoNext; }
    bool canGoPrevious() const { return m_canGoPrevious; }
    bool canSeek() const { return m_canSeek && m_track.lengthMs > 0; }
    bool canControl() const { return m_canControl; }
    bool hasShuffle() const { return m_hasShuffle; }
    bool hasLoop() const { return m_hasLoop; }
    bool hasVolume() const { return m_hasVolume; }
    bool shuffle() const { return m_shuffle; }
    QString loopStatus() const { return m_loopStatus; }
    double volume() const { return m_volume; }

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seekTo(qint64 ms);
    Q_INVOKABLE void seekBy(qint64 ms);
    Q_INVOKABLE void setVolume(double volume);
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void cycleLoop();
    Q_INVOKABLE void raise();

    // The pieces that turn D-Bus replies into state, public so tests can feed
    // them what a real player would send.
    static Track parseMetadata(const QVariantMap &metadata);
    static QVariant unwrap(const QVariant &value);
    // The first of `names` found as an app icon in the XDG data directories
    // (hicolor, then pixmaps), largest first, or an empty URL.
    static QUrl findIcon(const QStringList &names, const QStringList &dataDirectories);
    void applyPlayerProperties(const QVariantMap &properties);
    void applyRootProperties(const QVariantMap &properties);

signals:
    void identityChanged();
    void statusChanged();
    void trackChanged();
    void positionChanged();
    void capabilitiesChanged();
    void shuffleChanged();
    void loopStatusChanged();
    void volumeChanged();
    void failed(const QString &message);

private slots:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);
    void onSeeked(qlonglong positionUs);

private:
    void refresh();
    void call(const QString &method, const QVariantList &arguments = {});
    void setPlayerProperty(const QString &name, const QVariant &value);
    void setPositionMs(qint64 ms);

    QString m_service;
    QDBusConnection m_bus;
    QString m_identity;
    QUrl m_iconUrl;
    QString m_status = "Stopped";
    Track m_track;
    qint64 m_positionMs = 0;
    QElapsedTimer m_positionClock;
    double m_rate = 1;
    bool m_canGoNext = false, m_canGoPrevious = false, m_canSeek = false, m_canControl = false;
    bool m_hasShuffle = false, m_hasLoop = false, m_hasVolume = false;
    bool m_shuffle = false;
    QString m_loopStatus = "None";
    double m_volume = 1;
    QTimer m_tick;
    QTimer m_resync;
};
