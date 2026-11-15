# Current native shared-material / model-type-facts checkpoint

This development checkpoint is based on **v1.11.3**, then P (screen setter elision), Q (shared-program Sepia testbed) and S (model-positive backing facts). It contains the complete integrated engine under `engine/`, not only the last five-file patch. It is not a release, public performance promotion or Homebrew update.

[Exact source chain, trees, signed engine and normalization exceptions](SOURCE-CLOSURE.json) · [Every frozen input and Git blob](FULL-SOURCE-FILES.json).

The actual compiled S engine has SHA-256 `5069655ce90fbd301dce953f277d264af7f15f18aeff5ca16dd9651681d884a7`. P/Q/S source commits are separated in history. Every frozen file is compared against Git; four historical simdjson dotfiles remain and two personal path strings in one non-build provenance JSON are normalized, with original/published hashes recorded. All C++/Objective-C++, generated shaders, GLSL, Lua and test input bytes are preserved.

P observed setter reduction, with mean 184.700 → 184.738 FPS (+0.0208%); no established FPS gain. Q is a restricted GLSL AST-to-MSL/typed metadata/Lua testbed using the **160-byte exposure/bias variant**. The original tools README describes the earlier 128-byte packet; do not regenerate without the variant argument. Same-session hot relink, exact pixel equivalence, generic GLSL compilation and CUS pilot performance are not proved. Q's CUS prototype is off in all current tests.

S reuses only positive CPU model-type facts, with exact weak owner and execution-scope bounds. Provider requests stay 218/job; successful model casts fall 59 → 18, with 410 RTTI operations omitted per sampled job. GPU binding/readiness/lifetime work remains on its original path. The complete clean OFF/ON/ON/OFF set has mean OFF 184.902314 and ON 184.618023 FPS (−0.153752%); B1 is retained. FPS improvement and statistical equivalence are not established. The next private experiment policy selects MODEL_TYPE_FACTS=1 and SCREEN_STATE_ELISION=1; compiled defaults remain 0.

The shipped outer launcher clears BAR_NATIVE variables. Exporting pilot flags before launching that wrapper does not enable this experiment. Use a separately signed private native runtime and the recorded explicit child environment, with PREPARE=1, BEGIN=0, worker=1, FINAL_COLOR_STAGE=1, CUS pilot=0, Sepia off, and trace/Validation off for clean measurement. More detailed result, fixture and reproduction records are being added to this draft checkpoint; the release and Homebrew remain pinned.

Credits: existing engine/native work retains original authors including yeojuny; the inherited LuaVBO two-format-argument correction is from tomjn's RecoilEngine PR #3695, not a new import here. PR #3035 affinity and the benbreen QoS experiment are absent in this lineage. Beherith's Sepia widget/GLSL copyright and GPL v2-or-later notices are retained in `engine/tools/sepia-pilot/PROVENANCE.json` and original source.
