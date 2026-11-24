#include "spotify.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTcpSocket>

namespace {
// Reading the user's playlists is all Tono asks for; searching needs no
// scope at all.
const QString Scopes = QStringLiteral("playlist-read-private playlist-read-collaborative");

// Spotify caps Development Mode apps at 10 results per search.
constexpr int SearchLimit = 10;

QString randomString(int length) {
    static const QString alphabet = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~");
    QString text;
    text.reserve(length);
    for (int i = 0; i < length; ++i)
        text += alphabet.at(QRandomGenerator::system()->bounded(int(alphabet.size())));
    return text;
}

QString defaultStorePath() {
    const QString config = qEnvironmentVariableIsEmpty("XDG_CONFIG_HOME") ? QDir::homePath() + "/.config"
                                                                          : qEnvironmentVariable("XDG_CONFIG_HOME");
    return config + "/tono/spotify.json";
}

QString apiMessage(const QByteArray &body) {
    const QJsonObject object = QJsonDocument::fromJson(body).object();
    const QJsonValue error = object.value("error");
    if (error.isObject())
        return error.toObject().value("message").toString();
    const QString description = object.value("error_description").toString();
    return description.isEmpty() ? error.toString() : description;
}
}

Spotify::Spotify(const QString &storePath, QObject *parent, const QUrl &accounts, const QUrl &api)
    : QObject(parent), m_storePath(storePath.isEmpty() ? defaultStorePath() : storePath), m_accounts(accounts),
      m_api(api) {
    connect(&m_server, &QTcpServer::newConnection, this, &Spotify::onConnection);
    m_signInTimeout.setSingleShot(true);
    m_signInTimeout.setInterval(5 * 60 * 1000);
    connect(&m_signInTimeout, &QTimer::timeout, this, [this] {
        stopServer();
        setState("signedOut");
        setError(tr("Spotify sign-in timed out. Try again."));
    });
    load();
}

bool Spotify::isClientId(const QString &text) {
    static const QRegularExpression valid("^[0-9a-fA-F]{32}$");
    return valid.match(text.trimmed()).hasMatch();
}

QString Spotify::redirectUriFor(quint16 port) {
    // Spotify only allows plain http for a loopback address written out as
    // an IP, not "localhost".
    return QStringLiteral("http://127.0.0.1:%1/callback").arg(port);
}

