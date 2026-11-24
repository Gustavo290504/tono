#include "mpris.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace {
const QString ObjectPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString RootInterface = QStringLiteral("org.mpris.MediaPlayer2");
const QString PlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

bool isObjectPath(const QString &path) {
    static const QRegularExpression valid(R"(^/([A-Za-z0-9_]+(/[A-Za-z0-9_]+)*)?$)");
    return valid.match(path).hasMatch();
}
}

MprisPlayer::MprisPlayer(const QString &service, const QDBusConnection &bus, QObject *parent)
    : QObject(parent), m_service(service), m_bus(bus) {
    // Until the player says otherwise, call it by its bus name
    // (org.mpris.MediaPlayer2.spotify is "Spotify").
    const QString name = service.section('.', 3, 3);
    m_identity = name.isEmpty() ? service : name.left(1).toUpper() + name.mid(1);
    m_positionClock.start();

    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, &MprisPlayer::positionChanged);

    // Some players skip PropertiesChanged for part of their state, and none
    // report the position as it moves, so ask again every couple of seconds.
    m_resync.setInterval(2000);
    connect(&m_resync, &QTimer::timeout, this, &MprisPlayer::refresh);

    if (m_bus.isConnected()) {
        m_bus.connect(service, ObjectPath, PropertiesInterface, "PropertiesChanged", this,
                      SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
        m_bus.connect(service, ObjectPath, PlayerInterface, "Seeked", this, SLOT(onSeeked(qlonglong)));
        refresh();
        m_resync.start();
    }
}

qint64 MprisPlayer::position() const {
    qint64 ms = m_positionMs;
    if (playing())
        ms += qint64(m_positionClock.elapsed() * m_rate);
    if (m_track.lengthMs > 0)
        ms = qMin(ms, m_track.lengthMs);
    return qMax<qint64>(0, ms);
}

// D-Bus hands nested values over still wrapped (a variant inside a variant,
// or an unparsed argument for maps and arrays), so peel them down to plain
// Qt types before reading them.
QVariant MprisPlayer::unwrap(const QVariant &value) {
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        return unwrap(value.value<QDBusVariant>().variant());
    if (value.metaType() != QMetaType::fromType<QDBusArgument>())
        return value;

    const auto argument = value.value<QDBusArgument>();
    if (argument.currentType() == QDBusArgument::MapType)
        return qdbus_cast<QVariantMap>(argument);
    if (argument.currentSignature() == QLatin1String("as"))
        return qdbus_cast<QStringList>(argument);
    if (argument.currentType() == QDBusArgument::VariantType)
        return unwrap(qdbus_cast<QDBusVariant>(argument).variant());
    return value;
}

MprisPlayer::Track MprisPlayer::parseMetadata(const QVariantMap &metadata) {
    auto get = [&](const char *key) { return unwrap(metadata.value(QLatin1String(key))); };

    Track track;
    const QVariant id = get("mpris:trackid");
    track.id = id.metaType() == QMetaType::fromType<QDBusObjectPath>() ? id.value<QDBusObjectPath>().path()
                                                                       : id.toString();
    track.title = get("xesam:title").toString();
    if (track.title.isEmpty())
        track.title = QUrl(get("xesam:url").toString()).fileName();
    track.artist = get("xesam:artist").toStringList().join(", ");
    track.album = get("xesam:album").toString();

    // Older Spotify builds point artwork at a host that no longer serves it.
    QString art = get("mpris:artUrl").toString();
    art.replace("https://open.spotify.com/image/", "https://i.scdn.co/image/");
    track.art = QUrl(art);

    // MPRIS lengths are microseconds, signed or unsigned depending on the player.
    track.lengthMs = qMax<qint64>(0, get("mpris:length").toLongLong() / 1000);
    return track;
}

