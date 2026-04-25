# nicx

Personal Linux system toolkit by Nicolas.

```
  ███╗   ██╗██╗ ██████╗██╗  ██╗
  ████╗  ██║██║██╔════╝╚██╗██╔╝
  ██╔██╗ ██║██║██║      ╚███╔╝
  ██║╚██╗██║██║██║      ██╔██╗
  ██║ ╚████║██║╚██████╗██╔╝ ██╗
  ╚═╝  ╚═══╝╚═╝ ╚═════╝╚═╝  ╚═╝
```

A unified CLI for Linux administration tasks — system inspection, package
management with cross-machine tracking, git/GitHub setup, GPG key management,
and password-store administration.

---

## Install

```sh
curl -fsSL https://raw.githubusercontent.com/microservice-tech-nicolas/nicx/main/scripts/install.sh | sh
```

Installs to `~/.local/bin`. Builds from source — requires `g++`, `cmake`, `ninja`,
and `git` (the installer will offer to install them if missing).

Add to PATH if not already present:

```sh
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc && source ~/.zshrc
```

---

## Commands

| Command | Description |
|---|---|
| `nicx system info` | OS, CPU, memory, boot, disk overview |
| `nicx pkg <sub>` | Package management with install tracking |
| `nicx git <sub>` | Git + GitHub CLI setup, repo tracking |
| `nicx gpg <sub>` | GPG key management |
| `nicx pass <sub>` | password-store setup and sync |

---

## Quick Start

```sh
# System overview
nicx system info

# Install and track packages
nicx pkg install neovim tmux fzf ripgrep

# On a new machine — restore everything
nicx pkg sync

# Set up git and GitHub CLI
nicx git setup

# Track a repo and clone it
nicx git add git@github.com:microservice-tech-nicolas/nicx.git
nicx git clone nicx

# Clone all tracked repos at once
nicx git clone-all

# GPG key status
nicx gpg status

# Set up password-store
nicx pass setup
nicx pass status
```

---

## Subcommands

### `nicx system`
```
info    Full system overview (OS, CPU, RAM, boot, disks)
```

### `nicx pkg`
```
install <pkg...>   Install and track packages
remove  <pkg...>   Remove and untrack packages
update             Update all system packages
search  <term>     Search available packages
list               Show tracked packages
sync               Install all tracked packages (fresh machine restore)
```

### `nicx git`
```
setup              Check git + gh installed, verify identity, check auth
auth               Run gh auth login interactively
status             Show git/gh/GPG signing status
add <url> [name] [org]  Track a repo (parses name/org from URL)
remove <name>      Untrack a repo
list               List tracked repos with clone status
clone <name>       Clone a tracked repo (or pull if already present)
clone-all          Clone all uncloned tracked repos
```

### `nicx gpg`
```
setup              Check gpg installed, guide key generation if needed
list               List available secret keys
status             Show signing key, commit.gpgsign, pass gpg-id
export <id>        Export public key to stdout (ASCII-armored)
import <file>      Import a key from file
```

### `nicx pass`
```
setup              Install pass + gpg, init store, set up git inside store
status             Store dir, gpg-id, git remote, last sync, password count
pull               Pull latest from git remote
push               Push to git remote
```

---

## Configuration

`~/.config/nicx/config.toml` is created on first run:

```toml
[output]
color = true

[git]
dev_dir = ~/Development
# gh_user = myusername

[pass]
# gpg_key_id = 6BE764C6C37DEB87
# store_dir = ~/.password-store
```

Tracked packages: `~/.config/nicx/packages.toml`
Tracked repos: `~/.config/nicx/repos.toml`

---

## Build from Source

```sh
git clone https://github.com/microservice-tech-nicolas/nicx.git
cd nicx

# Install to ~/.local/bin (no root)
make install-user

# Install system-wide
sudo make install
```

Requires: `g++` (C++23), `cmake` 3.20+, `ninja`

---

## Man Page

```sh
man nicx
```

Installed automatically by `make install-user` and `make install`.

---

## Extending

nicx uses a self-registering command pattern — adding a new command requires
only a single new `.cpp` file with a static `core::CommandRegistrar`. No
existing file needs to be touched.