QString Spotify::codeChallenge(const QString &verifier) {
    const QByteArray digest = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QUrl Spotify::authorizeUrl(const QUrl &accounts, const QString &clientId, const QString &challenge,
                           const QString &state) {
    QUrl url = accounts.resolved(QUrl("/authorize"));
    QUrlQuery query;
    query.addQueryItem("client_id", clientId);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("redirect_uri", redirectUriFor(Port));
    query.addQueryItem("code_challenge_method", "S256");
    query.addQueryItem("code_challenge", challenge);
    query.addQueryItem("state", state);
    query.addQueryItem("scope", Scopes);
    url.setQuery(query);
    return url;
}

Spotify::Callback Spotify::parseCallback(const QByteArray &requestLine) {
    Callback callback;
    const QList<QByteArray> parts = requestLine.trimmed().split(' ');
    if (parts.size() < 2 || parts.at(0) != "GET")
        return callback;
    const QUrl url(QString::fromUtf8(parts.at(1)));
    if (url.path() != "/callback")
        return callback;
    const QUrlQuery query(url);
    callback.isCallback = true;
    callback.code = query.queryItemValue("code", QUrl::FullyDecoded);
    callback.state = query.queryItemValue("state", QUrl::FullyDecoded);
    callback.error = query.queryItemValue("error", QUrl::FullyDecoded);
    return callback;
}

QString Spotify::pickImage(const QJsonArray &images, int size) {
    QString best;
    int bestWidth = 0;
    for (const auto &value : images) {
        const QJsonObject image = value.toObject();
        const int width = image.value("width").toInt();
        const QString url = image.value("url").toString();
        if (url.isEmpty())
            continue;
        // Playlists made by users often come with one image and no size.
        const bool better = best.isEmpty() || (width >= size && (bestWidth < size || width < bestWidth))
                         || (bestWidth < size && width > bestWidth);
        if (better) {
            best = url;
            bestWidth = width;
        }
    }
    return best;
}

QVariantList Spotify::parseTracks(const QJsonObject &page) {
    QVariantList list;
    for (const auto &value : page.value("items").toArray()) {
        const QJsonObject track = value.toObject();
        if (track.value("uri").toString().isEmpty())
            continue;
        QStringList artists;
        for (const auto &artist : track.value("artists").toArray())
            artists << artist.toObject().value("name").toString();
        const QJsonObject album = track.value("album").toObject();
        QStringList subtitle{artists.join(", ")};
        if (!album.value("name").toString().isEmpty())
            subtitle << album.value("name").toString();
        list << QVariantMap{{"kind", "track"},
                            {"uri", track.value("uri").toString()},
                            {"title", track.value("name").toString()},
                            {"subtitle", subtitle.join(" · ")},
                            {"image", pickImage(album.value("images").toArray(), 128)},
                            {"durationMs", track.value("duration_ms").toInteger()}};
    }
    return list;
}

QVariantList Spotify::parsePlaylists(const QJsonObject &page) {
    QVariantList list;
    for (const auto &value : page.value("items").toArray()) {
        // Spotify sometimes leaves holes (null) in playlist pages.
        const QJsonObject playlist = value.toObject();
        if (playlist.value("uri").toString().isEmpty())
            continue;
        QStringList subtitle;
        const QString owner = playlist.value("owner").toObject().value("display_name").toString();
        if (!owner.isEmpty())
            subtitle << tr("By %1").arg(owner);
        // The song count moved from "tracks" to "items" in 2026.
        const QJsonValue contents = playlist.contains("items") ? playlist.value("items") : playlist.value("tracks");
        if (contents.isObject() && contents.toObject().contains("total"))
            subtitle << tr("%n songs", "", contents.toObject().value("total").toInt());
        list << QVariantMap{{"kind", "playlist"},
                            {"uri", playlist.value("uri").toString()},
                            {"title", playlist.value("name").toString()},
                            {"subtitle", subtitle.join(" · ")},
                            {"image", pickImage(playlist.value("images").toArray(), 128)},
                            {"durationMs", 0}};
    }
    return list;
}

QVariantList Spotify::filterPlaylists(const QVariantList &playlists, const QString &query) {
    const QString needle = query.trimmed();
    if (needle.isEmpty())
        return playlists;
    QVariantList list;
    for (const auto &playlist : playlists)
        if (playlist.toMap().value("title").toString().contains(needle, Qt::CaseInsensitive))
            list << playlist;
    return list;
}

void Spotify::load() {
    QFile file(m_storePath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    m_clientId = object.value("clientId").toString();
    m_refreshToken = object.value("refreshToken").toString();
    if (!m_refreshToken.isEmpty() && isClientId(m_clientId)) {
        m_state = "signedIn";
        fetchProfile();
    }
}

// Written beside the target and renamed over it, readable by the user alone
// from the first byte, since the refresh token opens their account.
void Spotify::save() {
    QDir().mkpath(QFileInfo(m_storePath).absolutePath());
    QSaveFile file(m_storePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(tr("Couldn't save the Spotify sign-in: %1").arg(file.errorString()));
        return;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QJsonObject object{{"clientId", m_clientId}};
    if (!m_refreshToken.isEmpty())
        object.insert("refreshToken", m_refreshToken);
    file.write(QJsonDocument(object).toJson());
    if (!file.commit())
        setError(tr("Couldn't save the Spotify sign-in: %1").arg(file.errorString()));
}

void Spotify::setClientId(const QString &clientId) {
    const QString trimmed = clientId.trimmed();
    if (trimmed == m_clientId)
        return;
    m_clientId = trimmed;
    save();
    emit accountChanged();
}

void Spotify::setState(const QString &state) {
    if (state == m_state)
        return;
    m_state = state;
    emit accountChanged();
}

void Spotify::setError(const QString &message) {
    if (message == m_error)
        return;
    m_error = message;
    emit errorChanged();
}

void Spotify::clearError() {
    setError({});
}

void Spotify::setResults(const QVariantList &results) {
    m_results = results;
    m_searching = false;
    emit resultsChanged();
}

void Spotify::copyRedirectUri() const {
    QGuiApplication::clipboard()->setText(redirectUri());
}

void Spotify::signIn() {
    if (!isClientId(m_clientId)) {
        setError(tr("Paste the Client ID of your Spotify app first: 32 letters and digits."));
        return;
    }
    if (!m_server.isListening() && !m_server.listen(QHostAddress::LocalHost, Port)) {
        setError(tr("Port %1 is in use, so Spotify can't hand the sign-in back to Tono.").arg(Port));
        return;
    }
    m_verifier = randomString(64);
    m_authState = randomString(24);
    if (!QDesktopServices::openUrl(authorizeUrl(m_accounts, m_clientId, codeChallenge(m_verifier), m_authState))) {
        stopServer();
        setError(tr("Couldn't open the browser to sign in to Spotify."));
        return;
    }
    setError({});
    setState("signingIn");
    m_signInTimeout.start();
}

void Spotify::stopServer() {
    m_signInTimeout.stop();
    if (m_server.isListening())
        m_server.close();
}

void Spotify::onConnection() {
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            // Only the request line matters; wait until it's complete.
            if (!socket->canReadLine())
                return;
            const Callback callback = parseCallback(socket->readLine());
            if (!callback.isCallback) {
                socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
                return;
            }

            const bool ok = !callback.code.isEmpty() && callback.state == m_authState;
            const QString message = ok ? tr("Tono is connecting to Spotify. You can close this tab.")
                                       : tr("Spotify didn't connect. Go back to Tono and try again.");
            const QByteArray body = QStringLiteral(
                "<!doctype html><meta charset=utf-8><title>Tono</title>"
                "<body style=\"font:16px system-ui;background:#111;color:#eee;display:grid;place-items:center;height:90vh\">"
                "<p>%1</p>").arg(message.toHtmlEscaped()).toUtf8();
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n"
                          "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
            socket->disconnectFromHost();
            handleCallback(callback);
        });
    }
}

