<!-- SPDX-FileCopyrightText: 2026 yeojuny
     SPDX-License-Identifier: CC0-1.0 -->

# Original launcher artwork

These SVG files are original artwork for this independent BAR Apple Silicon
launcher. They replace launcher assets whose upstream redistribution terms were
not documented. They contain no inherited artwork, screenshots, game emblems or
source-image pixels.

All artwork in this directory is dedicated under CC0-1.0. The original port app
icon master is `../port-icon.svg`; the original BAR path wordmark is
`../../src/renderer/assets/images/logo.svg`. Corresponding generated launcher
images have their own CC0 sidecars.

The SVG masters use the existing image dimensions. Raster copies were rendered
with the existing workspace Sharp utility; PNG transparency, existing JPEG/WebP
formats and source filenames are preserved. Native macOS icon sizes use the
existing `iconutil` utility. No rendering package is added to the application.

Terrain, mode, direction, cursor and faction images retain their existing UI
meanings. The generic faction fallback remains UNKNOWN. This changes launcher
artwork only; game archives, engine rendering, gameplay, faction names and
upstream source-code attribution are unchanged.
