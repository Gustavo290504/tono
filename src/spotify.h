#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>
#include <functional>

// The user's Spotify account, for searching songs and playlists and listing
// their own playlists through the Spotify Web API. Playing a result doesn't
// go through the API: the Spotify app is told to open it over MPRIS, which
// works on any plan.
//
// Signing in uses the user's own Spotify app (its Client ID) and the PKCE
// flow, so there is no client secret to keep: the browser hands the sign-in
// back to a one-shot server on 127.0.0.1, and only the refresh token is kept,
// in ~/.config/tono/spotify.json, readable by the user alone.
class Spotify : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY accountChanged)
    // signedOut, signingIn or signedIn.
    Q_PROPERTY(QString state READ state NOTIFY accountChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY accountChanged)
    Q_PROPERTY(QString redirectUri READ redirectUri CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY resultsChanged)
    // [{kind, uri, title, subtitle, image, durationMs}]
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)

public:
    static constexpr quint16 Port = 8898;

    explicit Spotify(const QString &storePath = {}, QObject *parent = nullptr,
                     const QUrl &accounts = QUrl("https://accounts.spotify.com"),
                     const QUrl &api = QUrl("https://api.spotify.com"));

    QString clientId() const { return m_clientId; }
    void setClientId(const QString &clientId);
    QString state() const { return m_state; }
    QString userName() const { return m_userName; }
    QString redirectUri() const { return redirectUriFor(Port); }
    QString error() const { return m_error; }
    bool searching() const { return m_searching; }
    QVariantList results() const { return m_results; }

    Q_INVOKABLE void signIn();
    Q_INVOKABLE void signOut();
    // kind is "track", "playlist", or "mine" (the user's own playlists,
    // filtered by the query).
    Q_INVOKABLE void search(const QString &query, const QString &kind);
    Q_INVOKABLE void copyRedirectUri() const;
    Q_INVOKABLE void clearError();
    Q_INVOKABLE static bool isClientId(const QString &text);

    static QString redirectUriFor(quint16 port);
    static QString codeChallenge(const QString &verifier);
    static QUrl authorizeUrl(const QUrl &accounts, const QString &clientId, const QString &challenge,
                             const QString &state);

    struct Callback {
        bool isCallback = false;
        QString code;
        QString state;
        QString error;
    };
    // The first line of the browser's request to the sign-in server.
    static Callback parseCallback(const QByteArray &requestLine);
    static QVariantList parseTracks(const QJsonObject &page);
    static QVariantList parsePlaylists(const QJsonObject &page);
    static QVariantList filterPlaylists(const QVariantList &playlists, const QString &query);
    // The smallest image at least `size` pixels wide, or the largest there is.
    static QString pickImage(const QJsonArray &images, int size);

signals:
    void accountChanged();
    void errorChanged();
    void resultsChanged();

private:
    void load();
    void save();
    void setState(const QString &state);
    void setError(const QString &message);
    void setResults(const QVariantList &results);
    void stopServer();
    void onConnection();
    void handleCallback(const Callback &callback);
    void requestToken(const QUrlQuery &form, const std::function<void(const QJsonObject &)> &done);
    void withToken(const std::function<void()> &run);
    void refresh();
    void apiGet(const QString &path, const QUrlQuery &query, const std::function<void(const QJsonObject &)> &done,
                bool retried = false);
    void loadMyPlaylists(const QUrl &next, const std::function<void()> &done);
    void fetchProfile();

    QString m_storePath;
    QUrl m_accounts;
    QUrl m_api;
    QString m_clientId;
    QString m_refreshToken;
    QString m_accessToken;
    QDateTime m_expiry;
    QString m_state = "signedOut";
    QString m_userName;
    QString m_error;
    QString m_verifier;
    QString m_authState;
    QTcpServer m_server;
    QTimer m_signInTimeout;
    QNetworkAccessManager m_network;
    QList<std::function<void()>> m_waitingForToken;
    bool m_refreshing = false;
    QVariantList m_results;
    bool m_searching = false;
    int m_searchSerial = 0;
    QVariantList m_myPlaylists;
    bool m_myPlaylistsLoaded = false;
};