void Spotify::handleCallback(const Callback &callback) {
    if (m_state != "signingIn")
        return;
    stopServer();
    // A reply that doesn't carry our state wasn't started by this sign-in.
    if (callback.state != m_authState) {
        setState("signedOut");
        setError(tr("The sign-in reply didn't match. Try again."));
        return;
    }
    if (!callback.error.isEmpty() || callback.code.isEmpty()) {
        setState("signedOut");
        setError(callback.error == "access_denied" ? tr("Spotify sign-in was cancelled.")
                                                   : tr("Spotify refused the sign-in: %1").arg(callback.error));
        return;
    }

    QUrlQuery form;
    form.addQueryItem("grant_type", "authorization_code");
    form.addQueryItem("code", callback.code);
    form.addQueryItem("redirect_uri", redirectUri());
    form.addQueryItem("client_id", m_clientId);
    form.addQueryItem("code_verifier", m_verifier);
    requestToken(form, [this](const QJsonObject &token) {
        if (token.isEmpty()) {
            setState("signedOut");
            return;
        }
        m_myPlaylistsLoaded = false;
        setState("signedIn");
        fetchProfile();
    });
}

// Calls `done` with the token response, or an empty object after reporting
// what went wrong.
void Spotify::requestToken(const QUrlQuery &form, const std::function<void(const QJsonObject &)> &done) {
    QNetworkRequest request(m_accounts.resolved(QUrl("/api/token")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setTransferTimeout(15000);
    QNetworkReply *reply = m_network.post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        const QJsonObject token = QJsonDocument::fromJson(body).object();
        if (reply->error() != QNetworkReply::NoError || token.value("access_token").toString().isEmpty()) {
            const QString reason = token.value("error").toString();
            // A refresh token stops working when the user removes Tono's
            // access, or the app is deleted.
            if (reason == "invalid_grant" || reason == "invalid_client") {
                m_refreshToken.clear();
                m_accessToken.clear();
                m_userName.clear();
                save();
                setState("signedOut");
                setError(tr("Spotify signed Tono out (%1). Connect again.").arg(apiMessage(body)));
            } else {
                const QString detail = apiMessage(body);
                setError(tr("Couldn't reach Spotify: %1").arg(detail.isEmpty() ? reply->errorString() : detail));
            }
            done({});
            return;
        }
        m_accessToken = token.value("access_token").toString();
        m_expiry = QDateTime::currentDateTimeUtc().addSecs(token.value("expires_in").toInt(3600));
        // Spotify may hand out a new refresh token; the old one then expires.
        const QString refreshToken = token.value("refresh_token").toString();
        if (!refreshToken.isEmpty() && refreshToken != m_refreshToken) {
            m_refreshToken = refreshToken;
            save();
        }
        setError({});
        done(token);
    });
}

void Spotify::withToken(const std::function<void()> &run) {
    if (m_refreshToken.isEmpty()) {
        setError(tr("Connect your Spotify account to search here."));
        m_searching = false;
        emit resultsChanged();
        return;
    }
    if (!m_accessToken.isEmpty() && QDateTime::currentDateTimeUtc().secsTo(m_expiry) > 30) {
        run();
        return;
    }
    m_waitingForToken << run;
    refresh();
}

