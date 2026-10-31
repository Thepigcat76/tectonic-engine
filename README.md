# Tectonic-Engine

2D-Multiplayer engine, based on my previous project [Cozy Wrath](https://github.com/Thepigcat76/le-game)

Features:
- Asset Manager
- Networking Infrastructure
- Serialization
- Asset embedding
- Modular backend
- Optional ECS
- Config/settings system

# Requirements

- A C23-capable compiler (`clang` by default)
- [`ccache`](https://ccache.dev/) (used by the Gurd build script)
- [lilc](https://github.com/Thepigcat76/lilc) and [cJSON](https://github.com/DaveGamble/cJSON), installed so the linker can find `-llilc` and `-lcjson`
- Backend dependencies:
  - **raylib backend:** `raylib`, OpenGL (`GL`), `m`, `dl`, `rt`, `pthread`
- Optional: `gdb` (for `-d`) and `valgrind` (for `-vg`)

# Building

To build the engine you can either use the [Gurd](https://github.com/Thepigcat76/gurd) build tool or any C compiler.

## Using Gurd

If you are using [Gurd](https://github.com/Thepigcat76/gurd), you do not need to build the engine manually, you can simply download it and add the directory of the engine as a module.

To build with the bundled build script:

```sh
gurd build --backend=raylib            # builds the engine (-> build/tectonic-bin)
gurd build example --backend=raylib    # builds the example project
gurd run example --backend=raylib      # builds and runs the example project
```

## Arguments

```
gurd [build|b|run|r] [target] --backend=<backend> [options]
```

| Argument | Description |
|---|---|
| `build`, `b` | Build only. This is the default if no command is given. |
| `run`, `r` | Build, then run the target. A target is required and it cannot be `engine`. |
| `target` | Directory containing your game's entry point (e.g. `example`). Defaults to `engine` when building. |
| `--backend=<name>` | **Required.** Selects the backend in `backends/<name>/`. Currently available: `raylib`. |
| `--builtin-server` | Compiles with `-DBUILTIN_SERVER`. |
| `-d` | Run the program under `gdb`. |
| `-vg` | Run the program under `valgrind` (full leak check). |
| `--args <...>` | Everything after this is passed to your program. Must be the last option. |

The binary is written to `build/<target>-bin` (`build/tectonic-bin` for the engine).

Examples:

```sh
gurd run example --backend=raylib --args --color red
gurd run example --backend=raylib --args --server
gurd run example --backend=raylib -d
```

> Note: the build script currently has `DEBUG` set to `true`, which compiles with `-O1`, `-g` and ThreadSanitizer (`-fsanitize=thread`). Set it to `false` in `build.c` for release builds.

# Project layout

| Directory | Contents |
|---|---|
| `engine/` | Base engine code (networking, assets, serialization, UI, ECS, ...) |
| `backends/<name>/` | Rendering/input backends, selected with `--backend=` |
| `vendor/` | Vendored third-party sources, compiled with the engine |
| `include/` | Public engine headers (`tt_*.h`) |
| `example/` | Example project using an ECS |

# Examples

[Example Project using ECS](./example/)

The example is a small multiplayer game. Every instance is either a client or a dedicated server.

## Running the example

Start the server first, then one or more clients (in separate terminals):

```sh
gurd run example --backend=raylib --args --server
gurd run example --backend=raylib --args --color red
gurd run example --backend=raylib --args --color blue
```

The client keeps retrying until it can connect to the server.

## CLI Arguments

| Argument | Description |
|---|---|
| `--server` | Run as a dedicated server instead of a client |
| `--color <red\|green\|blue>` | Player color (client only, default `green`) |
| `--ip <ip-addr>` | Server address to connect to |
| `--port <port>` | Server port |

## Controls

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Move |
| `Esc` | Toggle menu |
| Right click | Spawn a crate at the cursor (handled by the server and synced to all clients) |

Keybinds are stored in `keys.toml` (created on first launch) and can be changed there. Text is loaded from `en_us.toml`, and assets from the `assets/` directory.