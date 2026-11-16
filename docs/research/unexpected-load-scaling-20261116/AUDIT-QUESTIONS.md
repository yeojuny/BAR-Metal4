# External audit questions

Review the frozen evidence and source material in this package.
Do not assume that a candidate was accepted, that a proposed run occurred,
or that a helper pass establishes live-game image equivalence.
Cite a file and the relevant field or source location for each finding.
Separate direct observations, source-based inferences, and unknowns.

## 1. Identity and reproduction

- Does the pinned public base plus the documented patch chain reproduce each
  claimed source state within the exact exclusions recorded by the package?
- Are R01, S01-final, G01, and G02 identities attached to the correct results?
- Is the historical 9a44 binary evidence kept separate from latest 9626 measurements?
- Are the selected source snapshots sufficient to inspect the candidate changes,
  and does the reproduction record explain any remaining external inputs?

## 2. G01: benefit and scene dependence

- What exactly changed in the ground-decal index path, including its ownership,
  lifetime, validation, ordering, and fallback behavior?
- The same-binary one-option fresh comparison establishes a +7.184% net effect
  for this benchmark. Which evidence supports the proposed mechanism, including
  actual vertex-cache reuse, and which contribution remains unmeasured?
- What limits are imposed by the historical Simmers single result of −1.3285%?
- The historical comparison reports −98 DrawScreen frame counts, with −86 in
  seconds 17–23. What explains this concentration, if anything in the evidence?
  Do not interpret those counts as GPU draw-command elimination.
- The 40 recorded simulation/units/camera/speed/pause rows match. What visual
  or dynamic-effect differences remain untested beyond those recorded fields?

## 3. G02: object count versus cost

- Does the source preserve the required byte ranges, data lifetime, and final
  consumer behavior while reducing registered buffer objects from two to one?
- What allocation, copy, registration, or retention costs remain or move,
  including the original GPU-event lease? Distinguish logical440 from host448.
- Is any claimed CPU RAM, peak-memory, or physical GPU-memory benefit actually
  measured? Net CPU RAM and peak memory are currently UNKNOWN.
- How should the observed −0.0844347% FPS result limit the performance claim?

## 4. Grass, diagnostics, and visual scope

- What is the precise definition and capture scope of GroundPrepare = 0 in the
  latest Simmers diagnostic? Which broader statements would be unsupported?
- How does the original grass-height culling contract affect work comparability?
- Which claims are supported only by the synthetic 64 × 64 sample1 helper with its
  no-shadow / low-quality conditions and RG32-versus-production-RG16 limitation?
- Keep R01’s separate 3600-case GPU check distinct from that helper.
- What does the separate real-game view comparison show, and what does it leave
  untested? Do not combine these evidence classes into a full-parity claim.

## 5. Disposition

- Preserve G03 as HOLD and G04 as source/CPU preparation only, with GPU,
  engine build, gameplay and FPS unvalidated.
- List any source defect supported by concrete evidence separately from a
  performance hypothesis or a request for a future experiment.
- For every proposed next investigation, name the unresolved question and the
  smallest evidence needed to answer it. This review does not authorize runs.
- Do not recommend merging a candidate based solely on this documentation upload.
