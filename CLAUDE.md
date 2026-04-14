# mach — CLAUDE.md

Headless programmatic audio engine. C++23 core, Python 3.14 control plane via `nanobind`.

**Toolchain:** C++23 (`std::mdspan`, `std::flat_map`, `std::expected`) + Python 3.14. We are bleeding edge — use modern features, don't polyfill things the standard now provides.

**Code guidance:** Write idiomatic C++23 directly. Follow existing conventions in the codebase.

## The One Rule

**The audio thread never allocates, locks, or does I/O. Ever.**

If you are touching anything on the audio thread that is not reading from a pre-allocated region or writing to a lock-free structure, it is wrong. No exceptions.

## Architecture in One Paragraph

Python pushes commands into an SPSC queue. The audio thread pops them at block start, hands them to the EDF event scheduler, which block-splits and routes them to the DSP graph. The DSP graph evaluates a DAG of nodes each callback, reads from pre-allocated memory pools and asset buffers, writes to the hardware output buffer, pushes telemetry via relaxed atomics and triple-buffer pointer swaps, and hands dead node pointers to the Janitor thread for deferred recycling. A separate Disk I/O thread pre-loads assets and drains the recording ring buffer to disk. Python reads telemetry via zero-copy `std::mdspan` → NumPy views.

## Project Structure

```text
mach/
  src/
    engine/       # Engine init, thread management, shutdown
    queue/        # SPSC command queue
    scheduler/    # EDF event scheduler, block-splitting
    graph/        # DSP graph, DAG evaluation
    nodes/        # DSP node implementations
    memory/       # Node pool, scratch arena, asset buffers
    telemetry/    # Registry, triple-buffers, telemetry thread
    io/           # Disk I/O thread, ring buffer, format codecs
    janitor/      # Janitor thread, dead pointer queue
  python/
    mach/         # nanobind bindings, Python API surface
  tests/
  CMakeLists.txt
```

## Threads

| Thread | Priority | Blocking? |
|---|---|---|
| DSP Graph / Audio | `SCHED_FIFO` pri 99 | Never |
| Janitor | Normal | Yes |
| Disk I/O | Normal | Yes |
| Telemetry | Normal | Yes |
| Python Main | Normal | Yes |

Threads communicate exclusively through lock-free structures — SPSC queue, atomic writes, ring buffer, dead pointer queue. No mutexes anywhere in the hot path.

## Memory Regions

All regions allocated at `engine.play()`, freed at `engine.stop()` after all threads are joined.

| Region | Type | Owner |
|---|---|---|
| Node Pool | Fixed-size object pool | Janitor recycles slots |
| Scratch Arena | Bump-allocate, reset per block | Audio thread only |
| Asset Buffers | Variable slab | Disk I/O writes, audio thread reads |
| Telemetry Registry | Fixed struct | Audio thread writes, telemetry thread reads |
| Disk Write Ring Buffer | Fixed circular | Audio thread pushes, Disk I/O drains |
| SPSC Command Queue | Fixed circular | Python pushes, audio thread pops |

## Python API Shape

Nodes are opaque handles into the engine — Python holds a `NodeHandle` wrapping a `uint32_t` ID, not a real C++ object. All state lives in C++.

```python
engine = mach.Engine(sample_rate=48000, block_size=128)
osc = engine.add_node(mach.WavetableOscillator, waveform="sine")
out = engine.get_master_output()
engine.connect(osc, out)
engine.schedule(osc.set_frequency, value=440.0, time=mach.Beats(1.0))
engine.play()
```

`engine.play()` and `engine.stop()` release the GIL. Anything that serializes a command and pushes to SPSC can release the GIL after argument extraction.

## Key Conventions

- `alignas(64)` on any struct with atomic indices to prevent false sharing
- Node slot lifecycle: `Free → Acquired → Active → Abandoned → Free` (also `Acquired → Abandoned` on command queue full) — Janitor is the only thread that runs destructors
- Asset lifecycle: `Unloaded → Loading → Ready → Referenced → Ready → Unloaded` — audio thread holds read-only pointers only, never frees
- Shutdown order: stop audio callback → join Janitor + Disk I/O → join Telemetry → free all regions
- Pool exhaustion: fail-fast, raise to Python immediately — never block
- Ring buffer full: silent drop, increment drop counter in telemetry registry
- Stale `NodeHandle` after `remove_node()`: raise `NodeNotFound`, never segfault

## What's In Scope

DSP nodes (oscillators, samplers, channel strip, routing), MIDI 2.0, VST3 hosting, WAV/FLAC/MP3/OGG, WASAPI/CoreAudio/ALSA via `miniaudio`.

## What's Not

No GUI, no DAW, no destructive editing, no waveform rendering, no networking, no 32-bit, no WinMM.

## Current Phase

**Phase 1 — Proof of Concept.** Goal: sine wave out of a Jupyter cell, end to end, no audio thread allocations.

- [x] `miniaudio` device callback
- [x] SPSC queue
- [x] Single `WavetableOscillator` node from pool
- [x] `nanobind` wrapper — `Engine`, `NodeHandle`, `play()` / `stop()`
- [x] Sample-accurate `schedule()` working

## References

- [miniaudio docs](https://miniaud.io/docs/)
- [nanobind docs](https://nanobind.readthedocs.io/)
- [std::mdspan](https://en.cppreference.com/w/cpp/container/mdspan)
- [Real-time audio 101 — Ross Bencina](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing)
- `docs/design.md` — full technical design document
