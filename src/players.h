#pragma once

#include <QDBusConnection>
#include <QList>
#include <QObject>

#include "mpris.h"

// Every MPRIS player on the session bus, and the one Tono shows. Players
// come and go as apps open and quit; the shown one follows whatever starts
// playing unless the user picked one by hand.
class Players : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QObject *> players READ players NOTIFY playersChanged)
    Q_PROPERTY(MprisPlayer *current READ current NOTIFY currentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(bool busAvailable READ busAvailable CONSTANT)
    Q_PROPERTY(bool spotifyInstalled READ spotifyInstalled CONSTANT)
    Q_PROPERTY(bool followPlaying READ followPlaying WRITE setFollowPlaying NOTIFY followPlayingChanged)

public:
    explicit Players(QObject *parent = nullptr);
    // A disconnected `bus` starts empty and only changes through
    // addService() and removeService(), for the tests.
    Players(const QDBusConnection &bus, QObject *parent = nullptr);

    QList<QObject *> players() const;
    MprisPlayer *current() const { return m_current; }
    int currentIndex() const { return int(m_players.indexOf(m_current)); }
    bool busAvailable() const { return m_bus.isConnected(); }
    bool spotifyInstalled() const;
    bool followPlaying() const { return m_followPlaying; }
    void setFollowPlaying(bool follow);

    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void launchSpotify();

    void addService(const QString &service);
    void removeService(const QString &service);

    struct Candidate {
        QString service;
        bool playing = false;
    };
    // Which player to show: the one picked by hand, else (when following)
    // the current one while it plays or nothing else does, else the first
    // that plays, else the current one, else Spotify, else the first.
    static int choose(const QList<Candidate> &candidates, const QString &pinned, const QString &current,
                      bool followPlaying);
    static bool isPlayerService(const QString &service);

signals:
    void playersChanged();
    void currentChanged();
    void followPlayingChanged();
    void failed(const QString &message);

private slots:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void reconsider();

    QDBusConnection m_bus;
    QList<MprisPlayer *> m_players;
    MprisPlayer *m_current = nullptr;
    QString m_pinned;
    bool m_followPlaying = true;
};
