# Media strategy: demo footage, README media, LinkedIn reel (2026-07-11)

How to turn the Windows screen-captures in
`~/Downloads/browser_progress_artifacts/` into (a) the README hero
GIF/stills that `outward-facing-docs.md` flags as the #1 gap, and (b) a
LinkedIn demo reel.

This is the durable plan; the raw analysis was done by watching all eight
clips + four stills frame-by-frame.

## Source inventory

Grades reflect polish, not importance. All clips are 30fps, ~2878×1798 (16:10,
high-DPI), and have **junk audio** (ambient YouTube in a background tab). Mute
everything.

| File | Len | Taskbar content | Best clip window | Use | Grade |
|---|---|---|---|---|---|
| `finished_product.mp4` | 33s | 2-row pill: `Gmail·GitHub·Projects` buttons + active-tab pill; hover-fan listing all tabs | **~0:25-0:31** (fan) | Hero / README | A- |
| `click_on_and_go_to_minimized_tabs.mp4` | 42s | Gap fills with grouped per-window tab-chip row, `+11` overflow, `no minimized browsers` placeholder; click restores tab | **0:30-0:42** | Core-value demo | A |
| `fan_functionality_automatic_buttons.mp4` | 21s | Chip row + `Gmail/GitHub` auto-buttons; plan-mode frame showing the architecture decision | 0:00-0:03 (decision) | Building-in-public | B |
| `multiplle tabs.mp4` | 24s | Two staggered window cards, per-tab chips, `+2 (stale)` trailer | **~0:18-0:24** | Multi-window shot | B+ |
| `taskbar_auto_allocation.mp4` | 31s | Green debug outline measuring the real gap `[1418,1948]` via UIA | 0:03-0:12 | "How it works" only | B- |
| `mvp_slightly_flickered.mp4` | 22s | Two tiny `Gmail·GitHub` pills bottom-right | 0:00-0:04 | Progression "day 1" | C+ |
| `improved_flicker_except_app_opening.mp4` | 73s | Task Manager (Edge=27 procs), theme design, low CPU | - | Progression / cred | C |
| `live_shell_profiling.mp4` | 56s | `shell_profiler.exe` ETW consumer, live event table, CPU 0.3-0.8% / 24MB / 288 handles | **0:09-0:22** | Engineering-cred proof | A- |
| `profiler_inefficiency_tracking.png` | - | Power BI fan-latency dashboard: 602ms avg, restore→tabfound 74.2% dominant, 7 samples | still | Cred still / carousel | A |
| `taskbar_allocation_limit_test.png` | - | Overflow test - many chips + jumplist | still | Supporting | B |
| `taskbar_edit_v1.png` | - | Green-outline gap + Gmail/GitHub pills + the UIA reasoning text | still | Annotated "how" | B+ |
| `final_product_image_tab_overload.png` | - | Mislabeled - it is a file-explorer screenshot of the folder | - | Discard | - |

## Optional polish (NOT blockers)

The footage is usable as-is. The name-bearing email and the personal tab titles
are fine to show: explicitly **not** a privacy concern, no sanitizing needed.
The items below are purely aesthetic, take-or-leave:

- **Wallpaper**: the AWS "Shared Responsibility Model" diagram is busy behind
  the UI; a solid/neutral background would make the chips pop more. Optional.
- **Legibility**: chips are small at full frame, so crop to the taskbar strip
  (see ffmpeg appendix) instead of re-shooting.

## Core recommendation: crop existing; optional single re-record

The existing clips are strong raw material. Cropping them to the taskbar strip
(ffmpeg appendix) already yields usable README GIFs; nothing needs to be
cleaned or hidden.

The one thing the current footage lacks is a single clean *minimize → chips →
hover-fan → click-restore* sequence in one take (the fan moment is fleeting and
scattered across clips).

If you want a crisp hero, re-record ONLY that ~20 to 30s sequence, for the
tight choreography, not for privacy. Everything else stays as-is for
building-in-public / profiler material.