void MprisPlayer::applyPlayerProperties(const QVariantMap &properties) {
    bool capabilities = false;
    auto flag = [&](const char *key, bool &target) {
        const QString name = QLatin1String(key);
        if (!properties.contains(name))
            return;
        const bool value = unwrap(properties.value(name)).toBool();
        if (value != target) {
            target = value;
            capabilities = true;
        }
    };

    if (properties.contains("Rate")) {
        const double rate = unwrap(properties.value("Rate")).toDouble();
        if (rate > 0) {
            m_positionMs = position();
            m_positionClock.restart();
            m_rate = rate;
        }
    }

    if (properties.contains("PlaybackStatus")) {
        const QString status = unwrap(properties.value("PlaybackStatus")).toString();
        if (!status.isEmpty() && status != m_status) {
            // Fold the time played so far into the stored position, so a
            // pause freezes the clock where the music stopped.
            m_positionMs = position();
            m_positionClock.restart();
            m_status = status;
            if (playing())
                m_tick.start();
            else
                m_tick.stop();
            emit statusChanged();
            emit positionChanged();
        }
    }

    if (properties.contains("Metadata")) {
        const Track track = parseMetadata(unwrap(properties.value("Metadata")).toMap());
        if (!(track == m_track)) {
            const bool newTrack = track.id != m_track.id || track.title != m_track.title;
            const bool lengthChanged = track.lengthMs != m_track.lengthMs;
            m_track = track;
            if (newTrack && !properties.contains("Position"))
                setPositionMs(0);
            emit trackChanged();
            capabilities = capabilities || lengthChanged;
        }
    }

    if (properties.contains("Position"))
        setPositionMs(unwrap(properties.value("Position")).toLongLong() / 1000);

    flag("CanGoNext", m_canGoNext);
    flag("CanGoPrevious", m_canGoPrevious);
    flag("CanSeek", m_canSeek);
    flag("CanControl", m_canControl);

    if (properties.contains("Shuffle")) {
        capabilities = capabilities || !m_hasShuffle;
        m_hasShuffle = true;
        const bool shuffle = unwrap(properties.value("Shuffle")).toBool();
        if (shuffle != m_shuffle) {
            m_shuffle = shuffle;
            emit shuffleChanged();
        }
    }

    if (properties.contains("LoopStatus")) {
        capabilities = capabilities || !m_hasLoop;
        m_hasLoop = true;
        const QString loop = unwrap(properties.value("LoopStatus")).toString();
        if (!loop.isEmpty() && loop != m_loopStatus) {
            m_loopStatus = loop;
            emit loopStatusChanged();
        }
    }

    if (properties.contains("Volume")) {
        capabilities = capabilities || !m_hasVolume;
        m_hasVolume = true;
        const double volume = qBound(0.0, unwrap(properties.value("Volume")).toDouble(), 1.0);
        if (!qFuzzyCompare(volume + 1, m_volume + 1)) {
            m_volume = volume;
            emit volumeChanged();
        }
    }

    if (capabilities)
        emit capabilitiesChanged();
}

void MprisPlayer::applyRootProperties(const QVariantMap &properties) {
    const QString identity = unwrap(properties.value("Identity")).toString();
    const QString desktopEntry = unwrap(properties.value("DesktopEntry")).toString();

    // The icon is the player's own, found the way the launcher finds it:
    // through its desktop entry, then its name.
    QStringList candidates;
    if (!desktopEntry.isEmpty())
        candidates << desktopEntry << desktopEntry + "-client" << desktopEntry.section('.', -1).toLower();
    candidates << identity.toLower() << m_service.section('.', 3, 3).toLower();
    candidates.removeAll(QString());
    candidates.removeDuplicates();

    QStringList dataDirectories{qEnvironmentVariableIsEmpty("XDG_DATA_HOME")
                                    ? QDir::homePath() + "/.local/share" : qEnvironmentVariable("XDG_DATA_HOME")};
    dataDirectories += qEnvironmentVariable("XDG_DATA_DIRS", "/usr/local/share:/usr/share").split(':', Qt::SkipEmptyParts);
    const QUrl icon = findIcon(candidates, dataDirectories);

    const QString name = identity.isEmpty() ? m_identity : identity;
    if (name != m_identity || icon != m_iconUrl) {
        m_identity = name;
        m_iconUrl = icon;
        emit identityChanged();
    }
}

QUrl MprisPlayer::findIcon(const QStringList &names, const QStringList &dataDirectories) {
    static const QStringList sizes{"scalable", "512x512", "256x256", "128x128", "96x96", "64x64", "48x48", "32x32"};
    for (const auto &name : names) {
        for (const auto &directory : dataDirectories) {
            for (const auto &size : sizes) {
                for (const char *extension : {".svg", ".png"}) {
                    const QString path = directory + "/icons/hicolor/" + size + "/apps/" + name + extension;
                    if (QFileInfo::exists(path))
                        return QUrl::fromLocalFile(path);
                }
            }
            for (const char *extension : {".svg", ".png"}) {
                const QString path = directory + "/pixmaps/" + name + extension;
                if (QFileInfo::exists(path))
                    return QUrl::fromLocalFile(path);
            }
        }
    }
    return {};
}

