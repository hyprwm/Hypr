![Hypr](https://i.imgur.com/LtC153m.png)

![BuildStatus](https://github.com/vaxerski/Hypr/actions/workflows/c-cpp.yml/badge.svg)
![Issues](https://img.shields.io/github/issues/vaxerski/Hypr)
![PRs](https://img.shields.io/github/issues-pr/vaxerski/Hypr)
![Lang](https://img.shields.io/github/languages/top/vaxerski/Hypr)
![License](https://img.shields.io/github/license/vaxerski/Hypr)
![Best](https://img.shields.io/badge/Standard-C%2B%2B20-success)
![HiMom](https://img.shields.io/badge/Hi-mom!-ff69b4)
<br/><br/>
Hypr is a Linux tiling window manager for Xorg. It's written in XCB with modern C++ and aims to provide easily readable and expandable code.

!WARNING: Hypr is still in early development. Please report all bugs in Github issues, or open a PR!
<br/>

# Features
- True parabolic animations
- Config reloaded instantly upon saving
- A built-in status bar
- Keybinds
- Tiling windows
- Floating windows
- Workspaces
- Moving / Fullscreening windows

## Roadmap v2 (not in order)
- [x] Upgrade the status bar rendering to Cairo
- [x] Better status bar configability ~ WIP
- [ ] Rounded corners
- [x] Replace default X11 cursor with the pointer
- [x] Fix ghost windows once and for all
- [ ] Fix windows minimizing themselves to tray not being able to come back without pkill
- [x] Moving windows between workspaces without floating
- [x] EWMH ~ Basic, idk if i'll add more.
- [x] Docks / Fullscreen Apps etc. auto-detection
- [ ] Fix animation flicker (if possible)
- [ ] Config expansion (rules, default workspaces, etc.)


# Configuring
See the [Wiki Page](https://github.com/vaxerski/Hypr/wiki/Configuring-Hypr) for a detailed overview on the config, or refer to the example config in examples/hypr.conf.

To use a custom config, place it in ~/.config/hypr/hypr.conf

# Screenshot Gallery

![One](https://i.imgur.com/5HmGM4R.png)
![Two](https://i.imgur.com/V4lIjkC.png)
![Three](https://i.imgur.com/yvZVde7.png)

# Building
See the [Wiki](https://github.com/vaxerski/Hypr/wiki/Building) to see build instructions.

# Contributions
Refer to [CONTRIBUTING.md](https://github.com/vaxerski/Hypr/blob/main/CONTRIBUTING.md) and the [Wiki](https://github.com/vaxerski/Hypr/wiki/Contributing-&-Debugging) for contributing instructions and guidelines.