## Track A: README / `docs/media/`

GitHub READMEs autoplay looping **GIF/APNG** but not MP4. Deliverables:

1. `docs/media/hero.gif`: ~6 to 8s loop, <10MB, cropped to the taskbar strip:
   *minimize → chips → hover-fan → click-restore*. From the clean re-record, or
   splice `click_on_and_go` 0:30 to 0:42 (chips) + `finished_product` ~0:27 (fan).
2. `docs/media/hero.png`: clean still under the H1: the staggered two-window
   "cards + per-tab chips" frame (`multiplle tabs` ~0:20 or re-record).
3. `docs/media/how-it-works.png`: annotated `taskbar_edit_v1.png` (green gap
   measurement + UIA text) for `docs/overview.md`.
4. Keep GIFs small; link full MP4s (or a YouTube/Loom) below the fold.

Fills the `<!-- TODO: hero screenshot / GIF … docs/media/ -->` in `README.md`.

## Track B: LinkedIn reel (45 to 60s, captioned, muted-friendly)

LinkedIn autoplays muted → burn in captions. Lean into "native Win32 shell
tool, self-profiled": the AI-assisted-dev and profiler angles are a strength
here, not noise.

| Beat | Time | Source | Caption |
|---|---|---|---|
| 1. Problem | 0-6s | `mvp` or staged minimize | "Minimize a browser and everything you were reading disappears." |
| 2. The fix | 6-20s | clean re-record / `click_on_and_go` 0:30-0:42 | "So I made the taskbar's dead gap show your minimized tabs as chips." |
| 3. Fan + restore | 20-30s | `finished_product` ~0:27 | "Hover to fan out every tab. Click one - it restores that exact tab." |
| 4. Depth | 30-42s | `taskbar_auto_allocation` + `multiplle tabs` | "Measures the real taskbar gap live via UI Automation, stacks multiple windows." |
| 5. Proof | 42-55s | `live_shell_profiling` 0:09-0:22 + dashboard PNG | "Ships with its own ETW profiler. 0.3% CPU, 24MB. C++17 / Win32, zero deps." |
| 6. Close | 55-60s | repo / README | "browser_shell_os - on GitHub." |

Optional building-in-public open: the `fan_functionality` plan-mode frame (the
architecture decision on screen) → "designed, not vibe-coded."

## ffmpeg appendix

Paths use forward slashes (ffmpeg accepts them on Windows). Commands are single
lines so they paste into either PowerShell or Git Bash. Requires `ffmpeg` on
PATH (`winget install Gyan.FFmpeg`).

`SRC` = the artifacts folder; outputs go to an `edited/` working dir, finals
get copied into `docs/media/`.

**0. Setup / find your crop.** Grab a full-res frame to measure the taskbar strip
and fan bounds, then set crop heights (from the bottom): STRIP≈150 (chips only),
FAN≈920 (chips + upward fan). Nudge after previewing `probe.png`.

```
mkdir "C:/Users/randl/Downloads/browser_progress_artifacts/edited"
ffmpeg -ss 00:00:27 -i "C:/Users/randl/Downloads/browser_progress_artifacts/finished_product.mp4" -frames:v 1 "C:/Users/randl/Downloads/browser_progress_artifacts/edited/probe.png"
```

**1. README hero GIF: hover-fan** (crop fan region, scale to 1000 wide, quality palette):

```
ffmpeg -ss 00:00:25 -t 6 -i "C:/Users/randl/Downloads/browser_progress_artifacts/finished_product.mp4" -vf "crop=iw:920:0:ih-920,scale=1000:-1:flags=lanczos,fps=15,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3" -loop 0 "C:/Users/randl/Downloads/browser_progress_artifacts/edited/hero_fan.gif"
```

**2. README chips-strip GIF: legible** (crop to strip only, upscale to 1280 wide):

