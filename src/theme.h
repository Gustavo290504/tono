#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

// Follows the current Omarchy theme live: its accent, the background and
// foreground the glass is tinted with, the muted color for secondary text,
// whether it is a light or dark theme, and Hyprland's window rounding.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    // The theme's red, for errors.
    Q_PROPERTY(QString danger READ danger NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QString background() const { return m_background; }
    QString foreground() const { return m_foreground; }
    QString muted() const { return m_muted; }
    QString danger() const { return m_danger; }
    bool dark() const { return m_dark; }
    int radius() const { return m_radius; }

    static QString readAccent(const QString &path);
    // One `key = "#rrggbb"` line from colors.toml, or the fallback.
    static QString readColor(const QString &contents, const QString &key, const QString &fallback);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    QString m_background = "#0E0E10";
    QString m_foreground = "#F5F5F7";
    QString m_muted = "#8E8E93";
    QString m_danger = "#FF453A";
    bool m_dark = true;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
