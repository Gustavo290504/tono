#include <QDBusObjectPath>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "lyrics.h"
#include "mpris.h"
#include "players.h"
#include "settings.h"
#include "spotify.h"
#include "theme.h"
#include "translator.h"


// A stand-in for accounts.spotify.com and api.spotify.com: hands out tokens
// and a profile, and remembers what the token endpoint was sent.
class FakeSpotify : public QTcpServer {
public:
    FakeSpotify() {
        listen(QHostAddress::LocalHost, 0);
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", buffer);
                    const qsizetype end = buffer.indexOf("\r\n\r\n");
                    if (end < 0)
                        return;
                    static const QRegularExpression length("Content-Length: (\\d+)", QRegularExpression::CaseInsensitiveOption);
                    const auto match = length.match(QString::fromLatin1(buffer.left(end)));
                    const QByteArray body = buffer.mid(end + 4);
                    if (match.hasMatch() && body.size() < match.captured(1).toInt())
                        return;
                    const QByteArray line = buffer.left(buffer.indexOf("\r\n"));
                    QByteArray json = "{}";
                    if (line.contains("/api/token")) {
                        tokenRequests << body;
                        json = R"({"access_token": "access-1", "expires_in": 3600, "refresh_token": "refresh-1"})";
                    } else if (line.contains("/v1/me")) {
                        authorization = buffer.contains("Authorization: Bearer access-1");
                        json = R"({"display_name": "Demo Listener", "id": "demo"})";
                    }
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                                  + QByteArray::number(json.size()) + "\r\n\r\n" + json);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    QUrl url() const { return QUrl(QString("http://127.0.0.1:%1").arg(serverPort())); }
    QList<QByteArray> tokenRequests;
    bool authorization = false;
};

// Plays the browser: follows the authorize URL straight back to Tono's
// redirect URI, as if the user had approved.
class FakeBrowser : public QObject {
    Q_OBJECT
public:
    QUrl opened;
    bool follow = true;
public slots:
    void open(const QUrl &url) {
        opened = url;
        if (!follow)
            return;
        const QUrlQuery query(url);
        QUrl back(query.queryItemValue("redirect_uri", QUrl::FullyDecoded));
        back.setQuery("code=c0de&state=" + query.queryItemValue("state"));
        auto *reply = m_network.get(QNetworkRequest(back));
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            page = reply->readAll();
            reply->deleteLater();
        });
    }
public:
    QByteArray page;
