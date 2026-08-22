# Tectonic-Engine

2D-Multiplayer engine, based on my previous project [Cozy Wrath](https://github.com/Thepigcat76/le-game)

Features:
- Asset Manager
- Networking Infrastructure
- Serialization
- Asset embedding
- modular backend
- optional ecs
- config/settings system

# Building

To build the engine you can either use the [Gurd](https://github.com/Thepigcat76/gurd) build tool or any c compiler.

## Arguments

## Using Gurd

If you are using [Gurd](https://github.com/Thepigcat76/gurd), you do not need to build the engine manually, you can simply download it and add the directory of the engine as a module.

## Building a static library

To build the engine as a static library, simply run `gurd --static-lib` or if you are manually compiling use `my-c-compiler build.c -o build-file && ./build-file --static-lib`

# Examples

[Example Project using ECS](./example/)
