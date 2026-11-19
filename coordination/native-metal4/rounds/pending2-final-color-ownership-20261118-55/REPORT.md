# Pending-2 prerequisite on the accepted R03 source

The isolated candidate adds opt-in staged presentation color ownership. The two cached textures are reusable only when their previous owners have released them. The prepared main frame retains exactly its selected texture through GPU retirement or screen continuation. Texture format, final copy and display ordering are preserved. GPU submit depth stays one.

The actual renderer translation unit passed Objective-C++ syntax checking. The existing ASan/UBSan idle pool fixture passed. No engine build, GPU run or FPS comparison ran.

Metal4PostprocessNative.mm:488 pins the prior image but requires successful retirement before the consumer can submit. Metal4RuntimeNative.mm:1030 currently enforces that requirement globally. Exact producer/event dependencies, failure-safe private output and success-gated presentation must precede ordinary cap2 admission.

tomjn's open PR #3780 uses three IOSurface/PBO slots. It submits the current GL readback before waiting for the previous fence and presents that previous slot through SDL. LASRecomp defers one fence in its own classic Metal renderer. Both are design references with different ownership and presentation contracts.

[Verification](VERIFICATION.json) · [Patch](OWNED-FINAL-PRESENTATION-COLOR.patch) · [tomjn reference](TOMJN-REFERENCE.json) · [LASRecomp reference](LASRECOMP-REFERENCE.json)