```
ffmpeg -ss 00:00:30 -t 12 -i "C:/Users/randl/Downloads/browser_progress_artifacts/click_on_and_go_to_minimized_tabs.mp4" -vf "crop=iw:150:0:ih-150,scale=1280:-1:flags=lanczos,fps=15,split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3" -loop 0 "C:/Users/randl/Downloads/browser_progress_artifacts/edited/chips_strip.gif"
```

**3. Stills** (full frame + cropped strip):

```
ffmpeg -ss 00:00:20 -i "C:/Users/randl/Downloads/browser_progress_artifacts/multiplle tabs.mp4" -frames:v 1 "C:/Users/randl/Downloads/browser_progress_artifacts/edited/still_multiwindow.png"
ffmpeg -ss 00:00:36 -i "C:/Users/randl/Downloads/browser_progress_artifacts/click_on_and_go_to_minimized_tabs.mp4" -frames:v 1 -vf "crop=iw:160:0:ih-160" "C:/Users/randl/Downloads/browser_progress_artifacts/edited/still_chips_strip.png"
```

**4. GIF size optimization** (optional, needs gifsicle: `winget install gifsicle`):

```
gifsicle -O3 --lossy=60 "C:/Users/randl/Downloads/browser_progress_artifacts/edited/hero_fan.gif" -o "C:/Users/randl/Downloads/browser_progress_artifacts/edited/hero_fan_opt.gif"
```

**5. LinkedIn beats.** Normalize every beat to 1920×1080 / 30fps / no audio so
they concat with `-c copy`. Two filter choices per beat:

- *Whole-screen* (context): `scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30`
- *Zoom-to-strip* (legible chips): `crop=iw:920:0:ih-920,scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30`

Example: beat 2 zoomed to the chips:

```
ffmpeg -ss 00:00:30 -t 12 -i "C:/Users/randl/Downloads/browser_progress_artifacts/click_on_and_go_to_minimized_tabs.mp4" -vf "crop=iw:920:0:ih-920,scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30" -an -c:v libx264 -crf 20 -pix_fmt yuv420p "C:/Users/randl/Downloads/browser_progress_artifacts/edited/beat2.mp4"
```

Repeat per beat (`beat1`…`beat6`) changing `-ss`/`-t`/input. Then concat +
web-optimize (create `list.txt` with one `file 'beatN.mp4'` line per beat, in
order):

```
ffmpeg -f concat -safe 0 -i "C:/Users/randl/Downloads/browser_progress_artifacts/edited/list.txt" -c copy "C:/Users/randl/Downloads/browser_progress_artifacts/edited/demo_reel.mp4"
ffmpeg -i "C:/Users/randl/Downloads/browser_progress_artifacts/edited/demo_reel.mp4" -c copy -movflags +faststart "C:/Users/randl/Downloads/browser_progress_artifacts/edited/demo_linkedin.mp4"
```

Captions: add after cutting (CapCut/Premiere/Descript), or burn a subtitle file
with `-vf subtitles=captions.srt` on the final pass.

## Dashboard figures (Opus consult, 2026-07-11)

Through-line: **the dashboard is evidence, not the product.** Figure prominence
decays as the audience gets less technical.

Every number carries the `n=5` caveat; the Capture-2 double-count caveat
travels ONLY with per-stage figures (it doesn't distort total-latency, so it's
absent from Run Trends captions).

**Per-destination picks:**

- `docs/dashboard/README.md` (technical): go heavy: full-page PNGs of
  **Overview**, **Stage Bottlenecks**, **Run Trends**. Skip Capture Metadata as
  a figure (link it as provenance in prose).
- `profiler/README.md`: one figure max: **Overview big-number cards, cropped**
  (drop the 7-bar chart) + one line "full analysis in `docs/dashboard/`". Do NOT
  repeat Run Trends here.
- Main `README.md`: **one prose sentence + one link, zero embedded figures.**
  A "How it's built" aside near the bottom, not the hero. Inverting that altitude
  makes the BI aside compete with the product pitch.
