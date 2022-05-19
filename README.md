# node-fprintd

## Description
napi addon for that gives the node the ability to authenticate fingerprints on Linux with fprintd. Heavily influenced by the [fprintd](https://gitlab.freedesktop.org/libfprint/fprintd) pam [module](https://github.com/bitwarden/clients).

## Inspiration
Developed with the end goal of adding fingerprint authentication support to the Linux [Bitwarden](https://github.com/bitwarden/clients) client. Still WIP

## Dependencies
* libsystemd
* cmake-js

## Installation
```
npm install git+https://github.com/jbwong05/node-fprintd.git
```

## Building
```
npm install
```

## Note
`node-fprintd.h` and `node-fprintd.c` can be compiled independently in the off chance that you need a C library to interface with fprintd. When researching this myself, I found very few higher level libraries that provided an abstraction layer on top of using dbus to communicate with fprintd.