# World Cup 2026 Informer (STOMP)

A live World Cup updates service built for the SPL course at BGU.
Users subscribe to a game channel, report game events to it, and receive the events
reported by other subscribers, all over the
[STOMP 1.2](https://stomp.github.io/stomp-specification-1.2.html) protocol.

- **Server (Java, Maven)** - a STOMP server that runs in either **Thread-Per-Client** or **Reactor** mode.
- **Client (C++11)** - a two-threaded CLI client (keyboard thread + socket thread).
- **SQL server (Python + SQLite)** - the Java server sends it SQL to persist users, login/logout times and reported files.

```
 C++ client --STOMP--> Java server (TPC / Reactor) --SQL over TCP--> Python SQL server --> SQLite
```

## Layout

| Path | Contents |
|------|----------|
| `server/` | Maven project. Entry point: `bgu.spl.net.impl.stomp.StompServer`; protocol in `StompMessagingProtocolImpl`; `ConnectionsImpl` tracks handlers/subscriptions; `Database` talks to the SQL server |
| `client/` | C++ client (`src/`, `include/`, `makefile`), sample event files in `client/data/` |
| `data/sql_server.py` | Python SQL server (listens on `127.0.0.1:7778`, creates `stomp_server.db`) |

## Requirements

Java 8+, Maven, g++ (C++11), make, Boost (`libboost-system-dev`), Python 3.

## Build

```bash
cd server && mvn compile        # server
cd client && make               # produces client/bin/StompWCIClient
```

## Run

Start each in its own terminal:

```bash
# 1. SQL server (port 7778 is hard-coded in the Java server)
cd data && python3 sql_server.py

# 2. STOMP server: "tpc" or "reactor"
cd server && mvn exec:java -Dexec.mainClass="bgu.spl.net.impl.stomp.StompServer" -Dexec.args="7777 reactor"

# 3. One or more clients (run from client/ so the sample file paths work)
cd client && ./bin/StompWCIClient
```

## Client commands

| Command | Effect |
|---------|--------|
| `login {host:port} {user} {pass}` | Connect and log in (creates the user on first login) |
| `join {game}` | Subscribe to a game channel, e.g. `Germany_Japan` |
| `exit {game}` | Unsubscribe from a channel |
| `report {file}` | Send every event in a JSON events file to its game channel (must be subscribed) |
| `summary {game} {user} {file}` | Write the stats and events received from `{user}` for `{game}` to `{file}` |
| `logout` | Graceful disconnect (DISCONNECT + RECEIPT) |

## Example session

Terminal A (`bob`, listens) and terminal B (`alice`, reports):

```
A> login 127.0.0.1:7777 bob pw
A> join Germany_Japan
B> login 127.0.0.1:7777 alice pw
B> join Germany_Japan
B> report data/events1_partial.json
A> summary Germany_Japan alice out.txt     # out.txt now holds Alice's summary
A> logout
```

See `summary-example-event1.txt` for the summary format.

## Database

The SQL server keeps three tables: `users`, `login_history` (login/logout times) and
`file_tracking` (files uploaded via `report`). Inspect with
`sqlite3 data/stomp_server.db "select * from login_history;"`.

## Notes

Originally written as a university assignment; revisited and verified end-to-end in both
TPC and Reactor modes (two clients, join/report/summary/logout, DB rows written).
