plz note: i havent done any shit yet this is still normal hypr
okay wtf i dont understand lmao


<p align="center">
  <img src="https://i.imgur.com/LtC153m.png" />
  <img src="https://github.com/vaxerski/Hypr/actions/workflows/c-cpp.yml/badge.svg" />
  <a href="https://discord.gg/hQ9XvMUjjr"><img src="https://img.shields.io/badge/Join%20the-Discord%20server-6666ff" /></a>
  <img src="https://img.shields.io/github/issues/vaxerski/Hypr" />
  <img src="https://img.shields.io/github/issues-pr/vaxerski/Hypr" />
  <img src="https://img.shields.io/github/languages/top/vaxerski/Hypr" />
  <img src="https://img.shields.io/github/license/vaxerski/Hypr" />
  <img src="https://img.shields.io/tokei/lines/github/vaxerski/Hypr" />
  <img src="https://img.shields.io/badge/Standard-C%2B%2B20-success" />
  <img src="https://img.shields.io/badge/Hi-mom!-ff69b4" />
</p>
<br/><br/>
Hypr is a dynamic Linux tiling window manager for Xorg. It's written in XCB with modern C++ and aims to provide easily readable and expandable code.
<br/><br/>

For Hypr with `land`, see [Hyprland](https://github.com/vaxerski/Hyprland), the Wayland Compositor.

<br/>
Hypr is _only_ a window manager. It is not a compositor and does not implement a compositor's functionality. You can run it without one (e.g. Picom) though, since it runs on Xorg, which doesn't require a compositor.
<br/>

# Key Features
- True parabolic animations
- Rounded corners and borders
- Config reloaded instantly upon saving
- A built-in status bar with modules
- Easily expandable and readable codebase
- Pseudotiling
- Multiple tiling modes (dwindling and master)
- Window rules
- Intelligent transients
- Support for EWMH-compatible bars (e.g. Polybar)
- Keybinds config
- Tiling windows
- Floating windows
- Workspaces
- Moving / Fullscreening windows
- Mostly EWMH and ICCCM compliant

# Installation
I do not maintain any packages, but some kind people have made them for me. If I missed any, please let me know.

IMPORTANT: Hypr **requires** xmodmap to correctly apply keybinds. Make sure you have it installed.

For stable releases, use the Releases tab here on github, and follow the instructions to install it in the [Wiki](https://github.com/vaxerski/Hypr/wiki/Building) 

*Arch (AUR)*
```
yay -S hypr-git
```

*Void Linux*

[https://github.com/Flammable-Duck/hypr-template](https://github.com/Flammable-Duck/hypr-template)

## Manual building
If your distro doesn't have Hypr in its repositories, or you want to modify hypr,

see the [Wiki](https://github.com/vaxerski/Hypr/wiki/Building) to see build and installation instructions.

# Configuring
See the [Wiki Page](https://github.com/vaxerski/Hypr/wiki/Configuring-Hypr) for a detailed overview on the config, or refer to the example config in examples/hypr.conf.

You have to use a config, place it in ~/.config/hypr/hypr.conf

# Screenshot Gallery

![One](https://i.imgur.com/ygked0M.png)
![Two](https://i.imgur.com/HLukmeA.png)
![Three](https://i.imgur.com/B0MDTu2.png)

# Known issues
- Picom's shadow and effects do not update for cheap animations while animating
- Non-cheap animations are choppy (duh!)

# Contributions
Refer to [CONTRIBUTING.md](https://github.com/vaxerski/Hypr/blob/main/CONTRIBUTING.md) and the [Wiki](https://github.com/vaxerski/Hypr/wiki/Contributing-&-Debugging) for contributing instructions and guidelines.


# Stars over time

[![Stars over time](https://starchart.cc/vaxerski/Hypr.svg)](https://starchart.cc/vaxerski/Hypr)