- LinkedIn beat 5: **one frame: the Run Trends descent** (a downward line reads
  muted-and-scrolling; a card grid doesn't).

**Export vs crop vs prose:** full PNG export (Power BI → Export, high-DPI) when
the layout IS the argument + reader is technical; cropped screenshot to pull one
visual off a busy page.

Use prose numbers when the figure would out-weigh its point (always in the
main README, the only place the caveat can't fit a caption).

**Caption text (as used; sample is 5 runs / 102 clicks, NOT "7 clicks": that
was one baseline run's count in the old figure):**

- Run Trends: *"Guided descent: baseline 602 ms → latest 271 ms (-331 ms, 55%)
  across five capture runs. Run count: 5, each aggregating ~20
  clicks (102 total)."*
- Stage Bottlenecks: *"Where the per-run latency goes, stage by stage:
  `restore→tab-found` tops at 306.5 ms. Caveat: `First walk` and `Gate 2 wait`
  both read 283 ms (same physical interval measured two ways), so the per-stage
  total (1,554 ms) double-counts 283 ms. Total-latency figures unaffected."*
- Overview: *"Average total 530.6 ms (fastest run 271 ms) across 5 runs / 102
  clicks. `restore→tab-found` is the dominant stage."*
- LinkedIn on-frame: *"Profiled my own tab-restore path. Cut latency 602 ms →
  271 ms. (5 runs / 102 clicks.)"*
- Main README prose: *"Ships with a separate ETW-based profiler
  (`shell_profiler`) and a git-diffable Power BI dashboard I used to guide
  latency work on the tab-restore path: details in `docs/dashboard/`."*

**Framing (maturity, not bragging):** describe the *behavior* (measure → let data
pick the target → outcome), not the artifact. Lead with problem+outcome, tools
as the how; name the caveat yourself; call it a hobby tool.

LinkedIn voiceover: *"And because I can't leave well enough alone, I profiled
my own code with ETW and a Power BI dashboard, and used it to cut restore
latency roughly in half."*

**GitHub rendering:** dark-theme PNGs look broken on light mode. Bake a 1px
light-gray border, or use `<picture>` + `prefers-color-scheme` (dual export) in
`docs/dashboard/` only. Constrain `<img width="800">` (don't inline 1920px raw).
Alt text must carry the finding, not the filename
(`alt="Run Trends: average latency falling 602 ms → 271 ms across five runs"`).
Cut chartjunk before export (default titles, gridlines, >3 sig figs). Put PNGs in
`docs/dashboard/img/`, commit them, keep each <300 KB.

**Under-sold signals to add (one line each):** (1) the pbip is git-diffable text,
so "a BI dashboard checked into version control so its changes review like code"
does more portfolio work than the figures. (2) A provenance line near each PNG:
"generated from capture runs on <date>, see `CaptureMeta`". (3) A regenerate
line: "open `docs/dashboard/*.pbip` in Power BI Desktop over the captures":
turns "made a chart" into "built an instrument".

**Rounding consistency:** use exact figures everywhere, "602 ms → 271 ms", for
both portfolio and technical readers. Don't mix rounded and exact forms.

**Status: executed (2026-07-11).** All 3 page PNGs captured and placed in
`docs/dashboard/img/` (`overview.png`, `stage-bottlenecks.png`,
`run-trends.png`) with the captions above wired into `docs/dashboard/README.md`.

`overview-cards.png` is cropped into `profiler/README.md`; prose aside + link
added to the main `README.md`. Note: the live dashboard has 3 pages: Stage
Bottlenecks and Capture Metadata are combined on one page.

## Open decisions

1. Re-record a clean hero take, or ship cropped from existing? (Re-record wins.)
2. LinkedIn framing: pure product demo vs. building-in-public (show Claude Code
   plan-mode + profiler). The latter suits a technical audience.
3. Whether to publish full MP4s anywhere (YouTube/Loom) to link from README.
