#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "mpris.h"
#include "players.h"
#include "settings.h"
#include "theme.h"
#include "translator.h"

class TonoTests : public QObject {
    Q_OBJECT

private slots:
    // Theme

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QVERIFY(theme.dark());
    }

    void themeReadsThePalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "accent = \"#112233\"\nforeground = \"#1c1c1e\"\nbackground = \"#F2F2F7\"\n"
              "muted = \"#6c6c70\"\nmode = \"light\"\nselection_background = \"#000000\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
        QCOMPARE(theme.background(), QString("#F2F2F7"));
        QCOMPARE(theme.foreground(), QString("#1c1c1e"));
        QCOMPARE(theme.muted(), QString("#6c6c70"));
        QVERIFY(!theme.dark());
    }

    void themeGuessesModeFromTheBackground()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "background = \"#FAFAFA\"\n");
        Theme theme(dir.path());
        QVERIFY(!theme.dark());
        QCOMPARE(theme.foreground(), QString("#1C1C1E"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    // Translator

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "es-MX\n");
        write(dir.path() + "/es.json", R"({"Play": "Reproducir", "Pause": "Pausar"})");
        write(dir.path() + "/es-MX.json", R"({"Play": "Tocar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("es-MX"));
        QCOMPARE(translator.translate("Main", "Play"), QString("Tocar"));
        QCOMPARE(translator.translate("Main", "Pause"), QString("Pausar"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QCOMPARE(Translator::pluralCategory("ru", 22), QString("few"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
        QCOMPARE(Translator::pluralCategory("es-MX", 1), QString("one"));
    }

    void spanishCatalogIsValid()
    {
        Translator translator(QFINDTESTDATA("../src/i18n/es.json").isEmpty() ? QString() : writeLanguage("es"),
                              QFileInfo(QFINDTESTDATA("../src/i18n/es.json")).absolutePath());
        QCOMPARE(translator.translate("Main", "Nothing is playing"), QString("No suena nada"));
    }

    // Settings

    void settingsStartWithDefaults()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath("tono.conf"));
        QCOMPARE(settings.transparency(), Settings::DefaultTransparency);
        QCOMPARE(settings.artworkGlow(), Settings::DefaultArtworkGlow);
        QVERIFY(!settings.softCorners());
        QVERIFY(settings.followPlaying());
        QVERIFY(settings.motion());
    }

    void settingsClampAndPersist()
    {
        QTemporaryDir dir;
        {
            Settings settings(dir.filePath("tono.conf"));
            QSignalSpy changed(&settings, &Settings::changed);
            settings.setTransparency(7);
            settings.setArtworkGlow(-1);
            settings.setSoftCorners(true);
            settings.setSoftCorners(true);
            QCOMPARE(settings.transparency(), 1.0);
            QCOMPARE(settings.artworkGlow(), 0.0);
            QCOMPARE(changed.count(), 3);
        }
        Settings reopened(dir.filePath("tono.conf"));
        QCOMPARE(reopened.transparency(), 1.0);
        QCOMPARE(reopened.artworkGlow(), 0.0);
        QVERIFY(reopened.softCorners());

        reopened.reset();
        QCOMPARE(reopened.transparency(), Settings::DefaultTransparency);
        QVERIFY(!Settings(dir.filePath("tono.conf")).softCorners());
    }

    void settingsClampAHandEditedFile()
    {
        QTemporaryDir dir;
        write(dir.filePath("tono.conf"), "[look]\ntransparency=3.5\n");
        QCOMPARE(Settings(dir.filePath("tono.conf")).transparency(), 1.0);
    }

    // MPRIS

    void metadataParsesSpotify()
    {
        const QVariantMap metadata{
            {"mpris:trackid", "/com/spotify/track/abc123"},
            {"mpris:length", qulonglong(265232000)},
            {"mpris:artUrl", "https://open.spotify.com/image/ab67616d"},
            {"xesam:album", "Low Tide Radio"},
            {"xesam:artist", QStringList{"Lumen Coast", "Harbor Lights"}},
            {"xesam:title", "Golden Hour Drive"},
        };
        const auto track = MprisPlayer::parseMetadata(metadata);
        QCOMPARE(track.id, QString("/com/spotify/track/abc123"));
        QCOMPARE(track.title, QString("Golden Hour Drive"));
        QCOMPARE(track.artist, QString("Lumen Coast, Harbor Lights"));
        QCOMPARE(track.album, QString("Low Tide Radio"));
        QCOMPARE(track.lengthMs, qint64(265232));
        QCOMPARE(track.art, QUrl("https://i.scdn.co/image/ab67616d"));
    }

    void metadataFallsBackToTheFileName()
    {
        const auto track = MprisPlayer::parseMetadata({
            {"mpris:trackid", QVariant::fromValue(QDBusObjectPath("/org/mpv/Track/1"))},
            {"mpris:length", qlonglong(1500)},
            {"xesam:url", "file:///home/someone/Music/Night%20Swim.flac"},
        });
        QCOMPARE(track.id, QString("/org/mpv/Track/1"));
        QCOMPARE(track.title, QString("Night Swim.flac"));
        QCOMPARE(track.lengthMs, qint64(1));
        QVERIFY(track.artist.isEmpty());
    }

    void playerFollowsItsProperties()
    {
        MprisPlayer player("org.mpris.MediaPlayer2.spotify", QDBusConnection("tono-tests-none"));
        QCOMPARE(player.identity(), QString("Spotify"));
        QVERIFY(!player.hasShuffle());

        QSignalSpy track(&player, &MprisPlayer::trackChanged);
        player.applyPlayerProperties({
            {"PlaybackStatus", "Paused"},
            {"Metadata", QVariantMap{{"mpris:trackid", "/t/1"}, {"xesam:title", "One"}, {"mpris:length", 200000000}}},
            {"Position", qlonglong(30000000)},
            {"CanSeek", true},
            {"Shuffle", true},
            {"LoopStatus", "Playlist"},
            {"Volume", 1.4},
        });
        QCOMPARE(track.count(), 1);
        QCOMPARE(player.title(), QString("One"));
        QCOMPARE(player.position(), qint64(30000));
        QVERIFY(player.canSeek());
        QVERIFY(player.hasShuffle() && player.shuffle());
        QVERIFY(player.hasLoop());
        QCOMPARE(player.loopStatus(), QString("Playlist"));
        QCOMPARE(player.volume(), 1.0);
        QVERIFY(!player.playing());

        // A paused player's clock doesn't run.
        QTest::qWait(60);
        QCOMPARE(player.position(), qint64(30000));

        // A playing one's does.
        player.applyPlayerProperties({{"PlaybackStatus", "Playing"}});
        QTRY_VERIFY(player.position() >= 30050);

        // A new track starts from the top unless the player says otherwise.
        player.applyPlayerProperties({{"Metadata", QVariantMap{{"mpris:trackid", "/t/2"}, {"xesam:title", "Two"}}}});
        QVERIFY(player.position() < 1000);
        QCOMPARE(track.count(), 2);
    }

    void playerFindsItsIcon()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("icons/hicolor/128x128/apps");
        QDir(dir.path()).mkpath("pixmaps");
        write(dir.path() + "/icons/hicolor/128x128/apps/spotify.png", "png");
        write(dir.path() + "/pixmaps/mpv.svg", "svg");
        QCOMPARE(MprisPlayer::findIcon({"spotify-launcher", "spotify"}, {dir.path()}),
                 QUrl::fromLocalFile(dir.path() + "/icons/hicolor/128x128/apps/spotify.png"));
        QCOMPARE(MprisPlayer::findIcon({"mpv"}, {"/nonexistent", dir.path()}),
                 QUrl::fromLocalFile(dir.path() + "/pixmaps/mpv.svg"));
        QVERIFY(MprisPlayer::findIcon({"nothing"}, {dir.path()}).isEmpty());
    }

    // Players

    void choosePrefersWhatsPlaying()
    {
        using C = Players::Candidate;
        const QList<C> some{{"org.mpris.MediaPlayer2.chromium", false}, {"org.mpris.MediaPlayer2.spotify", false},
                            {"org.mpris.MediaPlayer2.mpv", true}};
        QCOMPARE(Players::choose({}, {}, {}, true), -1);
        QCOMPARE(Players::choose(some, {}, {}, true), 2);
        // Spotify when nothing plays.
        QCOMPARE(Players::choose({some[0], some[1]}, {}, {}, true), 1);
        // A playing current player stays, even when another plays too.
        QCOMPARE(Players::choose({{"a", true}, {"b", true}}, {}, "b", true), 1);
        // A paused current player gives way when following...
        QCOMPARE(Players::choose(some, {}, "org.mpris.MediaPlayer2.chromium", true), 2);
        // ...and stays when not.
        QCOMPARE(Players::choose(some, {}, "org.mpris.MediaPlayer2.chromium", false), 0);
        // A pick by hand beats everything.
        QCOMPARE(Players::choose(some, "org.mpris.MediaPlayer2.spotify", {}, true), 1);
    }

    void playersSkipPlayerctld()
    {
        QVERIFY(Players::isPlayerService("org.mpris.MediaPlayer2.spotify"));
        QVERIFY(Players::isPlayerService("org.mpris.MediaPlayer2.chromium.instance42"));
        QVERIFY(!Players::isPlayerService("org.mpris.MediaPlayer2.playerctld"));
        QVERIFY(!Players::isPlayerService("org.freedesktop.Notifications"));
    }

    void playersComeAndGo()
    {
        Players players{QDBusConnection("tono-tests-none")};
        QVERIFY(!players.busAvailable());
        QSignalSpy current(&players, &Players::currentChanged);

        players.addService("org.mpris.MediaPlayer2.spotify");
        players.addService("org.mpris.MediaPlayer2.mpv");
        players.addService("org.mpris.MediaPlayer2.mpv");
        QCOMPARE(players.players().size(), 2);
        QCOMPARE(players.current()->service(), QString("org.mpris.MediaPlayer2.spotify"));

        // mpv starts playing and is followed.
        auto *mpv = qobject_cast<MprisPlayer *>(players.players().at(1));
        mpv->applyPlayerProperties({{"PlaybackStatus", "Playing"}});
        QCOMPARE(players.current(), mpv);

        // Picking Spotify by hand sticks.
        players.select(0);
        QCOMPARE(players.currentIndex(), 0);

        // Spotify quits, and the shown player moves on; the last one leaving
        // still says so.
        players.removeService("org.mpris.MediaPlayer2.spotify");
        QCOMPARE(players.current(), mpv);
        current.clear();
        players.removeService("org.mpris.MediaPlayer2.mpv");
        QCOMPARE(players.current(), nullptr);
        QCOMPARE(current.count(), 1);
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    QString writeLanguage(const QString &code)
    {
        const QString path = m_dir.filePath("language");
        write(path, code.toUtf8());
        return path;
    }

    QTemporaryDir m_dir;
};

QTEST_MAIN(TonoTests)
#include "tono_tests.moc"
