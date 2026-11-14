# Audit and runtime review handoff — 2026-11-14

This is a **documentation-only publication** of ten selected local Markdown reports. It does not change engine code, app bundles, settings, or measurement results. The original files remain unchanged.

## Read first: version and evidence boundaries

- The 2026-11-13 static audit and priorities concern **a33ebfe888bc8abe0053df05e5e2892c0788d6a8**, not the complete later candidate. Their no-new-P0/P1 conclusion is historical and must not be read as clearance of later runtime behavior.
- The 2026-11-14 reports describe the newer candidate **13cd5d6f90dfe24b0325180f74fbd26fc00e2329**, signed engine SHA256 **ad5853c0513fe64b56e66eb44b916461038c18a6de8ecde536daf91b66fa2986**. The results documents themselves abbreviate this hash.
- These runtime measurements were performed separately and are published as reported, **not independently rerun or reaggregated during this export**.
- FPS and 1% low are CPU-observed DrawScreen interval metrics, not actual display throughput or GPU-only execution time.
- Raw per-frame CSVs, run JSONs, environment receipts, logs, screenshots, source/binary/patch JSON manifests, and the original plan are **not uploaded here**. The Markdown reports alone do not permit independent recomputation or complete reproducibility.

## Runtime reports

| Report | Reported result | Boundary |
|---|---|---|
| [New candidate OFF vs native245](prepare-ready-20261114-vs-native245-abba01/RESULTS.md) | 174.92 → 179.30 FPS; **+2.51%** | Different engine candidates; not pilot ON effect |
| [PrepareReady OFF/ON recovery ABBA](prepare-ready-20261114-off-on-abba-recovery01/RESULTS.md) | 178.78 → 102.57 FPS; **−42.63%** | One authorized recovery block; original failed block not included |
| [BeginReady OFF/ON ABBA](begin-ready-20261114-off-on-abba01/RESULTS.md) | 179.40 → 103.97 FPS; **−42.04%** | PrepareReady OFF; all four valid runs included |
| [Both pilots OFF/ON](both-ready-20261114-off-on-abba01/RESULTS.md) | First OFF 178.55 FPS; first ON terminated with **Renderer handoff access conflict** | No valid ON aggregate; remaining runs not performed |

Do not pool these experiments or discard their low/failed outcomes. They establish net observed mode effects, **not** causal attribution to CPU overlap, MRT COW, or another component. The PrepareReady report explicitly leaves actual eligible early release and positive work overlap unverified; MRT/COW copying/storage costs were not measured.

## Historical static review

- [Integrated audit](astra-ultra-upstream-audit-20261113/AUDIT.md)
- [BeginReady–MRT209 addendum](astra-ultra-upstream-audit-20261113/ADDENDUM-BEGINREADY-MRT209.md)
- [HQ priorities and ordering](astra-ultra-upstream-audit-20261113/HQ-PRIORITIES-20261113.md)
- [CPU audit](astra-ultra-upstream-audit-20261113/CPU-AUDIT.md)
- [GPU audit](astra-ultra-upstream-audit-20261113/GPU-AUDIT.md)
- [Version matrix](astra-ultra-upstream-audit-20261113/VERSION-MATRIX.md)

## Publication transformations and omissions

Only publication notes and link/path normalization were added. User-specific absolute filesystem prefixes were removed. Links to audited source lines were converted to GitHub links pinned to a33ebfe. Links among the six published audit documents remain relative. Unpublished evidence references are labeled as local evidence not included; bare names in historical prose also refer to omitted local evidence unless linked in this index.

These are project-authored technical reports. No game assets, third-party plan text, private profiles, secrets, binaries, or screenshots are included. Historical source claims and line numbers are not silently rewritten to match the newer source. The table below records original source-file SHA256s before normalization so an authorized holder of the local evidence can verify the originals.

| Original evidence-relative document | Original SHA256 |
|---|---|
| `prepare-ready-20261114-off-on-abba-recovery01/RESULTS.md` | `3f444a76aa928280a9014645e393870b6bc1730f770e3060957578038a94c1ab` |
| `begin-ready-20261114-off-on-abba01/RESULTS.md` | `800dd899a61cfe4e06b5da52dda8cb6d2027d49f5bf4fc4f07dd369db820e56d` |
| `both-ready-20261114-off-on-abba01/RESULTS.md` | `d59c36c09888f66dd26098106e2005ac776a7d71b6c4384bddcd1d3fdf306e6e` |
| `prepare-ready-20261114-vs-native245-abba01/RESULTS.md` | `d18b36325de62da9a649dcf057111db7241f0de8c4f59a49f3d9b796d195e07f` |
| `astra-ultra-upstream-audit-20261113/AUDIT.md` | `59deee1774c79c2afe758b179f5b7af3df828ed51b728fdcf3cfc09003cd1ac6` |
| `astra-ultra-upstream-audit-20261113/ADDENDUM-BEGINREADY-MRT209.md` | `e935c4bc810bfea9d1e7bb25378b9e15150e0db2509f7bfe2e068760b519aaaa` |
| `astra-ultra-upstream-audit-20261113/HQ-PRIORITIES-20261113.md` | `e7d1548c2d4f5941f66b0dbf330e4b296d70d9875acf234afec58ea162a19c13` |
| `astra-ultra-upstream-audit-20261113/VERSION-MATRIX.md` | `19ad30990d38e245672613b1896320940e44feec68e0bd20864c5d2e32e1ad5b` |
| `astra-ultra-upstream-audit-20261113/CPU-AUDIT.md` | `10dd466cd633270bd6106232ddb0d44708a3d3168a8dd65242d263083fb56b5d` |
| `astra-ultra-upstream-audit-20261113/GPU-AUDIT.md` | `421ad59f851a6dba050355f95e69c397f54fe2490f1e6fec5fe208f705841548` |
