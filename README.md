# Tono

![Tono showing a song with its blurred cover glowing behind the glass](docs/screenshot.png)

A glassy now-playing window for Omarchy. Tono shows what Spotify, or any
other music player, is playing and controls it from the keyboard: the cover,
blurred and slowly turning behind a see-through window, tints the glass the
way Apple Music's player does, and every color comes from the current Omarchy
theme, live.

Tono talks to players over MPRIS on the session D-Bus, so it needs no API
keys or logins: the Spotify desktop app, browsers playing YouTube Music or
SoundCloud, mpv, and terminal players like spotify_player all show up on
their own. When several are open, Tono follows whichever starts playing,
and the pill at the top switches between them.

## Shortcuts

| Key | Action |
| --- | --- |
| Space | Play or pause |
| N, Ctrl + → | Next track |
| P, Ctrl + ← | Previous track |
| ← / → | Back or forward 10 seconds |
| ↑ / ↓ | Volume up or down |
| S | Shuffle |
| R | Repeat (off, playlist, track) |
| Tab | Next player |
| O | Show the player's window |
| , | Preferences |
| ? | All shortcuts |
| Q | Quit |

## Preferences

Preferences (`,`) set how see-through the window is, how strongly the cover
colors it, soft iOS-like corners instead of the theme's window rounding, the
drifting cover, and whether Tono follows whatever starts playing. They're
kept in `~/.config/tono/tono.conf`. Colors aren't a preference: they always
follow the Omarchy theme, light or dark.

The glass blur is Hyprland's own (`decoration:blur`), which Omarchy turns on;
Tono never changes your Hyprland or Omarchy config.

## Build and install

```sh
bin/build      # qmake6 + make into build/tono
bin/test       # the Qt Test suite, offscreen
bin/install    # builds the Arch package and installs it with makepkg -fsi
```

After `bin/install`, Tono is in the app launcher (Super + Space).

Tono follows the language picked under Omarchy's Setup > Language. It ships
English and Spanish; other languages show in English until a catalog in
`src/i18n/` covers them.

## License

MIT. The theme and translator classes follow Omacut and Monologue by David
Heinemeier Hansson.
