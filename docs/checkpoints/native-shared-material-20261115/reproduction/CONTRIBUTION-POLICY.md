# Contribution provenance

The user requires explicit credit for improvements brought back from external contributors, including tomjn.

Attribute the actual additional work: unchanged extraction of yeojuny's existing code remains credited to yeojuny and does not require crediting tomjn as a new implementation author. Independently authored or further corrected/redesigned portions by tomjn must credit that contribution. Preserve original authorship and provenance when moving code.

- Before preparing a source export, commit, PR, or release, read `UPSTREAM-CONTRIBUTIONS.json` and preserve its applicable contributor, source PR/commit, and scope records.
- Include the recorded credit in the public change description and ship the provenance record with source exports. Preserve original Git authorship when transplanting commits; do not invent author emails or claim original authorship for adapted work.
- Distinguish fixes already present locally from new external refinements. For178, the imported tomjn refinement is the two missing LuaVBO error-format arguments from RecoilEngine PR3695.

# Native measurement isolation

During timed native measurements, every agent except the designated execution owner must pause tools and local work, including read-only source preparation, hashes, file searches, AST checks, and metadata inspection. Complete preparation before the execution owner starts and resume only after the measurement window closes. The execution owner may run the agreed, identical preflight checks in both arms. Record observed outliers and uncertain interference; do not discard them or infer causation from non-reproduction.
