# Building Monolith Reforged (ZONA baseline)

**Host:** Windows **only**. This tree does not support building the full Anomaly engine on Linux.

## Prerequisites

- **Visual Studio 2022** with **Desktop development with C++**
- **MFC and ATL** (see upstream README — required for a successful build)
- Submodules: from repo root run  
  `git submodule update --init --recursive`

## Solution

Primary solution file:

- `src/engine-vs2022.sln`

Upstream also documents `src/engine.sln` in some docs; prefer **`engine-vs2022.sln`** for the VS2022 workflow (see `README.md` in this repo — *How to compile exes*).

## Configurations

- Use **Release** / **Release_MT** (names may vary by project) for binaries you ship to players.
- **VerifiedDX11** is for **Hot Reload** / debugging — very slow and memory-heavy; not for playtesting modpacks.

## Outputs

After build, copy the produced `.exe` / DLLs from the build output tree (see upstream README — e.g. `_build`-style paths) into your Anomaly `bin` folder. Match **DX11 / AVX** variant to what ZONA expects.

## Pin to ZONA releases

ZONA pins MT co-op exes to release **2026.9.12** (`zona/config.toml` → `exes_mp`). Build from tag **`2026.9.12`** or from branch **`zona/baseline-2026.9.12`** for alignment with that line.

## Next steps (project)

- Megamap is mostly **Level Editor + gamedata** — see [`../docs/engine/02-megamap-gamedata-spike.md`](../docs/engine/02-megamap-gamedata-spike.md).
- Optional OMP merge: [`../docs/engine/05-omp-monolith-merge-poc.md`](../docs/engine/05-omp-monolith-merge-poc.md), diffs via `../tools/engine_diff_omp_monolith.py` from the **installer** repo root.
