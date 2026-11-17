# Texture trace: actual CPU admission observed, one validation game

Fixed source92b8b718a8cc2fe374de9a2889c8e289fc438474 (exact parent3713df592f99b6f8c3edc91ad99803e073daa230), manifest262cefbd639de63e833a7847b868a99de0aa2045e74b9d46691535e68bfe759f. Source packet/parent/patch hashes match; only declared2 production files change. One CPU script (2 compiler invocations,8 live/flag subruns) passes concurrency/budget/exception/nonlive/flag/wiring checks. One configure/build passes with production RECOIL_METAL4_LIVE_SCENE define;3142 compile rows differ only downloader version metadata.1538 objects/35 archives/0 mismatches. No source/test fixes or retries.

Raw engine2cd2f5c67223009333c630c8dacdc70f0fe16275f511a8539768f42a561733b0; signedf72528f6322627c32fe63548aeb126ccbb43fd723a8e22dd58bd689662118331. One copied runtime relocation/sign,19 unchanged libraries; one fresh single ON_TRACE fixture, exact pins frozen before launch. Runner2e3a727f86a5c239b0633583049fcd3c942b4bc6c8108be7c992e888f8078260; controller/parser/pacing/error/cleanup unchanged, execution AST identical after completion-label normalization.

Actual child INPUT1 TRACE1 and Metal API Validation1/shader validation0 confirmed. One game, normal exit,0 matched Metal errors. Canonical engine.log printed events:

| Event | Printed lines | Meaning |
| --- | ---: | --- |
| armed |1| Tracer enabled and Texture entry reached; not admission. |
| admitted |6| Existing exact pending-producer/independent-slot admission passed. |
| encode_permit_check |4| Borrower readability checked; duplicate upload/mip branch skipped. Not consumer draw/GPU execution. |
| producer_retired_check |4| Final prior-read callback checked producer completion. NOT consumer GPU completion. |

All nonarmed lines share printed producer64/consumer65 and log simulation frames91..103. Repeated pair is preserved; no unique texture/resource count, one-to-one event join, total frequency or rate claim. Independent caps are1/64/64/64; swallowed diagnostic exceptions also limit negative inference. Full ordered raw lines and grouped printed-pair relations are in TRACE-EVENTS.json. infolog duplicates are preserved but not added to canonical counts.

This run positively demonstrates actual CPU ordering admission and the two later guard sites, unlike prior ON-only unknown evidence. It does not retroactively prove earlier untraced runs used the path, nor prove consumer GPU completion or all lifetime/error cases. Observed admission/checks precede matched battle window540..1740; trace/validation timings are excluded and no FPS benefit is claimed. Stop after this one run; no workload changes or automatic repeat.

Raw sample1/drawable1512x982/safeview1512x950, Ultra4/AA0 PREPARE1 BEGIN0 worker1 S01/G01 conditions match. All40 world/tick/camera/speed/pause/unit rows match preserved prior OFF and ON; initial/seed154658555/camera and final600created433destroyed match. Actual PNG viewed relative to prior preserved OFF/ON: no obvious new static terrain/building/decal/occlusion corruption, animation/illumination/range arcs/debug text differ. RGB review is not exact pixel/depth/stencil equality; PNG stays local with hash/path/size in IMAGE-REVIEW.

Owned game49043, runner49034, awake49035 and registered build/sign/stage children exited; groups gone, image worker joined, awake released. Source unchanged after build; raw/signed and19 dylibs unchanged after game. Main/release/Homebrew/original app, GroundDecal/R04 protected. No GPU capture/helper/benchmark, source fixes, security/debugger change or extra game. Existing engine GPL/Beherith MIT/tomjn/benbreen provenance preserved.