private:
    QNetworkAccessManager m_network;
};

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


    // Lyrics

    void lrcParsesStampsAndRepeats()
    {
        const auto lines = Lyrics::parseLrc("[ar:Someone]\n[00:31.48] First line\n[00:35.4]Second\n"
                                            "[01:02.003][00:40.00] Chorus\n[00:50.00]\nnot a lyric\n");
        QCOMPARE(lines.size(), 5);
        QCOMPARE(lines.at(0).ms, qint64(31480));
        QCOMPARE(lines.at(0).text, QString("First line"));
        QCOMPARE(lines.at(1).ms, qint64(35400));
        QCOMPARE(lines.at(2).ms, qint64(40000));
        QCOMPARE(lines.at(2).text, QString("Chorus"));
        QCOMPARE(lines.at(3).text, QString());
        QCOMPARE(lines.at(4).ms, qint64(62003));
        QCOMPARE(lines.at(4).text, QString("Chorus"));
    }

    void lyricsFindTheLineBeingSung()
    {
        const auto lines = Lyrics::parseLrc("[00:10.00]a\n[00:20.00]b\n[00:30.00]c\n");
        QCOMPARE(Lyrics::indexAt(lines, 0), -1);
        QCOMPARE(Lyrics::indexAt(lines, 10000), 0);
        QCOMPARE(Lyrics::indexAt(lines, 29999), 1);
        QCOMPARE(Lyrics::indexAt(lines, 999999), 2);
        QCOMPARE(Lyrics::indexAt(Lyrics::parsePlain("a\nb"), 15000), -1);
    }

    void lyricsReadRecords()
    {
        auto result = Lyrics::parseRecord({{"syncedLyrics", "[00:01.00]hi"}, {"plainLyrics", "hi"}});
        QCOMPARE(result.status, QString("found"));
        QVERIFY(result.synced);

        result = Lyrics::parseRecord({{"syncedLyrics", QJsonValue()}, {"plainLyrics", "one\ntwo\n\n"}});
        QVERIFY(!result.synced);
        QCOMPARE(result.lines.size(), 2);

        QCOMPARE(Lyrics::parseRecord({{"instrumental", true}}).status, QString("instrumental"));
        QCOMPARE(Lyrics::parseRecord({}).status, QString("missing"));
    }

    void lyricsPickTheRightCut()
    {
        const QJsonArray records{
            QJsonObject{{"duration", 248.0}, {"syncedLyrics", "[00:01.00]radio edit"}},
            QJsonObject{{"duration", 369.0}, {"plainLyrics", "album words"}},
            QJsonObject{{"duration", 368.0}, {"syncedLyrics", "[00:02.00]album synced"}},
        };
        auto result = Lyrics::pickSearchResult(records, 369000);
        QVERIFY(result.synced);
        QCOMPARE(result.lines.first().text, QString("album synced"));

        // Only another cut has words: they show, but unsynced.
        result = Lyrics::pickSearchResult({records.at(0)}, 369000);
        QCOMPARE(result.status, QString("found"));
        QVERIFY(!result.synced);
        QCOMPARE(result.lines.first().ms, qint64(-1));

        QCOMPARE(Lyrics::pickSearchResult({}, 1000).status, QString("missing"));
    }

    void lyricsSimplifyTitles()
    {
        QCOMPARE(Lyrics::simplifyTitle("Let It Be - Remastered 2009"), QString("Let It Be"));
        QCOMPARE(Lyrics::simplifyTitle("Song (feat. Someone)"), QString("Song"));
        QCOMPARE(Lyrics::simplifyTitle("Love (Is All)"), QString("Love (Is All)"));
    }

    // Spotify

    void spotifyBuildsThePkceSignIn()
    {
        QCOMPARE(Spotify::codeChallenge("tono-test-verifier-0123456789-abcdefghijklmnopqrstuvwxyz._~ABCDEF"),
                 QString("kNVjncBvmsy2bUc45k-aKYOgUC0CPwB60a-0HmCZC3k"));
        QVERIFY(Spotify::isClientId("0123456789abcdef0123456789ABCDEF"));
        QVERIFY(!Spotify::isClientId("not-a-client-id"));

        const QUrl url = Spotify::authorizeUrl(QUrl("https://accounts.spotify.com"), "abc", "challenge", "xyz");
        const QUrlQuery query(url);
        QCOMPARE(url.path(), QString("/authorize"));
        QCOMPARE(query.queryItemValue("redirect_uri", QUrl::FullyDecoded), QString("http://127.0.0.1:8898/callback"));
        QCOMPARE(query.queryItemValue("code_challenge_method"), QString("S256"));
        QCOMPARE(query.queryItemValue("state"), QString("xyz"));
        QVERIFY(!url.toString().contains("secret"));
    }

    void spotifyReadsTheCallback()
    {
        auto callback = Spotify::parseCallback("GET /callback?code=AQ%2Fx&state=s1 HTTP/1.1\r\n");
        QVERIFY(callback.isCallback);
        QCOMPARE(callback.code, QString("AQ/x"));
        QCOMPARE(callback.state, QString("s1"));

        callback = Spotify::parseCallback("GET /callback?error=access_denied&state=s1 HTTP/1.1");
        QCOMPARE(callback.error, QString("access_denied"));
        QVERIFY(!Spotify::parseCallback("GET /favicon.ico HTTP/1.1").isCallback);
        QVERIFY(!Spotify::parseCallback("POST /callback HTTP/1.1").isCallback);
    }

    void spotifyParsesResults()
    {
        const auto tracks = Spotify::parseTracks(QJsonDocument::fromJson(R"({"items": [
            {"uri": "spotify:track:1", "name": "Golden Hour Drive", "duration_ms": 214000,
             "artists": [{"name": "Lumen Coast"}, {"name": "Harbor Lights"}],
             "album": {"name": "Low Tide Radio", "images": [
                {"url": "big", "width": 640}, {"url": "mid", "width": 300}, {"url": "small", "width": 64}]}},
            {"name": "no uri"}]})").object());
        QCOMPARE(tracks.size(), 1);
        const auto track = tracks.first().toMap();
        QCOMPARE(track.value("uri").toString(), QString("spotify:track:1"));
        QCOMPARE(track.value("subtitle").toString(), QString("Lumen Coast, Harbor Lights · Low Tide Radio"));
        QCOMPARE(track.value("image").toString(), QString("mid"));
        QCOMPARE(track.value("durationMs").toLongLong(), qint64(214000));

        const auto playlists = Spotify::parsePlaylists(QJsonDocument::fromJson(R"({"items": [
            {"uri": "spotify:playlist:a", "name": "Late Night Drive", "owner": {"display_name": "Demo"},
             "items": {"total": 1}, "images": [{"url": "only", "width": null}]},
            null,
            {"uri": "spotify:playlist:b", "name": "Focus", "tracks": {"total": 18}, "images": []}]})").object());
        QCOMPARE(playlists.size(), 2);
        QCOMPARE(playlists.at(0).toMap().value("subtitle").toString(), QString("By Demo · 1 songs"));
        QCOMPARE(playlists.at(0).toMap().value("image").toString(), QString("only"));
        QCOMPARE(playlists.at(1).toMap().value("subtitle").toString(), QString("18 songs"));

        QCOMPARE(Spotify::filterPlaylists(playlists, "focus").size(), 1);
        QCOMPARE(Spotify::filterPlaylists(playlists, "  ").size(), 2);
    }

    void spotifyKeepsTheSignInPrivate()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("tono/spotify.json");
        {
            Spotify spotify(path);
            QCOMPARE(spotify.state(), QString("signedOut"));
            spotify.setClientId(" 0123456789abcdef0123456789abcdef ");
            QCOMPARE(spotify.clientId(), QString("0123456789abcdef0123456789abcdef"));
        }
        QVERIFY(QFile::exists(path));
        QCOMPARE(QFile::permissions(path) & (QFile::ReadGroup | QFile::ReadOther | QFile::WriteGroup | QFile::WriteOther),
                 QFile::Permissions());
        QCOMPARE(Spotify(path).clientId(), QString("0123456789abcdef0123456789abcdef"));
    }

    void spotifySignsInWithPkce()
    {
        QTcpServer probe;
        if (!probe.listen(QHostAddress::LocalHost, Spotify::Port))
            QSKIP("Port 8898 is in use on this machine.");
        probe.close();

        QTemporaryDir dir;
        FakeSpotify server;
        FakeBrowser browser;
        QDesktopServices::setUrlHandler("http", &browser, "open");

        Spotify spotify(dir.filePath("spotify.json"), nullptr, server.url(), server.url());
        spotify.setClientId("0123456789abcdef0123456789abcdef");
        spotify.signIn();
        QCOMPARE(spotify.state(), QString("signingIn"));
        QTRY_COMPARE(spotify.state(), QString("signedIn"));
        QTRY_COMPARE(spotify.userName(), QString("Demo Listener"));
        QDesktopServices::unsetUrlHandler("http");

        QVERIFY(browser.page.contains("Tono"));
        QVERIFY(server.authorization);
        QCOMPARE(server.tokenRequests.size(), 1);
        const QUrlQuery form(QString::fromUtf8(server.tokenRequests.first()));
        QCOMPARE(form.queryItemValue("grant_type"), QString("authorization_code"));
        QCOMPARE(form.queryItemValue("code"), QString("c0de"));
        // The verifier sent now is the one the challenge was made from.
        QCOMPARE(Spotify::codeChallenge(form.queryItemValue("code_verifier", QUrl::FullyDecoded)),
                 QUrlQuery(browser.opened).queryItemValue("code_challenge"));

        // The refresh token is kept, and signs in again on the next start.
        QFile store(dir.filePath("spotify.json"));
        QVERIFY(store.open(QIODevice::ReadOnly));
        QVERIFY(store.readAll().contains("refresh-1"));
        QCOMPARE(Spotify(dir.filePath("spotify.json"), nullptr, server.url(), server.url()).state(), QString("signedIn"));

        spotify.signOut();
        QCOMPARE(spotify.state(), QString("signedOut"));
        QVERIFY(store.seek(0));
        QCOMPARE(Spotify(dir.filePath("spotify.json")).state(), QString("signedOut"));
    }

    void spotifyRejectsAForeignCallback()
    {
        QTcpServer probe;
        if (!probe.listen(QHostAddress::LocalHost, Spotify::Port))
            QSKIP("Port 8898 is in use on this machine.");
        probe.close();

        QTemporaryDir dir;
        // A browser that never comes back: something else calls the redirect.
        FakeBrowser browser;
        browser.follow = false;
        QDesktopServices::setUrlHandler("https", &browser, "open");
        Spotify spotify(dir.filePath("spotify.json"));
        spotify.setClientId("0123456789abcdef0123456789abcdef");
        spotify.signIn();
        QDesktopServices::unsetUrlHandler("https");
        QCOMPARE(spotify.state(), QString("signingIn"));

        QNetworkAccessManager network;
        auto *reply = network.get(QNetworkRequest(QUrl(spotify.redirectUri() + "?code=stolen&state=wrong")));
        QTRY_VERIFY(reply->isFinished());
        reply->deleteLater();
        QTRY_COMPARE(spotify.state(), QString("signedOut"));
        QVERIFY(!spotify.error().isEmpty());
    }

    void spotifySearchNeedsAnAccount()
    {
        QTemporaryDir dir;
        Spotify spotify(dir.filePath("spotify.json"));
        spotify.search("anything", "track");
        QVERIFY(spotify.results().isEmpty());
        QVERIFY(!spotify.searching());
    }

    void playerTakesUris()
    {
        MprisPlayer player("org.mpris.MediaPlayer2.spotify", QDBusConnection("tono-tests-none"));
        QVERIFY(!player.supportsUri("spotify:track:1"));
        player.applyRootProperties({{"Identity", "Spotify"}, {"SupportedUriSchemes", QStringList{"spotify"}}});
        QVERIFY(player.supportsUri("spotify:track:1"));
        QVERIFY(!player.supportsUri("https://example.com"));
    }

    void metadataKeepsTheLeadArtist()
    {
        const auto track = MprisPlayer::parseMetadata({{"xesam:artist", QStringList{"Tyler, The Creator", "Kali Uchis"}}});
        QCOMPARE(track.leadArtist, QString("Tyler, The Creator"));
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
