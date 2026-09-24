# omniWM
Any config language, Any layout, Any look. 


OmniWM is a Wayland Window Manager inspired by MangoWM. It will be designed from the ground up to allow *full* configurability in any config language of choice through user extensibility.

The way I intend to design this is as follows:

## Any Language:

OmniWM will come with a built-in TOML parser for configuration, but OmniWM will actually be driven and configured entirely through a *socket API*, this means that anyone can take that API and easily build a parser for any config format they want, or build a library to allow scriptable configuration in any language. Because both of these are external tools, it keeps OmniWM itself maintainable as it only has to support the socket itself. built-in TOML parser will serve as the entry point for new users and a sensible default.


## Any Layout:

OmniWM will have some default layouts (i intend to at least bring in master/stack, scrolling and Binary Space Partitioning), But instead of these layouts being hard-coded, OmniWM will have a config-driveable layout engine that allows users to define any layout they want through constraints. The goal is to provide a broad enough featureset to provide most currently-popular forms of tiling window management, scrolling window management, floating/snapping and infinite canvas types


## Any Look:

I intend to make OmniWM the *most riceable* window manager currently on the market, Native support for multi-layered textured borders, window overlays and underlays, opacity, rounding, gradients, shadows, glow, dimming, custom shaders, custom 2d and 3d animations and even the ability to spawn custom UI components that match your rice. Just like the other two points, the goal is to primarily achieve this by providing a framework rather than some simple options


## New-User friendliness:

unlike many other "hyper-configurable" window managers, OmniWM aims to still be super new-user friendly, that's why OmniWM will ship with "Macro" options that activate various included presets.


## The power of the API:

With OmniWM configured entirely through the API, it is possible for OmniWM to be driven by configs from *other window managers*, the goal is to at least provide a reference implementation for this.


## The Road Ahead:

I am still a newbie C developer (yes, this project will be built in C), so this project will take me a while. I intend to start by working out the backend details in a set of markdown documents, as i am more confident about my system design capabilities than my actual programming skills 