void MprisPlayer::onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                                      const QStringList &invalidated) {
    if (interface == PlayerInterface)
        applyPlayerProperties(changed);
    else if (interface == RootInterface)
        applyRootProperties(changed);
    if (!invalidated.isEmpty())
        refresh();
}

void MprisPlayer::onSeeked(qlonglong positionUs) {
    setPositionMs(positionUs / 1000);
}

void MprisPlayer::setPositionMs(qint64 ms) {
    m_positionMs = qMax<qint64>(0, ms);
    m_positionClock.restart();
    emit positionChanged();
}

void MprisPlayer::refresh() {
    if (!m_bus.isConnected())
        return;
    for (const QString &interface : {RootInterface, PlayerInterface}) {
        auto message = QDBusMessage::createMethodCall(m_service, ObjectPath, PropertiesInterface, "GetAll");
        message << interface;
        auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 1500), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, interface](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            const QDBusPendingReply<QVariantMap> reply = *w;
            // A player that just quit fails here; Players drops it when its
            // bus name goes away.
            if (reply.isError())
                return;
            if (interface == RootInterface)
                applyRootProperties(reply.value());
            else
                applyPlayerProperties(reply.value());
        });
    }
}

void MprisPlayer::call(const QString &method, const QVariantList &arguments) {
    if (!m_bus.isConnected())
        return;
    auto message = QDBusMessage::createMethodCall(m_service, ObjectPath,
                                                  method == "Raise" ? RootInterface : PlayerInterface, method);
    message.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            emit failed(tr("%1 didn't take that: %2").arg(m_identity, w->error().message()));
            refresh();
        }
    });
}

void MprisPlayer::setPlayerProperty(const QString &name, const QVariant &value) {
    if (!m_bus.isConnected())
        return;
    auto message = QDBusMessage::createMethodCall(m_service, ObjectPath, PropertiesInterface, "Set");
    message.setArguments({PlayerInterface, name, QVariant::fromValue(QDBusVariant(value))});
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        // The change was shown right away, so put the real state back.
        if (w->isError()) {
            emit failed(tr("%1 didn't take that: %2").arg(m_identity, w->error().message()));
            refresh();
        }
    });
}

void MprisPlayer::playPause() { call("PlayPause"); }
void MprisPlayer::next() { call("Next"); }
void MprisPlayer::previous() { call("Previous"); }
void MprisPlayer::raise() { call("Raise"); }

void MprisPlayer::seekTo(qint64 ms) {
    if (!canSeek())
        return;
    ms = qBound<qint64>(0, ms, m_track.lengthMs);
    // SetPosition is exact but needs the track's object path; players that
    // don't give one only take a relative Seek.
    if (isObjectPath(m_track.id))
        call("SetPosition", {QVariant::fromValue(QDBusObjectPath(m_track.id)), qlonglong(ms * 1000)});
    else
        call("Seek", {qlonglong((ms - position()) * 1000)});
    setPositionMs(ms);
}

void MprisPlayer::seekBy(qint64 ms) {
    seekTo(position() + ms);
}

void MprisPlayer::setVolume(double volume) {
    if (!m_hasVolume)
        return;
    volume = qBound(0.0, volume, 1.0);
    setPlayerProperty("Volume", volume);
    if (!qFuzzyCompare(volume + 1, m_volume + 1)) {
        m_volume = volume;
        emit volumeChanged();
    }
}

void MprisPlayer::toggleShuffle() {
    if (!m_hasShuffle)
        return;
    m_shuffle = !m_shuffle;
    setPlayerProperty("Shuffle", m_shuffle);
    emit shuffleChanged();
}

void MprisPlayer::cycleLoop() {
    if (!m_hasLoop)
        return;
    m_loopStatus = m_loopStatus == "None" ? "Playlist" : m_loopStatus == "Playlist" ? "Track" : "None";
    setPlayerProperty("LoopStatus", m_loopStatus);
    emit loopStatusChanged();
}
