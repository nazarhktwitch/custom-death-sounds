<img src="logo.png" width="128" height="128" alt="Custom Death Sounds Logo">

# Custom Death Sounds

**A simple [Geode](https://geode-sdk.org) mod for Geometry Dash that lets you replace death sounds with custom audio files**

[![Geode](https://img.shields.io/badge/Geode-5.7.1-brightgreen?style=flat-square)](https://geode-sdk.org)
[![GD Version](https://img.shields.io/badge/GD-2.2081-blue?style=flat-square)](https://store.steampowered.com/app/322170/Geometry_Dash/)

---

## Features

- Per-death-type custom sounds (spikes, blocks, other)
- Support for `.mp3`, `.wav`, and `.ogg` audio formats
- Separate settings for each death type
- Automatic fallback to the game's default sound when a custom file is missing

Additionally, this mod includes (inspired by [Custom Death Sound by rgc-exists](https://github.com/rgc-exists/Custom-Death-Sound)):

- Optional custom **Level Complete** sound
- Volume control for custom sounds
- Pitch randomization for variety on play
- Support for a directory of multiple custom death sounds (random pick per death)

Although this mod has per-death-type behavior that is not shared by other single sound mods, some of the additional playback settings above have been inspired by and implemented according to the guidelines of [rgc-exists/Custom-Death-Sound](https://github.com/rgc-exists/Custom-Death-Sound)

## Installation

### Geode (Open Geode Index)

Install Geode from [https://geode-sdk.org/](https://geode-sdk.org/)

Install Open Geode Index Proxy from [https://open-geode-index.bccst.ru/ui](https://open-geode-index.bccst.ru/ui)

Download mod via Alt. Index (in search filters)

### Manual

1. Download the latest `.geode` file from the [Releases](https://github.com/nazarhktwitch/custom-death-sounds/releases) page
2. Place it in Geode mods folder (for Steam version (default path): `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\geode\mods`)

## Usage

1. Open Geode menu in the main menu
2. Find **Custom Death Sounds** in the mod list
3. Open mod settings
4. Click the file picker next to each death type to select your custom sound
5. Use the clear button (X) to reset to default game sound

## Building from Source

### Requirements

- [Geode CLI](https://github.com/geode-sdk/cli)
- Geode SDK `5.7.1` (installed via `geode sdk install` with Geode CLI)
- Visual Studio 2022+ with C++23 support

(For Geode SDK installation refer to: `https://docs.geode-sdk.org/`)

### Steps

(I use Release build config to reduce mod size)

```bash
git clone https://github.com/nazarhktwitch/custom-death-sounds.git
cd custom-death-sounds
geode build --config Release
```

## License

MIT - see [LICENSE](LICENSE) for details