void Spotify::refresh() {
    if (m_refreshing)
        return;
    m_refreshing = true;
    QUrlQuery form;
    form.addQueryItem("grant_type", "refresh_token");
    form.addQueryItem("refresh_token", m_refreshToken);
    form.addQueryItem("client_id", m_clientId);
    requestToken(form, [this](const QJsonObject &token) {
        m_refreshing = false;
        const auto waiting = std::exchange(m_waitingForToken, {});
        if (token.isEmpty()) {
            m_searching = false;
            emit resultsChanged();
            return;
        }
        for (const auto &run : waiting)
            run();
    });
}

void Spotify::apiGet(const QString &path, const QUrlQuery &query,
                     const std::function<void(const QJsonObject &)> &done, bool retried) {
    withToken([this, path, query, done, retried] {
        QUrl url = path.startsWith("https://") ? QUrl(path) : m_api.resolved(QUrl(path));
        if (!query.isEmpty())
            url.setQuery(query);
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
        request.setTransferTimeout(15000);
        QNetworkReply *reply = m_network.get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, path, query, done, retried] {
            reply->deleteLater();
            const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll();
            if (code == 401 && !retried) {
                m_accessToken.clear();
                apiGet(path, query, done, true);
                return;
            }
            if (reply->error() != QNetworkReply::NoError || code >= 400) {
                const QString detail = apiMessage(body);
                if (code == 403)
                    setError(tr("Spotify refused (%1). A Development Mode app needs its owner on Premium, "
                                "and your account added under User Management.").arg(detail));
                else if (code == 429)
                    setError(tr("Spotify is asking Tono to slow down. Try again in a minute."));
                else
                    setError(tr("Couldn't reach Spotify: %1").arg(detail.isEmpty() ? reply->errorString() : detail));
                m_searching = false;
                emit resultsChanged();
                return;
            }
            setError({});
            done(QJsonDocument::fromJson(body).object());
        });
    });
}

void Spotify::fetchProfile() {
    apiGet("/v1/me", {}, [this](const QJsonObject &me) {
        const QString name = me.value("display_name").toString();
        m_userName = name.isEmpty() ? me.value("id").toString() : name;
        emit accountChanged();
    });
}

void Spotify::search(const QString &query, const QString &kind) {
    const int serial = ++m_searchSerial;
    const QString text = query.trimmed();

    if (m_state != "signedIn") {
        setResults({});
        return;
    }

    if (kind == "mine") {
        if (m_myPlaylistsLoaded) {
            setResults(filterPlaylists(m_myPlaylists, text));
            return;
        }
        m_searching = true;
        emit resultsChanged();
        m_myPlaylists.clear();
        QUrlQuery page;
        page.addQueryItem("limit", "50");
        QUrl first = m_api.resolved(QUrl("/v1/me/playlists"));
        first.setQuery(page);
        loadMyPlaylists(first, [this, serial, text] {
            m_myPlaylistsLoaded = true;
            if (serial == m_searchSerial)
                setResults(filterPlaylists(m_myPlaylists, text));
        });
        return;
    }

    if (text.isEmpty()) {
        setResults({});
        return;
    }
    m_searching = true;
    emit resultsChanged();
    QUrlQuery parameters;
    parameters.addQueryItem("q", text);
    parameters.addQueryItem("type", kind == "playlist" ? "playlist" : "track");
    parameters.addQueryItem("limit", QString::number(SearchLimit));
    apiGet("/v1/search", parameters, [this, serial, kind](const QJsonObject &response) {
        // An answer to an older query is dropped; the newer one is coming.
        if (serial != m_searchSerial)
            return;
        setResults(kind == "playlist" ? parsePlaylists(response.value("playlists").toObject())
                                      : parseTracks(response.value("tracks").toObject()));
    });
}

// Follows the pages of the user's playlists, up to 200 of them.
void Spotify::loadMyPlaylists(const QUrl &next, const std::function<void()> &done) {
    apiGet(next.toString(), {}, [this, done](const QJsonObject &page) {
        m_myPlaylists += parsePlaylists(page);
        const QString following = page.value("next").toString();
        if (!following.isEmpty() && m_myPlaylists.size() < 200)
            loadMyPlaylists(QUrl(following), done);
        else
            done();
    });
}

void Spotify::signOut() {
    stopServer();
    m_refreshToken.clear();
    m_accessToken.clear();
    m_userName.clear();
    m_myPlaylists.clear();
    m_myPlaylistsLoaded = false;
    save();
    setError({});
    setResults({});
    setState("signedOut");
    emit accountChanged();
}
