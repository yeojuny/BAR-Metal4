# Round 35 · R03 and Prepare batch runtime smoke

The original R03 and the seven-change Prepare batch both loaded the existing Seths Ravine combat scene and exited normally. Metal API validation was enabled in both children. Neither log contained a matching Metal validation, crash or fatal error. The mapped executable and child environment were checked while each game was alive.

Both arms used seed 7654321, AA off, a 1512 × 950 viewport and the same camera. Their synced scenario receipts record 600 units created and 433 destroyed. The two native readbacks were taken while paused at simulation tick 1740.

[Original R03 at tick 1740](R03-1740.png) · [Prepare batch at tick 1740](PREPARE-1740.png)

Terrain and unit layout match visually. The range-like overlays and combat remain visible in both images. Particles, trails, and the on-screen timing text differ. The comparison does not establish pixel parity or attribute those image differences to the patch.

Commands FX completed a PreUnit line draw in both arms at tick 255. WeaponRange exercised Prepare in both arms, as shown by the native upload counter. BuildSquare, Sensor31657Circle, StartBox, EnergyExplosion and LuaNano remain unverified at draw level. [Per-path coverage](PATH-COVERAGE.json)

The staged binaries kept their pinned hashes. Both used identical runtime data and scene inputs. Their direct dependency version metadata matched, with packaged libraries for R03 and host Homebrew libraries for the batch. [Launch staging and library inventory](STAGING-EVIDENCE.json)

Two games and two screenshots were recorded. Build, helper, GPU trace capture, profiling and retry counts were zero. The existing scene controller writes incidental timing to its own logs. No FPS comparison was calculated. The original binaries, source trees, prepared indexed launcher and user's caffeinate process were verified after cleanup. [Scene evidence](SCENE-EVIDENCE.json) · [Protection evidence](PROTECTED-HASHES.json) · [Cleanup evidence](CLEANUP-EVIDENCE.json)
