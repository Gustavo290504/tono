// tono — a glassy now-playing window for Spotify and any MPRIS music player.

#include <QGuiApplication>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QUrl>

#include "players.h"
#include "settings.h"
#include "theme.h"
#include "translator.h"

namespace {
// Album art comes from the web (Spotify serves it from its CDN), so keep it
// on disk: skipping back to a song shows its cover at once, even offline.
class ArtworkCacheFactory : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager *create(QObject *parent) override {
        auto *manager = new QNetworkAccessManager(parent);
        auto *cache = new QNetworkDiskCache(manager);
        cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/artwork");
        cache->setMaximumCacheSize(64 * 1024 * 1024);
        manager->setCache(cache);
        return manager;
    }
};
}

int main(int argc, char *argv[]) {
    // The window is see-through so Hyprland can blur the desktop behind it;
    // that needs an alpha channel before any window exists.
    QQuickWindow::setDefaultAlphaBuffer(true);

    QGuiApplication app(argc, argv);
    app.setApplicationName("tono");
    app.setApplicationVersion("0.1.0");

    // Matches the window to tono.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("tono");
    app.setWindowIcon(QIcon::fromTheme("tono"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Settings settings;
    Players players;
    players.setFollowPlaying(settings.followPlaying());
    QObject::connect(&settings, &Settings::changed, &players,
                     [&] { players.setFollowPlaying(settings.followPlaying()); });

    ArtworkCacheFactory artworkCache;
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&artworkCache);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("players", &players);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
