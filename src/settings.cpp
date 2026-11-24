#include "settings.h"

#include <QDir>
#include <QtGlobal>

static QString defaultPath() {
    const QString config = qEnvironmentVariableIsEmpty("XDG_CONFIG_HOME") ? QDir::homePath() + "/.config"
                                                                          : qEnvironmentVariable("XDG_CONFIG_HOME");
    return config + "/tono/tono.conf";
}

Settings::Settings(const QString &path, QObject *parent)
    : QObject(parent), m_store(path.isEmpty() ? defaultPath() : path, QSettings::IniFormat) {
    m_transparency = qBound(0.0, m_store.value("look/transparency", m_transparency).toDouble(), 1.0);
    m_artworkGlow = qBound(0.0, m_store.value("look/artworkGlow", m_artworkGlow).toDouble(), 1.0);
    m_softCorners = m_store.value("look/softCorners", m_softCorners).toBool();
    m_motion = m_store.value("look/motion", m_motion).toBool();
    m_followPlaying = m_store.value("players/followPlaying", m_followPlaying).toBool();
}

// QSettings writes through a temporary file and a rename, so a crash never
// leaves a half-written config behind.
void Settings::store(const QString &key, const QVariant &value) {
    m_store.setValue(key, value);
    m_store.sync();
    emit changed();
}

void Settings::setTransparency(double value) {
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(value + 1, m_transparency + 1))
        return;
    m_transparency = value;
    store("look/transparency", value);
}

void Settings::setArtworkGlow(double value) {
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(value + 1, m_artworkGlow + 1))
        return;
    m_artworkGlow = value;
    store("look/artworkGlow", value);
}

void Settings::setSoftCorners(bool value) {
    if (value == m_softCorners)
        return;
    m_softCorners = value;
    store("look/softCorners", value);
}

void Settings::setFollowPlaying(bool value) {
    if (value == m_followPlaying)
        return;
    m_followPlaying = value;
    store("players/followPlaying", value);
}

void Settings::setMotion(bool value) {
    if (value == m_motion)
        return;
    m_motion = value;
    store("look/motion", value);
}

void Settings::reset() {
    m_store.clear();
    m_store.sync();
    m_transparency = DefaultTransparency;
    m_artworkGlow = DefaultArtworkGlow;
    m_softCorners = false;
    m_followPlaying = true;
    m_motion = true;
    emit changed();
}
