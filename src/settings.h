#pragma once

#include <QObject>
#include <QSettings>

// The look and behavior the user tunes in Preferences, kept in
// ~/.config/tono/tono.conf. Every value is clamped on the way in, so a
// hand-edited file can't push the window into something unreadable.
class Settings : public QObject {
    Q_OBJECT
    // How see-through the window is: 0 is solid, 1 lets the most of the
    // blurred desktop through.
    Q_PROPERTY(double transparency READ transparency WRITE setTransparency NOTIFY changed)
    // How strongly the blurred album art colors the window.
    Q_PROPERTY(double artworkGlow READ artworkGlow WRITE setArtworkGlow NOTIFY changed)
    // Soft, iOS-like corners instead of the theme's window rounding.
    Q_PROPERTY(bool softCorners READ softCorners WRITE setSoftCorners NOTIFY changed)
    // Jump to whichever player starts playing, unless one was picked by hand.
    Q_PROPERTY(bool followPlaying READ followPlaying WRITE setFollowPlaying NOTIFY changed)
    // The slow drift of the artwork behind the glass, like Apple Music.
    Q_PROPERTY(bool motion READ motion WRITE setMotion NOTIFY changed)

public:
    explicit Settings(const QString &path = {}, QObject *parent = nullptr);

    double transparency() const { return m_transparency; }
    double artworkGlow() const { return m_artworkGlow; }
    bool softCorners() const { return m_softCorners; }
    bool followPlaying() const { return m_followPlaying; }
    bool motion() const { return m_motion; }

    void setTransparency(double value);
    void setArtworkGlow(double value);
    void setSoftCorners(bool value);
    void setFollowPlaying(bool value);
    void setMotion(bool value);

    Q_INVOKABLE void reset();

    static constexpr double DefaultTransparency = .45;
    static constexpr double DefaultArtworkGlow = .6;

signals:
    void changed();

private:
    void store(const QString &key, const QVariant &value);

    QSettings m_store;
    double m_transparency = DefaultTransparency;
    double m_artworkGlow = DefaultArtworkGlow;
    bool m_softCorners = false;
    bool m_followPlaying = true;
    bool m_motion = true;
};
