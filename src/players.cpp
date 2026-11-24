#include "players.h"

#include <QDBusConnectionInterface>
#include <QProcess>
#include <QStandardPaths>

Players::Players(QObject *parent) : Players(QDBusConnection::sessionBus(), parent) {}

Players::Players(const QDBusConnection &bus, QObject *parent) : QObject(parent), m_bus(bus) {
    if (!m_bus.isConnected())
        return;
    m_bus.connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged", this,
                  SLOT(onNameOwnerChanged(QString, QString, QString)));
    const QStringList names = m_bus.interface()->registeredServiceNames().value();
    for (const auto &name : names)
        if (isPlayerService(name))
            addService(name);
}

// playerctld re-publishes whichever player is active, which would show the
// same music twice.
bool Players::isPlayerService(const QString &service) {
    return service.startsWith(QLatin1String("org.mpris.MediaPlayer2."))
        && service != QLatin1String("org.mpris.MediaPlayer2.playerctld");
}

QList<QObject *> Players::players() const {
    QList<QObject *> list;
    for (auto *player : m_players)
        list << player;
    return list;
}

bool Players::spotifyInstalled() const {
    return !QStandardPaths::findExecutable("spotify").isEmpty();
}

void Players::setFollowPlaying(bool follow) {
    if (follow == m_followPlaying)
        return;
    m_followPlaying = follow;
    emit followPlayingChanged();
    reconsider();
}

void Players::onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner) {
    if (!isPlayerService(name))
        return;
    if (!oldOwner.isEmpty())
        removeService(name);
    if (!newOwner.isEmpty())
        addService(name);
}

void Players::addService(const QString &service) {
    for (auto *player : std::as_const(m_players))
        if (player->service() == service)
            return;
    auto *player = new MprisPlayer(service, m_bus, this);
    connect(player, &MprisPlayer::statusChanged, this, &Players::reconsider);
    connect(player, &MprisPlayer::failed, this, &Players::failed);
    m_players << player;
    emit playersChanged();
    reconsider();
}

void Players::removeService(const QString &service) {
    for (int i = 0; i < m_players.size(); ++i) {
        MprisPlayer *player = m_players.at(i);
        if (player->service() != service)
            continue;
        m_players.removeAt(i);
        if (m_pinned == service)
            m_pinned.clear();
        emit playersChanged();
        // m_current may still point at the removed player here, which makes
        // reconsider() see a change and announce the new one (or none). It is
        // deleted later because QML still holds it until it rebinds.
        reconsider();
        player->deleteLater();
        return;
    }
}

int Players::choose(const QList<Candidate> &candidates, const QString &pinned, const QString &current,
                    bool followPlaying) {
    auto find = [&](auto predicate) {
        for (int i = 0; i < candidates.size(); ++i)
            if (predicate(candidates.at(i)))
                return i;
        return -1;
    };
    if (candidates.isEmpty())
        return -1;

    const int picked = pinned.isEmpty() ? -1 : find([&](const Candidate &c) { return c.service == pinned; });
    if (picked >= 0)
        return picked;

    const int now = current.isEmpty() ? -1 : find([&](const Candidate &c) { return c.service == current; });
    const int playing = find([](const Candidate &c) { return c.playing; });
    if (followPlaying && playing >= 0 && (now < 0 || !candidates.at(now).playing))
        return playing;
    if (now >= 0)
        return now;
    if (playing >= 0)
        return playing;

    const int spotify = find([](const Candidate &c) { return c.service.contains("spotify", Qt::CaseInsensitive); });
    return spotify >= 0 ? spotify : 0;
}

void Players::reconsider() {
    QList<Candidate> candidates;
    for (auto *player : std::as_const(m_players))
        candidates << Candidate{player->service(), player->playing()};
    const int index = choose(candidates, m_pinned, m_current ? m_current->service() : QString(), m_followPlaying);
    MprisPlayer *chosen = index >= 0 ? m_players.at(index) : nullptr;
    if (chosen != m_current) {
        m_current = chosen;
        emit currentChanged();
    }
}

void Players::select(int index) {
    if (index < 0 || index >= m_players.size())
        return;
    m_pinned = m_players.at(index)->service();
    reconsider();
}

void Players::selectNext() {
    if (m_players.size() < 2)
        return;
    select((currentIndex() + 1) % int(m_players.size()));
}

void Players::launchSpotify() {
    // Omarchy's launcher focuses Spotify when it's already open and starts it
    // through uwsm otherwise, the way the desktop launches its own apps.
    const QString omarchy = QStandardPaths::findExecutable("omarchy-launch-or-focus");
    const bool started = !omarchy.isEmpty() ? QProcess::startDetached(omarchy, {"spotify"})
                                            : QProcess::startDetached("spotify", {});
    if (!started)
        emit failed(tr("Couldn't start Spotify."));
}
