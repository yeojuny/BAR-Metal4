# Selection and area-command visuals (2026-11-26)

- Removed the Metal4-only persistent, solid selected radar range in the world and selected-only PiP. BAR's original dotted allied radar coverage remains. Original command/hover radar, sonar and other range displays are unchanged. The additional overlapping radar ring was not part of the original BAR rendering contract.
- Intentionally hid Recoil's 128-world-unit vertical area-command center marker in native Metal4 previews, including area repair/reclaim/attack. This is a visual preference, not a parity fix: the original circular stencil volume, tinted area, color/blend and other state, command geometry, interaction, and gameplay are retained. The legacy GL path remains unchanged.

These changes are the selected source baseline after v1.11.61. They are not part of the already published v1.11.61 archive. ModelKey246 is an independent candidate and is not included in this change.
