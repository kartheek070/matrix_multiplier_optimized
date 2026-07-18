# ⚡ Configurable Matrix Multiplication Accelerator

### Runtime-Configurable Floating-Point MatMul Engine in Vitis HLS, Integrated into a MicroBlaze SoC on the Nexys A7-100T

<p align="center">
  <img src="https://img.shields.io/badge/Vitis%20HLS-2023.2-blueviolet?style=for-the-badge" alt="Vitis HLS">
  <img src="https://img.shields.io/badge/Vivado-2023.2-orange?style=for-the-badge" alt="Vivado">
  <img src="https://img.shields.io/badge/Target-Nexys%20A7--100T-green?style=for-the-badge" alt="Target Board">
  <img src="https://img.shields.io/badge/Language-C%2B%2B-blue?style=for-the-badge" alt="C++">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/II-1-brightgreen?style=flat-square" alt="II=1">
  <img src="https://img.shields.io/badge/Speedup-~84x-red?style=flat-square" alt="Speedup">
  <img src="https://img.shields.io/badge/Interface-AXI4%20%2F%20AXI4--Lite-informational?style=flat-square" alt="AXI">
  <img src="https://img.shields.io/badge/CPU-MicroBlaze-yellow?style=flat-square" alt="MicroBlaze">
  <img src="https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square" alt="License">
</p>

<p align="center">
  <em>A ground-up exploration of High-Level Synthesis optimization — from a naive triple-nested loop at <b>II = 5</b> to a hierarchical, fully-partitioned, reduction-tree architecture at <b>II = 1</b>, exported as reusable IP and dropped into a live MicroBlaze embedded system.</em>
</p>

---

## 📑 Table of Contents

- [Project Overview](#-project-overview)
- [Project Goals](#-project-goals)
- [Features](#-features)
- [Repository Structure](#-repository-structure)
- [Project Flow](#-project-flow)
- [Hardware Architecture](#-hardware-architecture)
- [Version 1 — Baseline Implementation](#-version-1--baseline-implementation)
- [Version 2 — The Optimization Journey](#-version-2--the-optimization-journey)
- [Version 3 — Final Architecture](#-version-3--final-architecture)
- [Resource Utilization](#-resource-utilization)
- [Version 1 vs Version 3](#-version-1-vs-version-3--the-full-comparison)
- [Vivado Integration](#-vivado-integration--microblaze-embedded-system)
- [Optimization Timeline](#-optimization-timeline)
- [Lessons Learned](#-lessons-learned)
- [Future Work](#-future-work)
- [References](#-references)

---

## 🧭 Project Overview

This repository documents the complete design, optimization, and system-integration journey of a **configurable floating-point matrix multiplication accelerator**, built using **Vitis HLS 2023.2** and packaged as a reusable AXI IP core inside a **Vivado 2023.2** block design.

The accelerator was synthesized, implemented, and validated on a **Digilent Nexys A7-100T** (Xilinx Artix-7 XC7A100T), driven by a **MicroBlaze** soft processor over a combination of **AXI4** (bulk data movement) and **AXI4-Lite** (control/status) interfaces.

What makes this project interesting isn't just the final number — it's the *path* to it. The design went through three distinct architectural generations, each one a response to a specific bottleneck exposed by HLS scheduling and dependency analysis:

| Stage | Core Idea | Initiation Interval | Latency |
|-------|-----------|:---:|---:|
| **Version 1** | Naive triple-nested loop, floating point accumulation | II = 5 | 178,212 cycles |
| **Version 2** | Fifteen incremental optimization experiments (pragma tuning → architectural rework) | II = 5 → II = 1 (partial) | Varies per experiment |
| **Version 3** | Fully partitioned multiplier array + hierarchical reduction tree + DATAFLOW + 3× AXI masters | **II = 1** | **2,120 cycles** |

The headline result: **~84× latency reduction**, achieved not by throwing more pragmas at the problem, but by recognizing that **the floating-point accumulator itself was the bottleneck**, and restructuring the datapath around that fact.

This README is written as a build log and a design-reasoning document — the goal is that another engineer picking up this repo can understand not just *what* was built, but *why* each decision was made, and *why* the obvious optimizations (`UNROLL`, `ARRAY_PARTITION` in isolation) don't get you to II=1 on their own.

---

## 🎯 Project Goals

The project was scoped around six concrete engineering objectives:

- [x] **Reduce end-to-end latency** of an `N×N` floating-point matrix multiply on FPGA fabric
- [x] **Achieve Initiation Interval (II) = 1** on the innermost compute loop
- [x] **Preserve runtime-configurable matrix dimensions** (no fixed `N` baked into the hardware)
- [x] **Export the HLS kernel as a reusable, drop-in IP core**
- [x] **Integrate the IP into a MicroBlaze-based embedded system** with proper AXI memory-mapping
- [x] **Generate a working FPGA bitstream** and validate on physical hardware (Nexys A7-100T)

Every optimization documented in Version 2 was evaluated against these goals — in particular, several techniques that looked attractive on paper (e.g., naive `UNROLL`) were rejected specifically because they violated the "runtime-configurable dimensions" constraint or because they *increased* II rather than decreasing it.

---

## ✨ Features

- 🔢 **Runtime-configurable matrix dimensions** — no recompilation needed for different `N×N` sizes (up to a synthesis-time maximum bound)
- 🌊 **II = 1 pipelined core datapath** — one MAC-tree evaluation issued every clock cycle in the final architecture
- 🧮 **IEEE-754 single-precision floating point** — full `float` arithmetic, no fixed-point shortcuts
- 🧱 **Hierarchical reduction-tree accumulation** — breaks the floating-point loop-carried dependency that capped Version 1 at II = 5
- 🔌 **Dual interface model** — AXI4 for bulk matrix data movement, AXI4-Lite for control/status register access
- 🧩 **Reusable Vivado IP** — packaged, versioned, and instantiable in any Vivado block design
- 🖥️ **MicroBlaze SoC integration** — full embedded system with interconnect, BRAM controller, clocking, and reset infrastructure
- 📊 **Fully documented optimization trail** — 15 discrete experiments, each with problem statement, theory, synthesis result, and postmortem
- 🧪 **Reproducible synthesis reports** — every version's `csynth` and implementation reports are checked into the repo

---

## 🗂️ Repository Structure

```text
matrix-mult-accelerator/
│
├── README.md                          # You are here
│
├── hls/
│   ├── version1_baseline/
│   │   ├── src/
│   │   │   ├── matmul.cpp             # Triple-nested loop, single AXI master
│   │   │   └── matmul.h
│   │   ├── tb/
│   │   │   └── matmul_tb.cpp          # Testbench w/ golden-model comparison
│   │   ├── directives.tcl             # PIPELINE-only pragma set
│   │   └── reports/
│   │       └── csynth_v1.rpt
│   │
│   ├── version2_experiments/
│   │   ├── exp01_local_buffer_a/
│   │   ├── exp02_local_buffer_ab/
│   │   ├── exp03_pipeline/
│   │   ├── exp04_array_partition/
│   │   ├── exp05_unroll/
│   │   ├── exp06_partition_unroll/
│   │   ├── exp07_separate_product_array/
│   │   ├── exp08_separate_mult_accum/
│   │   ├── exp09_four_partial_accumulators/
│   │   ├── exp10_accumulator_banks/
│   │   ├── exp11_reduction_tree/
│   │   ├── exp12_function_decomposition/
│   │   ├── exp13_inline_off/
│   │   ├── exp14_dataflow/
│   │   ├── exp15_three_axi_masters/
│   │   └── SUMMARY.md                 # Per-experiment synthesis deltas
│   │
│   ├── version3_final/
│   │   ├── src/
│   │   │   ├── matmul_top.cpp         # Top-level, DATAFLOW region
│   │   │   ├── mult_array.cpp         # Fully parallel multiplier array
│   │   │   ├── reduction_tree.cpp     # Hierarchical adder tree
│   │   │   └── matmul_top.h
│   │   ├── tb/
│   │   │   └── matmul_top_tb.cpp
│   │   ├── directives.tcl             # Full pragma set (partition/dataflow/inline)
│   │   └── reports/
│   │       ├── csynth_v3.rpt
│   │       └── export_ip/             # Solution IP export (v3.0)
│   │
│   └── common/
│       └── golden_model.py            # NumPy reference model for TB verification
│
├── vivado/
│   ├── block_design/
│   │   ├── matmul_soc_bd.tcl          # Full block design as Tcl (reproducible)
│   │   └── matmul_soc_bd.pdf          # Rendered block diagram
│   ├── constraints/
│   │   └── nexys_a7_100t.xdc
│   ├── ip_repo/
│   │   └── matmul_accel_v3_0/         # Packaged IP (from HLS export)
│   └── bitstream/
│       └── matmul_soc.bit
│
├── sw/
│   ├── microblaze_app/
│   │   ├── src/
│   │   │   └── main.c                 # Bare-metal driver + test vectors
│   │   └── bsp/
│   └── xsdb_scripts/
│       └── program_and_run.tcl
│
├── docs/
│   ├── architecture_diagrams/
│   ├── optimization_notes/
│   └── resource_reports/
│
└── LICENSE
```

---

## 🔁 Project Flow

```text
 ┌──────────────────────┐
 │  1. Algorithm Design  │   C++ golden model, NumPy reference for TB verification
 └──────────┬────────────┘
            │
 ┌──────────▼────────────┐
 │  2. HLS Baseline (V1)  │   Triple-nested loop, C-simulation, C/RTL cosim
 └──────────┬────────────┘
            │
 ┌──────────▼────────────┐
 │ 3. Iterative Optimization (V2) │  15 pragma / architecture experiments
 └──────────┬────────────┘         Each measured independently against V1 baseline
            │
 ┌──────────▼────────────┐
 │  4. Final Architecture (V3) │  Reduction tree + full partition + DATAFLOW
 └──────────┬────────────┘
            │
 ┌──────────▼────────────┐
 │  5. IP Export           │   Vitis HLS "Export RTL" → packaged AXI IP
 └──────────┬────────────┘
            │
 ┌──────────▼────────────┐
 │  6. Vivado Integration  │   MicroBlaze SoC: interconnect, BRAM, clocking, reset
 └──────────┬────────────┘
            │
 ┌──────────▼────────────┐
 │  7. Bitstream + Bring-up │  Synthesis → Implementation → Bitstream → xsdb program
 └──────────┬────────────┘
            │
 ┌──────────▼────────────┐
 │  8. Hardware Validation │   MicroBlaze firmware drives AXI-Lite control regs,
 └────────────────────────┘   compares accelerator output vs. software reference
```

Each stage fed back into the previous one. Notably, the jump from **Version 2 experiment 11 (Reduction Tree)** back to a full architectural rewrite for **Version 3** happened because incremental pragma changes on top of the V1 loop structure had a hard ceiling — the loop-carried dependency in the accumulator could not be pipelined away no matter how the surrounding loops were partitioned or unrolled. That realization is the throughline of this entire README.

---

## 🏗️ Hardware Architecture

At the system level, the accelerator sits behind a dual-interface boundary:

```text
                         ┌────────────────────────────────────────┐
                         │            MicroBlaze SoC                │
                         │                                          │
   ┌──────────────┐      │   ┌─────────────┐      ┌──────────────┐ │
   │  MicroBlaze   │      │   │    AXI       │      │  Matrix Mult  │ │
   │  (Soft Core)  │◄────►│   │ Interconnect │◄────►│  Accelerator  │ │
   │               │      │   │              │      │      IP       │ │
   └──────┬────────┘      │   └──────┬──────┘      └──────┬───────┘ │
          │                │          │                     │        │
          │                │   ┌──────▼──────┐              │        │
          │                │   │  AXI BRAM    │◄─────────────┘        │
          │                │   │  Controller  │  (3× AXI4 Masters:    │
          │                │   │              │   Matrix A, Matrix B, │
          │                │   └──────┬──────┘   Matrix C write)     │
          │                │          │                              │
          │                │   ┌──────▼──────┐                       │
          │                │   │ Block Memory │                       │
          │                │   │  Generator   │                       │
          │                │   └─────────────┘                       │
          │                │                                          │
          │                │   AXI4-Lite: control/status/dimensions   │
          │                │   (start, done, N, M, K registers)       │
          └────────────────┴──────────────────────────────────────────┘
```

**Interface contract:**

| Signal Group | Protocol | Purpose |
|---|---|---|
| `s_axi_control` | AXI4-Lite | `ap_start`, `ap_done`, `ap_idle`, matrix dimensions (`N`, `M`, `K`), base addresses |
| `m_axi_gmem_a` | AXI4 (Master) | Streams Matrix A operand from BRAM |
| `m_axi_gmem_b` | AXI4 (Master) | Streams Matrix B operand from BRAM |
| `m_axi_gmem_c` | AXI4 (Master) | Writes back result Matrix C to BRAM |

The three independent AXI masters (introduced in Version 2, Experiment 15) exist specifically to avoid serializing operand reads and result writes on a single shared bus — a decision explained in detail in the [Version 2](#-version-2--the-optimization-journey) section.

---

## 🧱 Version 1 — Baseline Implementation

### Overview

Version 1 is the "get it correct first" implementation. No aggressive pragmas, no manual restructuring — just a triple-nested loop performing floating-point matrix multiplication, with a single AXI4 master interface handling all memory traffic (both operand reads and result writes share the same bus). The goal here was functional correctness and a clean C-simulation / C-RTL cosimulation pass, establishing a known-good baseline against which every later optimization could be measured.

### Architecture

```cpp
// Simplified structure of the Version 1 kernel
void matmul_v1(float A[MAX_N][MAX_N], float B[MAX_N][MAX_N],
               float C[MAX_N][MAX_N], int N) {

    float bufA[MAX_N][MAX_N];
    float bufB[MAX_N][MAX_N];

    // Local buffering: bring operands on-chip before compute
    load_A: for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            #pragma HLS PIPELINE II=1
            bufA[i][j] = A[i][j];

    load_B: for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            #pragma HLS PIPELINE II=1
            bufB[i][j] = B[i][j];

    // Triple nested compute loop
    row: for (int i = 0; i < N; i++) {
        col: for (int j = 0; j < N; j++) {
            float acc = 0.0f;
            reduce: for (int k = 0; k < N; k++) {
                #pragma HLS PIPELINE
                acc += bufA[i][k] * bufB[k][j];   // <-- loop-carried dependency
            }
            C[i][j] = acc;
        }
    }
}
```

### Design Characteristics

- **Triple nested loops** — the canonical `for-i, for-j, for-k` matrix multiply structure, unmodified from a textbook implementation.
- **Floating-point implementation** — full IEEE-754 `float` arithmetic throughout, no fixed-point conversion.
- **Local buffering** — Matrix A and Matrix B are copied from external memory into on-chip BRAM-backed arrays (`bufA`, `bufB`) before compute begins. This avoids repeated AXI transactions inside the innermost loop, which would otherwise stall the pipeline on every iteration waiting for memory responses.
- **Single AXI Master interface** — reads for A, reads for B, and writes for C all share one `m_axi` port. This is architecturally simple but becomes a throughput bottleneck later (addressed in Version 2, Experiment 15).
- **Pipeline optimization** — a bare `#pragma HLS PIPELINE` is applied to the innermost `reduce` loop, with no explicit II target and no surrounding restructuring.

### Why Floating-Point Accumulation Creates a Loop-Carried Dependency

This is the central issue that defines the entire optimization arc of this project, so it's worth being precise about it.

The line `acc += bufA[i][k] * bufB[k][j];` looks like a single operation, but in hardware it lowers to:

```text
tmp        = bufA[i][k] * bufB[k][j]     (floating-point multiply)
acc_next   = acc + tmp                    (floating-point add)
acc        = acc_next                     (register update, fed back into next iteration)
```

The critical detail is that **iteration `k+1` cannot begin its addition until iteration `k`'s addition has produced a result**, because `acc` is both read and written every iteration. This is a **loop-carried dependency** — a true RAW (read-after-write) hazard on the accumulator register.

For *integer* accumulation, this dependency is often still pipelineable at II=1, because an integer adder has a short, fixed combinational latency (often 1 cycle) and HLS can chain single-cycle additions back-to-back.

**Floating-point addition is fundamentally different.** An IEEE-754 single-precision adder is not a 1-cycle operation — it requires:

1. Exponent comparison and alignment (mantissa shifting)
2. Mantissa addition/subtraction
3. Normalization (leading-zero detection + shift)
4. Rounding (round-to-nearest-even)

In Vitis HLS at 2023.2 default settings targeting the Artix-7 speed grade used here, this pipeline for `fadd` typically resolves to **a multi-cycle latency of around 4-5 cycles** to meet timing at a reasonable clock period. Because the accumulator feeds back into itself, **the next iteration cannot start until this multi-cycle latency has fully drained** — the adder's own pipeline cannot be used to overlap iterations, because there's nothing independent to fill those cycles with.

### Why II Remained 5

Given the above, Vitis HLS's scheduler is not being conservative or missing an optimization opportunity — it is reporting the **true achievable II given the dependency structure**: the reduce loop cannot issue a new iteration faster than the floating-point adder's feedback latency allows. In this case that resolved to **II = 5**, meaning a new `k` iteration begins only once every 5 clock cycles, even though the multiply and other logic could individually run much faster.

This single dependency is responsible for essentially all of the "wasted" cycles in Version 1. With `N` iterations of `k` per `(i,j)` pair, and `N²` pairs, the total reduce-loop cost scales as:

```text
Total cycles (reduce only) ≈ N² × N × II  =  N³ × 5
```

Every unit of II directly multiplies total latency — this is why so much of Version 2 is spent attacking this exact number.

### Explaining the Pipeline

The `#pragma HLS PIPELINE` directive instructs Vitis HLS to overlap the execution of successive loop iterations rather than executing them fully sequentially (which is the default rolled/unpipelined behavior). Without pipelining, each `k` iteration would need to fully complete — multiply, add, normalize, round, register write — before the next iteration's multiply could even begin, at massive cycle cost. Pipelining lets iteration `k+1`'s multiply begin while iteration `k`'s addition is still resolving, **up to the limit imposed by the loop-carried dependency**. That limit is exactly what produces II=5 here: the pipeline is real and functioning, it's simply bounded by the accumulator's feedback latency.

### Explaining the Local Buffers

`bufA` and `bufB` exist to decouple the compute loops from the AXI memory hierarchy. Without them, `bufA[i][k]` would instead be a live AXI read on every single iteration of the innermost loop — and AXI4 transactions carry request/response latency and arbitration overhead that is orders of magnitude larger than a BRAM read. By loading both operand matrices into on-chip block RAM up front (in their own pipelined `load_A`/`load_B` loops, both cleanly achieving II=1 since there's no data dependency between loop iterations there), the compute loops can instead issue single-cycle BRAM reads, isolating the *external* memory bottleneck from the *internal* compute bottleneck. This is a textbook HLS pattern, and it's the reason the compute loop's II is limited purely by arithmetic dependency rather than by memory latency.

### Explaining the Overall Architecture

Putting it together, Version 1's dataflow is: **stream operands from DDR/BRAM into on-chip buffers → run a triple-nested compute loop with a locally-pipelined innermost reduction → stream results back out over the same AXI master**. It is functionally complete and numerically correct, but every one of its architectural choices — the shared accumulator, the single AXI master, the unmodified loop nest — becomes a specific target in Version 2.

### Version 1 Synthesis Results

| Metric | Value |
|---------|------:|
| Estimated Clock | 7.3 ns |
| Latency | 178212 cycles |
| Latency(ns) | 1.78E+06 |
| II | 5 |
| BRAM | 8 |
| DSP | 17 |
| FF | 3627 |
| LUT | 4129 |

At a ~7.3 ns clock (≈137 MHz), a 178,212-cycle latency translates to roughly **1.78 ms** for a full matrix multiply at the tested dimension — functionally correct, but nowhere near what the fabric is capable of. That gap is the entire motivation for Version 2.

---

## 🔬 Version 2 — The Optimization Journey

Version 2 is not a single design — it's a **log of fifteen sequential experiments**, each one building on (or deliberately reacting against) the lessons of the last. This is the largest section of this README because it's the most valuable part of the repository: the dead ends are as instructive as the wins.

Each experiment below follows the same structure:

- **Problem** — what specifically was limiting performance going into this experiment
- **Why We Tried It** — the reasoning that motivated the change
- **Theory** — the HLS/hardware mechanism the change was expected to exploit
- **Expected Hardware Behavior** — what we predicted `csynth` would report
- **Actual Synthesis Result** — what actually came out of Vitis HLS
- **Why It Worked / Why It Failed** — the postmortem
- **Lesson Learned** — the one-sentence takeaway carried into the next experiment

> **Reading order matters here.** Experiments 1–10 are largely *pragma-level* tuning on top of the Version 1 loop structure. Experiments 11–15 are *architectural* changes. The transition between those two regimes — and understanding why the first regime hits a hard ceiling — is the single most important insight in this repository.

---

### 1️⃣ Local Buffer A

**Problem:** In the earliest pre-V1 draft, both operand matrices were read directly from the AXI master inside the innermost loop, causing massive per-iteration stalls.

**Why We Tried It:** Isolate one operand (Matrix A) from the AXI bus first, as an incremental, low-risk change, to measure the impact of on-chip buffering in isolation before doing it for both operands.

**Theory:** Moving `A` into a local BRAM-backed array should let the `bufA[i][k]` read resolve in a single cycle instead of an AXI round trip, reducing scheduling stalls in the reduce loop.

**Expected Hardware Behavior:** Noticeable latency reduction, but `B` still read live from AXI, so the reduce loop should still stall on `bufB[k][j]` accesses.

**Actual Synthesis Result:** Latency dropped substantially versus the un-buffered draft, but II on the reduce loop remained large and unstable — synthesis reports showed the scheduler still inserting wait states tied to the `B` operand's AXI response latency.

**Why It Worked:** Removing `A` from the AXI path did eliminate roughly half of the live-memory stalls, confirming that on-chip buffering is the correct general strategy.

**Why It Failed (to fully solve the problem):** Buffering only one of two operands is a half-measure — the loop body still contains a live AXI read for `B`, so the compute loop's achievable II is still gated by external memory latency, not by arithmetic.

**Lesson Learned:** Partial buffering gives partial results — memory-bound loops need *all* hot operands moved on-chip, not just the most obviously reused one.

---

### 2️⃣ Local Buffer A + B

**Problem:** Following on from Experiment 1, `B` was still being read live over AXI inside the compute loop.

**Why We Tried It:** Complete the buffering strategy — bring both `A` and `B` fully on-chip before compute begins, exactly as implemented in the final Version 1 baseline described above.

**Theory:** With both operands resident in BRAM, the compute loop's dependency chain should be purely arithmetic (multiply → add → feedback), with zero external memory dependency inside the hot loop.

**Expected Hardware Behavior:** The scheduler should stop reporting memory-latency-related stalls entirely; any remaining II limitation should now be attributable purely to the floating-point accumulator's feedback path.

**Actual Synthesis Result:** Confirmed — II stabilized (down to a consistent, memory-independent value), and the synthesis log's bottleneck report switched from "waiting on M_AXI response" to explicitly citing the floating-point adder's multi-cycle latency on the accumulator. This became the Version 1 baseline (II=5, table above).

**Why It Worked:** Fully decoupling the compute loop from external memory removes an entire class of nondeterministic, bus-arbitration-dependent stalls, leaving a single, well-characterized dependency to attack.

**Why It Failed (to reach II=1):** Buffering solves the *memory* bottleneck but does nothing about the *arithmetic* bottleneck — the loop-carried floating-point accumulation dependency described in the Version 1 section is completely orthogonal to where the operands live.

**Lesson Learned:** Solving the memory-bound problem reveals the compute-bound problem underneath it. This is the point where the project's real challenge — the floating point accumulator — was fully exposed for the first time.

---

### 3️⃣ Pipeline

**Problem:** With memory stalls eliminated (Experiment 2), the reduce loop was still running at a poor II with no pipelining pragma applied at all in this specific experiment's isolated test (pragma removed to measure the un-pipelined baseline for comparison).

**Why We Tried It:** Establish a clean before/after measurement of `#pragma HLS PIPELINE` in isolation, on top of the fully-buffered design, to quantify its exact contribution.

**Theory:** Pipelining should allow successive `k` iterations to overlap in the datapath, bounded only by the resource/dependency constraints of the loop body — expected to land at II=5 given the floating-point adder analysis above.

**Expected Hardware Behavior:** Latency should drop dramatically relative to the fully-rolled (non-pipelined) version, converging toward `N³ × 5` cycles rather than something closer to `N³ × (adder latency + scheduling overhead)`.

**Actual Synthesis Result:** Matched prediction almost exactly — II=5, matching the Version 1 baseline table. The non-pipelined control run (pragma removed) showed roughly 4-5× worse latency, confirming the pipeline was doing real, expected work even though it couldn't beat the dependency-imposed floor.

**Why It Worked:** `PIPELINE` correctly overlapped independent portions of successive iterations (instruction fetch/schedule/multiply of iteration k+1 while iteration k's addition is still draining), exactly as the tool is designed to do.

**Why It Failed (to reach II=1):** The pragma can only overlap what the dependency graph *allows* to be overlapped. It has no power to shorten the floating-point adder's intrinsic latency or to break a true data dependency.

**Lesson Learned:** `PIPELINE` is necessary but not sufficient — it exposes the true dependency-bound II rather than creating new parallelism out of nothing. If you want a better II, you have to change the dependency graph, not the pragma.

---

### 4️⃣ ARRAY_PARTITION

**Problem:** Even with buffering and pipelining in place, II was stuck at 5. The natural next guess: maybe the BRAM itself is the bottleneck, since a dual-port BRAM can only service two accesses per cycle.

**Why We Tried It:** `#pragma HLS ARRAY_PARTITION` on `bufA`/`bufB` splits a single BRAM array into multiple independent, smaller memories, each with its own read/write ports — a classic technique for increasing memory-side parallelism.

**Theory:** If BRAM port contention were the bottleneck, partitioning `bufA` and `bufB` (e.g., `dim=2, factor=4, cyclic`) should let the compute loop issue multiple parallel BRAM reads per cycle, potentially unblocking a lower II.

**Expected Hardware Behavior:** We expected *at least some* II improvement, since partitioning generally can't hurt when there's real memory contention.

**Actual Synthesis Result:** **No change to II whatsoever.** II remained exactly 5. LUT/FF utilization increased slightly (more, smaller memory instances), and BRAM count changed (some partitions were small enough to implement in LUTRAM/registers instead of BRAM primitives), but the reported bottleneck in the scheduler log was, verbatim, still the floating-point accumulator's dependency chain.

**Why It Failed:** This was the first clear, unambiguous signal that we were solving the wrong problem. `ARRAY_PARTITION` increases *memory bandwidth* — but the compute loop was never memory-bandwidth-bound in the first place (Experiment 2 already proved that). The bottleneck is a **true data dependency on a scalar register** (`acc`), and no amount of memory parallelism can shorten a dependency chain that doesn't touch memory at all in its critical path.

**Why This Matters:** This is the first of several "obvious" HLS optimizations in this repo that turned out to be irrelevant to this specific bottleneck. It's included deliberately, unfiltered, because it's a very common mistake: partitioning arrays is a *memory-bandwidth* lever, and pulling it does nothing for an *arithmetic-dependency* bottleneck.

**Lesson Learned:** Always identify *which class* of bottleneck you have (memory-bound vs. compute-bound vs. dependency-bound) before reaching for a pragma — `ARRAY_PARTITION` is the right tool for a different job than the one we had.

---

### 5️⃣ UNROLL

**Problem:** Still at II=5. `ARRAY_PARTITION` alone changed nothing, so the next hypothesis: maybe the loop body itself needs to do more *parallel* work per iteration to amortize the adder's latency, via `#pragma HLS UNROLL`.

**Why We Tried It:** Unrolling the reduce loop by a factor (e.g., factor=4) replicates the loop body four times per iteration, creating four independent multiply operations and, hopefully, more scheduling freedom.

**Theory:** More parallel multiplies per iteration means more instruction-level parallelism for the scheduler to exploit, which *could* help hide adder latency if the four partial products could be summed independently before joining the main accumulator.

**Expected Hardware Behavior:** Best case, a partial reduction in effective iteration count (fewer loop trips needed for the same total work) and possibly improved II if the unrolled multiplies could be scheduled off the critical accumulation path.

**Actual Synthesis Result:** **II got worse**, not better — in our unroll=4 trial, the reported II rose to roughly double the baseline, because the naive unroll still summed all four partial products *sequentially into the same scalar accumulator* (`acc += a0*b0; acc += a1*b1; ...`), which is exactly four back-to-back instances of the same loop-carried floating-point add dependency, now serialized within a single iteration instead of spread across iterations.

**Why It Failed:** Naive `UNROLL` on a loop with a scalar reduction doesn't parallelize the reduction — it just **unrolls the dependency chain into straight-line code**, and the floating-point adder's multi-cycle feedback latency now has to be paid four times in a row within one iteration instead of once per iteration. The compiler didn't gain any new parallelism; it just made the existing serial dependency chain longer and more visible.

**Why This Matters:** This is arguably the most important negative result in the whole project. It's the direct proof that **the reduction pattern itself, not the loop structure, is the problem** — and it's what motivated every subsequent experiment (7 through 11) toward restructuring *how partial products get summed*, rather than *how loops get scheduled*.

**Lesson Learned:** `UNROLL` on a scalar accumulation is not free parallelism — without also restructuring the reduction into independent partial sums, unrolling can make a loop-carried dependency *worse*, not better.

---

### 6️⃣ ARRAY_PARTITION + UNROLL

**Problem:** Both levers pulled individually had failed (Exp. 4: no effect; Exp. 5: made things worse). The obvious "brute force" next step: combine both pragmas and see if they compensate for each other's shortcomings.

**Why We Tried It:** A common HLS folk-wisdom pattern is that `UNROLL` needs `ARRAY_PARTITION` to actually deliver its benefit, since unrolled parallel accesses need parallel memory ports to avoid becoming serialized by BRAM port contention.

**Theory:** With `bufA`/`bufB` partitioned to match the unroll factor, the four unrolled multiplies could at least issue their BRAM reads in parallel, even if the reduction itself remained serial.

**Expected Hardware Behavior:** Improved *memory-side* parallelism for the unrolled multiplies, though we predicted the II would still be dominated by the same serialized-accumulator problem identified in Experiment 5.

**Actual Synthesis Result:** Confirmed the prediction: partitioning removed BRAM-port contention as a factor (visible in the scheduler log, which no longer cited memory port conflicts), but II remained at the *worse* value from Experiment 5. The bottleneck was, again, explicitly reported as the accumulator's floating-point dependency chain.

**Why It Failed:** This experiment isolated the variables cleanly: memory bandwidth was never the limiter (consistent with Experiments 2 and 4), and combining a bandwidth fix with a dependency-chain problem does not fix the dependency chain. Two pragmas solving two different problems, applied to a design with only one of those problems, unsurprisingly changes nothing about the one problem that actually exists.

**Lesson Learned:** Combining pragmas is not a substitute for correctly diagnosing the bottleneck category. If `ARRAY_PARTITION` alone didn't help and `UNROLL` alone hurt, combining them will not average out to a win — each pragma needs to be solving a problem that's actually present.

---

### 7️⃣ Separate Product Array

**Problem:** Every experiment so far confirmed the same root cause: partial products were being summed directly and serially into a single scalar accumulator. The fix has to change *how the partial products are combined*, not how they're computed or where they're stored.

**Why We Tried It:** As a first architectural step, compute all `N` partial products for a given `(i,j)` pair into a **separate array** (`float prod[N]`) first, fully decoupled from any accumulation, before doing any summation at all.

**Theory:** If the multiply stage writes into an array rather than accumulating in-place, the multiply loop itself becomes trivially parallel (no dependency between `prod[k]` and `prod[k+1]`), and it becomes possible to design the *summation* stage as a completely separate, independently-optimizable piece of hardware.

**Expected Hardware Behavior:** The multiply-only loop should pipeline at II=1 immediately, since there's no dependency between iterations at all. The subsequent summation loop (still naive, sequential `for` loop summing the array) was expected to still show the same II=5 floating-point-adder-bound behavior — but now isolated and easier to attack independently.

**Actual Synthesis Result:** Exactly as predicted. The multiply loop reported II=1 cleanly. The separate summation loop reported II=5, identical bottleneck as before — but crucially, now that bottleneck exists in an isolated ~N-iteration loop rather than being entangled with the multiply logic, giving a clean foundation for the next several experiments.

**Why It Worked:** Separating concerns — "produce N independent values" vs. "combine N values into one" — let HLS fully parallelize the part of the problem that was actually parallel (the multiplies), and left only the genuinely serial part (the reduction) as the remaining target.

**Why It's Not the Final Answer:** The reduction stage is still a naive linear scan with the same accumulator dependency — this experiment doesn't fix the core problem, it isolates it cleanly for the experiments that follow.

**Lesson Learned:** When a loop mixes independent work with dependent work, splitting them into separate loops/stages lets each be optimized on its own terms — don't try to out-pragma a loop that's secretly doing two different jobs at once.

---

### 8️⃣ Separate Multiply and Accumulate

**Problem:** Building on Experiment 7's separation, formalize the multiply stage and the accumulate stage as two distinct, independently pipelined loops (rather than one array + a follow-up naive sum), to more precisely characterize each stage's behavior.

**Why We Tried It:** Confirm that the multiply stage sustains II=1 under realistic pipelining pragmas as a *standalone* loop, and quantify the accumulate stage's II=5 cost as a standalone loop, setting a clean two-stage baseline before attempting to parallelize the accumulate stage itself.

**Theory:** With `#pragma HLS PIPELINE` applied independently to each stage, the multiply stage (embarrassingly parallel, no dependency) should freely hit II=1, while the accumulate stage remains fundamentally serial and stuck at II=5, giving a precise "cost of the reduction" figure to design against.

**Expected Hardware Behavior:** Two-stage latency roughly equal to `(N × 1) + (N × 5)` cycles per `(i,j)` output element, i.e., the reduction stage completely dominates total latency — quantifying exactly how much is on the table if the reduction could be fixed.

**Actual Synthesis Result:** Matched theory closely. Multiply stage: II=1, confirmed embarrassingly parallel. Accumulate stage: II=5, confirmed still dependency-bound, contributing roughly 5× more cycles than the multiply stage for the same number of elements.

**Why It Worked (as a diagnostic):** This experiment produced the clearest quantitative evidence yet that essentially all of the "wasted" latency in the design lived in the reduction stage specifically, not anywhere else in the datapath — motivating the shift toward tree-based reduction structures in Experiments 9–11.

**Why It's Still Not the Final Answer:** Two clean, separately-optimized loops are still two *sequential* stages — this experiment doesn't overlap them (that comes later, with DATAFLOW in Experiment 14), and it doesn't yet fix the accumulate stage's own internal serialization.

**Lesson Learned:** Quantifying where cycles actually go, stage by stage, turns "the design is slow" into "this specific 5-cycle-II reduction stage is responsible for ~80%+ of total latency" — a target precise enough to actually design against.

---

### 9️⃣ Four Partial Accumulators

**Problem:** The accumulate stage is a serial scan through N elements into one scalar register, paying the floating-point adder's feedback latency on every single addition.

**Why We Tried It:** If one accumulator has a loop-carried dependency, what if we use **four independent accumulators**, each summing every 4th element (`acc0 += prod[0], prod[4], prod[8]...`), and combine the four partial sums at the very end?

**Theory:** Four independent accumulators means four independent dependency chains that can, in principle, be scheduled to overlap — the scheduler should be able to interleave operations on `acc0`, `acc1`, `acc2`, `acc3` such that while `acc0`'s addition is draining through the adder pipeline, `acc1`'s addition (an unrelated register) can begin immediately, hiding the latency behind independent work.

**Expected Hardware Behavior:** Roughly a 4× improvement in effective throughput of the reduction stage — the four independent chains should let the scheduler achieve an II close to `5/4 ≈ 1.25`, rounding to an achievable II in the 1-2 range depending on how cleanly the interleaving scheduled.

**Actual Synthesis Result:** A genuine, measurable improvement — II dropped from 5 down to roughly 2 in this configuration (four independent chains, each still individually bound by the same adder latency, but now overlapped). This was the first experiment in the entire optimization arc to actually move the II number.

**Why It Worked:** This is the first design that correctly diagnosed and treated the actual problem: rather than trying to make one dependency chain faster (impossible — the adder's latency is fixed by IEEE-754 arithmetic requirements), it created **multiple independent dependency chains** that the scheduler could genuinely overlap in time, hiding each individual chain's latency behind the others' progress.

**Why It Still Wasn't II=1:** Four chains, each still individually gated at roughly the adder's 4-5 cycle latency, can overlap enough to approach (but not cleanly hit) II=1 with only 4-way splitting — the final combination step (`acc0+acc1+acc2+acc3`) also reintroduces a small serial tail, and scheduling four hand-written accumulators doesn't automatically balance perfectly.

**Lesson Learned:** The fix for a loop-carried dependency is not to make the dependency shorter (you usually can't) — it's to create **independent, parallel copies of the dependency** so their latencies overlap instead of stacking. This is the conceptual seed that grows into the full reduction tree in Experiment 11.

---

### 🔟 Accumulator Banks

**Problem:** Four hand-written accumulators (Experiment 9) worked, but the code was awkward, didn't generalize to runtime-configurable `N`, and the final 4-way combine was still an unstructured tail.

**Why We Tried It:** Generalize the "multiple independent accumulators" idea into a proper **bank of accumulators** — a small array of `K` accumulator registers, indexed by `k % K`, written in a loop rather than by hand, so the bank size `K` becomes a tunable parameter rather than hardcoded duplicated code.

**Theory:** A parameterized accumulator bank should let us sweep `K` (the bank size) and directly observe the II-vs-K tradeoff, rather than guessing. Larger `K` should mean more independent chains and lower II, at the cost of more registers and a larger final combine step.

**Expected Hardware Behavior:** II should continue decreasing as `K` increases, following roughly `II ≈ ceil(adder_latency / K)`, until either resource limits or the final combine step's overhead dominates.

**Actual Synthesis Result:** The sweep behaved close to theory for small-to-medium `K` (II decreased steadily from K=2 through K=8), but returns diminished sharply beyond that: increasing `K` further mostly just grew the final linear-combine tail (still a naive sequential sum of `K` partial accumulators) without further reducing steady-state II, since the combine step became the new bottleneck for large `K`.

**Why It Worked (partially):** Parameterizing the bank confirmed the "independent chains" theory quantitatively and gave a clean, reusable structure — but also exposed a **second-order version of the same problem**: the final combine of `K` partial sums is itself a scalar reduction with the exact same loop-carried-dependency issue, just at a smaller scale.

**Why It Failed (to fully solve it):** A flat bank + naive final combine just moves the bottleneck from "reduce N elements" to "reduce K elements," and a naive linear combine of K elements is still O(K) serial additions with the same adder latency per step.

**Lesson Learned:** A "bag of parallel accumulators with a naive final combine" is a partial fix — the *combine* step needs the same tree-structured thinking as the main reduction, or you've just relocated the bottleneck to a smaller version of itself. This directly motivates a fully tree-structured design in Experiment 11.

---

### 1️⃣1️⃣ Balanced Reduction Tree

**Problem:** Both the main N-element reduction and the K-element final combine (Experiment 10) suffer from the same disease: naive, linear, scalar-chained summation.

**Why We Tried It:** Replace *every* linear summation in the design — main reduction and final combine alike — with a **balanced binary reduction tree**: pairs of elements summed independently in parallel at each tree level (`sum[i] = prod[2i] + prod[2i+1]`), producing half as many partial sums per level, recursively, down to a single result.

**Theory:** A balanced tree of depth `log2(N)` means the *longest dependency chain* from input to final output is only `log2(N)` additions long, rather than `N` additions long — and critically, **every addition at a given tree level is completely independent of every other addition at that same level**, meaning an entire tree level can be scheduled to execute with full overlap, bound only by the adder's own pipeline throughput (not its latency) once the pipeline is full.

**Expected Hardware Behavior:** Because same-level additions have zero dependency on each other, we expected the *pipelined adder pattern* to reach II=1 for well-pipelined levels, with total tree latency dominated by `log2(N)` sequential *levels* (each with fixed adder latency) rather than `N` sequential *additions*. For a well-pipelined design processing many independent `(i,j)` output elements back-to-back, the steady-state II should approach 1.

**Actual Synthesis Result:** This was the breakthrough experiment. With the reduction restructured as a balanced binary tree and each tree level properly pipelined, the scheduler reported **II=1** on the overall compute datapath for the first time in the entire project. Latency per individual `(i,j)` element still reflects the `log2(N)` tree depth (an inherent, unavoidable latency), but *throughput* — a new output element's worth of computation beginning every single cycle — reached the theoretical best case.

**Why It Worked:** The tree structure directly attacks the root cause identified all the way back in Version 1: it doesn't try to make the floating-point adder faster (impossible) or hide its latency behind a fixed number of hand-rolled parallel chains (Experiment 9/10, partial fix) — it **restructures the dependency graph itself** so that the critical path length scales logarithmically instead of linearly, and every level's operations are trivially independent and therefore trivially pipelineable/overlappable across successive output elements.

**Why This Is the Best Architecture (not just "a fix"):** Every earlier experiment was, in retrospect, a partial or disguised version of this same idea — Experiment 9's four accumulators are literally the *bottom level* of a depth-2 tree; Experiment 10's accumulator bank is a tree with an unbalanced, non-tree-structured top; this experiment simply completes the pattern all the way down, and all the way up, consistently.

**Lesson Learned:** For any associative reduction (sum, max, XOR, etc.) with a latency-dominated operator, a balanced tree is the canonically correct hardware structure — not a "nice-to-have" optimization, but the structurally correct answer that every ad hoc partial fix was approximating.

---

### 1️⃣2️⃣ Function Decomposition

**Problem:** With the reduction tree solving the core arithmetic bottleneck, the design's source code had become a single large, deeply nested function — hard to reason about, and (more importantly for HLS) hard for the tool to schedule and pipeline optimally, since large monolithic functions can limit the scheduler's ability to apply region-specific optimizations.

**Why We Tried It:** Break the design into clearly-scoped functions: a `multiply_array()` function (the fully parallel multiply stage), a `reduce_tree()` function (the balanced tree), and a top-level `matmul_top()` that sequences/overlaps them — mirroring, in code structure, the architectural separation that Experiments 7 and 8 had already proven valuable conceptually.

**Theory:** Smaller, single-purpose functions give Vitis HLS cleaner scheduling boundaries, make per-function pragma application (partition, pipeline, inline) more precise and predictable, and — critically — set up the top-level function to become a `DATAFLOW` region in the next experiment, which requires clearly-separated producer/consumer functions to work correctly.

**Expected Hardware Behavior:** Little to no change in raw latency/II numbers versus Experiment 11 (same underlying architecture, just reorganized code), but improved synthesis stability and a necessary precondition for `DATAFLOW` to apply cleanly.

**Actual Synthesis Result:** Confirmed — near-identical performance numbers to Experiment 11, as expected, with the added benefit that per-function synthesis reports became far easier to read and debug, and resource attribution (which function uses how many DSPs/BRAMs) became explicit rather than needing to be inferred from a single flat report.

**Why It Worked:** This experiment's value isn't a performance number — it's an enabling refactor. Good hardware architecture and good software architecture converge here: clear separation of concerns at the C++ level maps directly onto clear separation of concerns at the RTL level.

**Lesson Learned:** Function decomposition in HLS is not just a code-cleanliness nicety — it's frequently a *hard prerequisite* for the scheduling and dataflow optimizations that matter most (as the next two experiments confirm directly).

---

### 1️⃣3️⃣ `#pragma HLS INLINE OFF`

**Problem:** Vitis HLS aggressively inlines small functions by default, which — while sometimes beneficial — can silently undo the clean functional boundaries just established in Experiment 12, collapsing everything back into one flat scheduling region and making `DATAFLOW` (the next experiment) unable to find distinct, independently-schedulable tasks.

**Why We Tried It:** Explicitly mark `multiply_array()` and `reduce_tree()` with `#pragma HLS INLINE OFF` to force Vitis HLS to keep them as **distinct hardware blocks** with their own control logic, rather than flattening them back into the caller.

**Theory:** `DATAFLOW` requires genuinely separate tasks (with their own start/done handshaking) to pipeline them at the task level; an inlined function has no such boundary, so `INLINE OFF` should be a strict precondition for `DATAFLOW` to have any effect at all.

**Expected Hardware Behavior:** No performance change on its own — this pragma's entire purpose is to *preserve structure* for a later optimization, not to optimize anything directly.

**Actual Synthesis Result:** Exactly as expected: synthesis reports confirmed `multiply_array` and `reduce_tree` now appear as distinct sub-blocks with independent `ap_start`/`ap_done` handshake signals in the generated RTL, with no meaningful change to the latency/II numbers from Experiment 12.

**Why It Worked:** It did exactly its narrow job — preserved the function-level boundaries that `DATAFLOW` needs, without which the next experiment would have silently done nothing (a common, hard-to-diagnose HLS pitfall, since a flattened design produces valid but non-overlapped RTL with no error or warning).

**Lesson Learned:** Some pragmas exist purely to *protect a precondition* for a later optimization rather than to optimize anything themselves — `INLINE OFF` is invisible in isolation but essential in combination, and skipping it is a classic way to have `DATAFLOW` silently fail to help.

---

### 1️⃣4️⃣ `#pragma HLS DATAFLOW`

**Problem:** Even with clean, non-inlined function boundaries (Experiment 13), `multiply_array()` and `reduce_tree()` were still executing **sequentially** at the top level for each `(i,j)` output — the multiply stage had to fully finish before the reduce stage could begin, for every element.

**Why We Tried It:** Apply `#pragma HLS DATAFLOW` at the top-level function, instructing Vitis HLS to pipeline execution **across function calls**, allowing `reduce_tree()` for output element `n` to begin operating on partial results while `multiply_array()` is already working on producing data for element `n+1`.

**Theory:** `DATAFLOW` builds a task-level pipeline using automatically-inferred FIFOs/ping-pong buffers between producer and consumer functions, so that instead of `total_time ≈ (multiply_time + reduce_time) × num_elements`, steady-state throughput approaches `total_time ≈ max(multiply_time, reduce_time) × num_elements` (with a fixed pipeline fill/drain overhead at the very start/end).

**Expected Hardware Behavior:** Meaningful latency reduction at the *top level* (across the full multi-element computation), though we did not expect `DATAFLOW` to change the *inner* per-element II established in Experiment 11 — the reduction tree's II=1 achievement was already an inner-loop-level result, and `DATAFLOW`'s job is purely to overlap the outer sequence of function calls around it.

**Actual Synthesis Result:** Confirmed precisely — top-level latency dropped meaningfully (consistent with overlapping multiply and reduce stages across the outer element sequence), while the *inner* II of both `multiply_array` and `reduce_tree` individually stayed exactly at their Experiment 11 values. Synthesis reports also confirmed automatically-inferred ping-pong buffers between the two functions, as expected for `DATAFLOW`-connected array-typed interfaces.

**Why It Worked:** `DATAFLOW` is solving a genuinely different problem than every previous experiment — all of Experiments 1–11 were about the **inner loop's** II; `DATAFLOW` is about the **outer sequence's** overlap. It's not competing with the reduction tree fix, it's stacking on top of it.

**Why It Only Helped at the Top Level (and not inside):** `DATAFLOW` pipelines *calls between functions* — it has no visibility into, and no effect on, the dependency structure *inside* a function's own loops. The reduction tree's II=1 was already the best achievable inner-loop result; `DATAFLOW` simply stops that already-optimal inner behavior from being needlessly serialized at the level above it.

**Lesson Learned:** Inner-loop optimization (fixing II) and outer-sequence optimization (`DATAFLOW`) are two different axes of the same design and need to be solved separately, in the right order — `DATAFLOW` applied to a design with a bad inner II just pipelines a slow stage faster than it pipelines a fast one, and won't fix the inner bottleneck itself.

---

### 1️⃣5️⃣ Three AXI Master Interfaces

**Problem:** With the compute datapath fully optimized (II=1 inner loop, `DATAFLOW`-overlapped outer stages), the single shared AXI master from Version 1 (used for reading A, reading B, and writing C) became the new limiting factor — all three data streams competing for one bus's arbitration and bandwidth.

**Why We Tried It:** Split the single `m_axi` interface into **three independent AXI4 master ports** — one dedicated to streaming Matrix A, one dedicated to streaming Matrix B, and one dedicated to writing back Matrix C — via `#pragma HLS INTERFACE m_axi` applied three times with distinct bundle names.

**Theory:** With physically separate AXI master ports, reads of A, reads of B, and writes of C can all proceed **concurrently** at the system level (each with its own address/data channels), rather than being serialized through shared arbitration logic on a single bus — directly complementing the `DATAFLOW`-based overlap established in Experiment 14, since overlapped compute stages are only useful if their memory traffic can also proceed concurrently.

**Expected Hardware Behavior:** Reduced stalling at the system boundary, particularly visible once the design is integrated into the full Vivado block design where actual bus contention with the MicroBlaze and other system traffic becomes observable (rather than in isolated C-simulation, which doesn't model system-level bus contention).

**Actual Synthesis Result:** At the HLS level, resource utilization increased modestly (more independent AXI infrastructure, more DSPs/LUTs for address generation logic per port) but II and per-kernel latency numbers were unaffected, as expected, since C-simulation and cosimulation don't model shared-bus contention. The real payoff was measured after Vivado integration, where the three-master configuration measurably reduced end-to-end runtime versus a single-master version of the same core, particularly for larger matrix sizes where operand streaming and result write-back genuinely overlap in time with the compute-bound `DATAFLOW` pipeline.

**Why It Worked:** This experiment correctly recognized that once the *compute* datapath is no longer the bottleneck (thanks to Experiments 11 and 14), the **system-level memory architecture** becomes the next constraint, and that constraint lives outside of what C-simulation alone can reveal — it required actual system integration to validate.

**Lesson Learned:** HLS-level `csynth` numbers (II, latency) are necessary but not sufficient for judging overall system performance — once a kernel's internal datapath is optimized, the memory interface architecture around it needs the same level of scrutiny, and some of that scrutiny only pays off once the kernel is dropped into a real bus fabric with real contention.

---

### 📌 Version 2 — Synthesized Conclusions

A few cross-cutting observations, having now walked through all fifteen experiments:

1. **`ARRAY_PARTITION` alone did not improve latency** (Experiment 4) because the bottleneck was never memory bandwidth — it was a scalar arithmetic dependency. Partitioning is the correct fix for BRAM-port contention, and BRAM-port contention was never present in this design once operands were locally buffered (Experiment 2).

2. **`UNROLL` increased II** (Experiment 5) because unrolling a loop that reduces into a single scalar doesn't create parallelism — it serializes the same dependency chain into a longer straight-line sequence, paying the floating-point adder's multi-cycle latency more times per iteration, not fewer.

3. **Multiple accumulators still had dependency** (Experiments 9–10) because each individual accumulator is still a linear chain, and the *final combine* of several partial accumulators is itself a smaller instance of the exact same problem — parallelism at one level doesn't remove serialization at the level above it unless applied recursively.

4. **The reduction tree became the best architecture** (Experiment 11) because it's the only structure that makes the *critical path length* itself logarithmic rather than linear, while keeping every operation at each tree level fully independent and thus fully pipelineable — it's not a bigger version of the accumulator-bank idea, it's the complete, correctly-recursive version of it.

5. **`DATAFLOW` only helped at the top level** (Experiment 14) because it operates on the *sequence of function calls*, not on the dependency structure *inside* a function — it's a complementary axis of optimization to the reduction tree, not a substitute for it, and applying it before fixing the inner II would have simply pipelined a bottlenecked stage.

6. **Architecture optimization is more important than pragma tuning.** Experiments 1–10 span roughly the first two-thirds of this optimization log and, despite real engineering effort, only moved II from 5 to about 2 in the best case (Experiment 9). Experiment 11 alone — a single architectural rewrite of the reduction pattern — is what actually reached II=1. Pragmas can expose or fail to expose the parallelism that's *already present* in a given code structure; they cannot manufacture parallelism that the underlying algorithm's dependency graph doesn't have. Get the architecture right first.

---

## 🚀 Version 3 — Final Architecture

Version 3 is the synthesis of every lesson from Version 2, assembled into a single, coherent, production-quality kernel — not a grab-bag of pragmas, but a deliberately structured architecture.

### Final Architecture Diagram

```text
 ┌───────────────────────────────────────────────────────────────────────┐
 │                          matmul_top()  [DATAFLOW region]                │
 │                                                                         │
 │   m_axi_gmem_a ──►┌───────────────┐                                     │
 │                    │  Load Stage A  │──┐                                │
 │   m_axi_gmem_b ──►│  Load Stage B  │──┤                                │
 │                    └───────────────┘  │                                │
 │                                         ▼                                │
 │                              ┌─────────────────────┐                    │
 │                              │  Multiplier Array     │  Fully parallel  │
 │                              │  (ARRAY_PARTITION:    │  N×N multiplies  │
 │                              │   complete, dim=1&2)  │  INLINE OFF      │
 │                              └──────────┬───────────┘                    │
 │                                         ▼                                │
 │                       ┌───────────────────────────────┐                 │
 │                       │   Hierarchical Reduction Tree    │  log2(N)      │
 │                       │   (balanced binary adder tree,   │  depth,       │
 │                       │    every level fully pipelined)  │  II=1 steady  │
 │                       └───────────────┬───────────────┘                 │
 │                                         ▼                                │
 │                              ┌─────────────────────┐                    │
 │                              │   Write-Back Stage    │                   │
 │                              └──────────┬───────────┘                    │
 │                                         │                                │
 │   m_axi_gmem_c ◄───────────────────────┘                                │
 │                                                                         │
 │   s_axi_control (AXI4-Lite): ap_start / ap_done / N / M / K / base addrs │
 └───────────────────────────────────────────────────────────────────────┘
```

### Key Architectural Elements

| Element | Implementation | Purpose |
|---|---|---|
| **Complete `ARRAY_PARTITION`** | `dim=1` and `dim=2`, `type=complete` on operand buffers | Every element independently addressable in the same cycle — required to feed a fully parallel multiplier array with zero port contention |
| **Fully parallel multiplier array** | One DSP-mapped multiply per `(k)` index, generated via unrolled, dependency-free loop over pre-partitioned operands | Produces all `N` partial products for a given output element in a single cycle, with no serialization |
| **Hierarchical reduction tree** | Balanced binary adder tree, `log2(N)` levels, each level pipelined independently | Collapses `N` partial products into one sum with `O(log N)` critical-path depth instead of `O(N)` |
| **Three AXI4 master interfaces** | Independent `m_axi` bundles for A, B, C | Removes bus-arbitration serialization between operand reads and result writes |
| **AXI4-Lite control** | `s_axi_control` bundle, auto-generated by HLS `INTERFACE` pragma | Exposes `ap_start`/`ap_done`/`ap_idle` plus memory-mapped registers for runtime `N`/`M`/`K` and base addresses |
| **Runtime-configurable dimensions** | Loop bounds driven by register-mapped `N`/`M`/`K` inputs, bounded by a synthesis-time `MAX_N` for buffer sizing | No recompilation needed to run a different (smaller-or-equal) matrix size |
| **MicroBlaze integration** | Packaged as versioned Vivado IP, instantiated in a MicroBlaze block design | Full embedded system control via bare-metal C driver over AXI4-Lite |

### Why Runtime-Configurable Dimensions Still Work at II=1

A natural question: doesn't a fully-partitioned, fully-parallel multiplier array require a *fixed* matrix size, since the hardware is physically replicated `MAX_N × MAX_N` times? The answer is that the hardware is sized for the **maximum** supported dimension (`MAX_N`), and the runtime `N`/`M`/`K` registers control **loop trip counts and address generation**, not the physical replication factor. For matrix sizes smaller than `MAX_N`, unused rows/columns of the multiplier array and reduction tree simply go unused for that invocation (with control logic gating which partial products are valid), preserving both configurability and the II=1 steady-state behavior for the active region.

### Version 3 Synthesis Results

| Metric | Value |
|---------|------:|
| Estimated Clock | 7.923 ns |
| Latency | 2120 cycles |
| Latency(ns) | 2.120E4 |
| II | 1 |
| BRAM | 76 |
| DSP | 169 |
| FF | 27534 |
| LUT | 19426 |

At a 7.923 ns clock (≈126 MHz), 2,120 cycles resolves to roughly **21.2 μs** — down from Version 1's 1.78 ms, for the same computational task.

---

## 📊 Resource Utilization

Post-implementation utilization on the Nexys A7-100T (XC7A100T, `csg324-1`):

| Resource | Used | Available | Utilization |
|----------|-----:|----------:|------------:|
| BRAM_18K | 76 | 270 | 28% |
| DSP | 169 | 240 | 70% |
| FF | 27534 | 126800 | 21% |
| LUT | 19426 | 63400 | 30% |
| URAM | 0 | 0 | 0% |

```text
BRAM_18K  ████████░░░░░░░░░░░░░░░░░░░  28%
DSP       ██████████████████░░░░░░░░  70%
FF        █████░░░░░░░░░░░░░░░░░░░░░  21%
LUT       ████████░░░░░░░░░░░░░░░░░░  30%
URAM      ░░░░░░░░░░░░░░░░░░░░░░░░░░   0%
```

### Why DSP Usage Increased

Version 1 used only 17 DSP slices; Version 3 uses 169 — roughly a **10× increase**. This is a direct, expected consequence of the architectural shift from Version 2's Experiment 12 onward: Version 1 time-multiplexed a small number of DSP-mapped floating-point multipliers across all `N³` multiply operations sequentially (one or a few DSPs, reused every cycle across many loop iterations), whereas Version 3's **fully parallel multiplier array** instantiates a physically distinct multiplier per `(k)` index for a given output element, all active simultaneously. Trading a small number of *time-multiplexed* DSPs for a larger number of *spatially parallel* DSPs is precisely the mechanism that collapses `N³ × II` cycles down to a `log2(N)`-depth, II=1 pipeline — there is no way to achieve that throughput increase without a proportional increase in physically parallel arithmetic units. At 70% DSP utilization, this design is now approaching (but not exceeding) the practical ceiling for this specific device — a meaningful data point for anyone considering scaling `MAX_N` further on this same part.

### Resource–Performance Tradeoff

This project is a clean illustration of the fundamental HLS tradeoff space: **latency and resource usage trade against each other along the parallelism axis.** Version 1 sits at one extreme (minimal resources, maximal latency — a mostly time-multiplexed design); Version 3 sits much further toward the other extreme (substantially more silicon, ~84× lower latency — a mostly spatially-parallel design). The DSP and FF/LUT growth in Version 3 isn't overhead or waste — it's the literal, physical cost of turning sequential time-steps into concurrent hardware, and every one of the Version 2 experiments that *didn't* improve II (Experiments 4, 5, 6) is instructive precisely because they show that **spending resources without changing the dependency structure buys nothing** — Experiment 4's `ARRAY_PARTITION` alone added resources with zero latency benefit, while Experiment 11's reduction tree spent resources exactly where the dependency graph allowed them to pay off. The lesson threaded through the entire Version 2 log is that resource spending is only profitable when it's spent on breaking a real, identified bottleneck — not applied reflexively.

---

## ⚖️ Version 1 vs Version 3 — The Full Comparison

| Metric | Version 1 (Baseline) | Version 3 (Final) | Change |
|---|---:|---:|---:|
| Estimated Clock | 7.3 ns | 7.923 ns | +8.5% (slightly relaxed to accommodate a wider, more parallel datapath) |
| Latency (cycles) | 178,212 | 2,120 | **−98.8%** |
| Latency (ns) | 1.78 × 10⁶ | 2.120 × 10⁴ | **≈ 84× faster** |
| II | 5 | 1 | **5× improvement in steady-state throughput** |
| BRAM | 8 | 76 | +68 (+850%) |
| DSP | 17 | 169 | +152 (+894%) |
| FF | 3,627 | 27,534 | +23,907 (+659%) |
| LUT | 4,129 | 19,426 | +15,297 (+370%) |

### Latency Reduction, Worked Out

```text
Version 1 latency:   178,212 cycles
Version 3 latency:     2,120 cycles

Reduction:  178,212 − 2,120  =  176,092 cycles saved

Percentage reduction:
  (178,212 − 2,120) / 178,212 × 100%  =  98.81%

Speedup factor:
  178,212 / 2,120  ≈  84.06×
```

**Headline result: a ~84× reduction in cycle-count latency, i.e., a 98.8% reduction in total cycles, achieved for roughly an 8–9× increase in DSP/FF/LUT resource usage.** Given that Version 1 was using only a small fraction of the Nexys A7-100T's fabric to begin with (17 of 240 DSPs, 8 of 270 BRAM_18K blocks), Version 3's larger footprint (169/240 DSP, 76/270 BRAM) is a well-justified trade: the design went from using ~7% of available DSPs to ~70%, in exchange for two orders of magnitude less latency — a trade that would be difficult to argue against for any application where throughput matters more than fitting multiple accelerator instances on one device.

### Per-Metric Commentary

- **Clock period** barely moved (7.3 ns → 7.923 ns, a ~9% relaxation) despite the dramatically larger, more parallel datapath — a strong signal that the design's critical path remained reasonable even after full partitioning and parallelization, rather than degrading as fanout and routing congestion typically do in a naively-parallelized design.
- **II** improved 5×, which multiplies directly with **latency-per-element**, and combined with the reduction tree's shallow `log2(N)` depth (rather than Version 1's `N`-deep serial chain), produces the compounding ~84× total latency improvement.
- **BRAM** grew moderately (8 → 76) — largely a consequence of full `ARRAY_PARTITION` converting what was one or a few wide BRAM instances into many narrower, independently-addressable memories (some implemented in BRAM primitives, some in distributed RAM depending on partition granularity).
- **DSP** grew the most in relative terms (17 → 169, ~10×) — this is the direct, expected cost of moving from a time-multiplexed to a spatially-parallel multiplier array, as discussed above.

---

## 🔧 Vivado Integration — MicroBlaze Embedded System

### Embedded System Overview

The Version 3 HLS kernel was exported via **Vitis HLS → Solution → Export RTL → Vivado IP** and imported into a Vivado 2023.2 IP repository, then instantiated inside a full MicroBlaze-based block design targeting the Nexys A7-100T.

### Block Design Components

| IP Block | Role |
|---|---|
| **MicroBlaze** | Soft-core RISC processor; runs the bare-metal control application that configures and triggers the accelerator |
| **AXI Interconnect** | Routes AXI4 and AXI4-Lite traffic between MicroBlaze, the accelerator's three master ports, and memory-mapped peripherals |
| **AXI BRAM Controller** | Provides an AXI4 slave interface onto on-chip Block RAM, used as the working memory for matrices A, B, and C |
| **Block Memory Generator** | Underlying dual-port BRAM primitive instantiated behind the AXI BRAM Controller |
| **Clocking Wizard** | Generates the system clock domain(s) from the Nexys A7's 100 MHz onboard oscillator, including the clock feeding the accelerator IP |
| **Processor System Reset** | Generates synchronized, properly-sequenced reset signals for MicroBlaze and all AXI-connected peripherals |
| **Utility Vector Logic** | Small glue logic (bit-width adaptation, signal inversion) needed to connect heterogeneous IP interfaces cleanly |
| **Matrix Multiplication Accelerator IP** | The Version 3 HLS-exported core, instantiated with its AXI4-Lite control port and three AXI4 master ports |

### How the IP Communicates with BRAM

The accelerator IP does not have direct point-to-point BRAM ports — instead, all of its bulk data movement goes through the **AXI Interconnect**, which arbitrates access to a shared **AXI BRAM Controller** sitting in front of the **Block Memory Generator**. Concretely:

1. MicroBlaze writes Matrix A and Matrix B into designated BRAM address ranges over its own data-side AXI connection.
2. MicroBlaze writes the matrix dimensions (`N`, `M`, `K`) and base addresses into the accelerator's AXI4-Lite control registers, then asserts `ap_start` via the same interface.
3. The accelerator's three independent AXI4 master ports (`m_axi_gmem_a`, `m_axi_gmem_b`, `m_axi_gmem_c`) each issue burst read/write transactions through the AXI Interconnect to the shared AXI BRAM Controller, reading A and B and eventually writing C — with the Interconnect arbitrating concurrent access from all three ports plus MicroBlaze's own memory traffic.
4. The accelerator asserts `ap_done` on its AXI4-Lite interface upon completion; MicroBlaze polls (or, in an interrupt-driven variant, is notified) and then reads Matrix C back out of BRAM for verification against the software reference.

### Why AXI4 vs. AXI4-Lite

**AXI4** (full, burst-capable) is used for the three data-movement ports because matrix operands and results are large, contiguous, high-bandwidth transfers that benefit substantially from AXI4's burst transaction support (`AxLEN`, `AxSIZE`, `AxBURST`), amortizing per-transaction address-phase overhead across many beats of data. **AXI4-Lite** is used for the control interface because it carries small, single-beat, register-style reads and writes (start/done/status/dimension registers) where burst support would add complexity for no benefit — this is the standard, HLS-recommended interface pairing (`m_axi` for bulk data, `s_axilite` for control) and matches the pattern used throughout the Xilinx/AMD HLS IP ecosystem.

---

## 🕒 Optimization Timeline

```text
 ════════════════════════════════════════════════════════════════════════════
  VERSION 1                VERSION 2 (Experiments 1-15)              VERSION 3
 ════════════════════════════════════════════════════════════════════════════

 II=5 ●
      │
      │  Exp 1 ●  Local Buffer A               (memory: partial fix)
      │        │
      │  Exp 2 ●  Local Buffer A+B              (memory: fully solved)
      │        │
      │  Exp 3 ●  Pipeline                       II=5   (arithmetic bound exposed)
      │        │
      │  Exp 4 ●  ARRAY_PARTITION                II=5   (no change — wrong lever)
      │        │
 II=5 ┤  Exp 5 ●  UNROLL                          II↑    (got WORSE — dependency chain grows)
      │        │
      │  Exp 6 ●  ARRAY_PARTITION + UNROLL        II↑    (still worse — confirms diagnosis)
      │        │
      │  Exp 7 ●  Separate Product Array          (multiply II=1, reduce still II=5, isolated)
      │        │
      │  Exp 8 ●  Separate Multiply + Accumulate  (quantified: reduction = ~80% of latency)
      │        │
 II≈2 ┤  Exp 9 ●  Four Partial Accumulators       II≈2   ★ first real breakthrough
      │        │
      │  Exp 10 ● Accumulator Banks               (parameterized — combine step now bottleneck)
      │        │
 II=1 ┤══Exp 11 ●═ Balanced Reduction Tree ════════II=1══ ★★★ THE breakthrough architecture
      │        │
      │  Exp 12 ● Function Decomposition          (refactor — enables DATAFLOW)
      │        │
      │  Exp 13 ● INLINE OFF                       (preserves boundaries for DATAFLOW)
      │        │
      │  Exp 14 ● DATAFLOW                         (outer-level overlap on top of II=1 core)
      │        │
      │  Exp 15 ● Three AXI Masters                (system-level bandwidth, post-integration win)
      │        │
      ▼        ▼                                              ▼
 178,212     [ 15 experiments, ~2 weeks of iteration ]      2,120 cycles
  cycles                                                     II = 1
                                                            ~84× speedup
 ════════════════════════════════════════════════════════════════════════════
```

---

## 🎓 Lessons Learned

1. **Diagnose the bottleneck class before reaching for a pragma.** Memory-bound, compute-bound, and dependency-bound problems have completely different fixes — `ARRAY_PARTITION` fixing nothing in Experiment 4 was the clearest possible signal that memory bandwidth was never the issue.

2. **`UNROLL` on a scalar reduction can make things worse, not better.** Experiment 5 is proof that "more parallelism pragmas" isn't a monotonically-safe strategy — unrolling a dependency chain just makes the chain longer within one iteration.

3. **Floating-point accumulation has a real, physical, multi-cycle latency** (exponent alignment, mantissa add, normalization, rounding) that no scheduling pragma can shrink — the only lever available is restructuring *how many independent chains* exist.

4. **A loop-carried dependency needs a structurally different reduction, not a faster adder.** The fix for II=5 was never going to be found by tuning existing code around the same linear accumulator — it required replacing the accumulator with a tree.

5. **A balanced binary reduction tree turns an O(N) critical path into an O(log N) critical path** — this is the single highest-leverage change in the entire project, worth more than all ten prior pragma experiments combined.

6. **Partial fixes (multiple accumulators, accumulator banks) are valuable stepping stones**, not dead ends — Experiments 9 and 10 didn't reach II=1, but they built the conceptual and empirical foundation that made Experiment 11 an obvious next step rather than a leap of faith.

7. **Local buffering must be complete, not partial.** Buffering only Matrix A (Experiment 1) left the design memory-bound on Matrix B; both operands needed to move on-chip before the arithmetic bottleneck could even be measured cleanly.

8. **Function decomposition and `INLINE OFF` are invisible-until-combined optimizations.** Neither changes performance on its own, but skipping either one silently prevents `DATAFLOW` from having any effect — a failure mode with no error message, only a suspiciously unchanged latency number.

9. **`DATAFLOW` and inner-loop II are orthogonal axes.** Fixing one doesn't fix the other, and applying `DATAFLOW` before fixing a bad inner II just pipelines a bottleneck faster without removing it.

10. **C-simulation and cosimulation don't model system-level bus contention.** Experiment 15's three-AXI-master change showed no HLS-level latency change in isolation — its value only became visible after real Vivado integration, a reminder that `csynth` numbers are a necessary but incomplete picture of true system performance.

11. **Resource growth is only justified when it's spent on a diagnosed bottleneck.** Version 3's ~9× DSP growth bought an 84× latency win; Experiment 4's `ARRAY_PARTITION`-only resource growth bought nothing — the same category of "spend more silicon" produced opposite outcomes depending on whether it targeted the real dependency.

12. **Runtime-configurable dimensions and full parallelism are not mutually exclusive**, provided the hardware is sized for a maximum bound (`MAX_N`) and runtime registers control trip counts/addressing rather than physical replication — a distinction worth being explicit about when a stakeholder assumes "fully parallel" must mean "fixed size."

13. **AXI4 for bulk data, AXI4-Lite for control is the correct default pairing**, not an arbitrary style choice — burst-capable AXI4 amortizes address-phase overhead across large operand transfers, while AXI4-Lite's simplicity is exactly right for infrequent, single-beat register access.

14. **The most valuable documentation is the negative results.** Experiments 4, 5, and 6 "failed" by the metric of moving II, but they're the reason Experiment 11's architecture was recognized as correct rather than being one more untested guess — a build log that only records what worked would have made this project harder to trust, not easier.

15. **Architecture beats pragma-tuning, every time this project tested it head-to-head.** Ten experiments of careful, individually well-reasoned pragma application moved II from 5 to ~2 at best. One correctly-diagnosed architectural rewrite moved it to 1. That ratio is the single biggest takeaway of this entire repository.

---

## 🔮 Future Work

- [ ] **Fixed-point arithmetic exploration** — evaluate a `Q-format` fixed-point datapath as an alternative to IEEE-754 float, trading numerical range/precision for a shorter (or fully combinational) accumulator latency, potentially reducing the DSP/LUT cost of the reduction tree for applications that can tolerate reduced precision.
- [ ] **AXI DMA integration** — replace polled, CPU-orchestrated AXI BRAM Controller transfers with a dedicated AXI DMA engine, freeing MicroBlaze from managing bulk data movement and enabling scatter-gather transfers directly from a host or from larger backing memory.
- [ ] **DDR-backed operand storage** — extend beyond on-chip BRAM to the Nexys A7's onboard DDR2, enabling matrix dimensions well beyond what block RAM alone can hold, with an accompanying tiling/streaming strategy to keep the II=1 reduction tree fed.
- [ ] **Systolic array architecture** — investigate a systolic-array-based matrix multiplier as an alternative to the current multiplier-array-plus-reduction-tree structure, particularly for very large `N` where data reuse patterns could reduce off-chip bandwidth pressure further.
- [ ] **Multiple processing elements (Multi-PE)** — replicate the II=1 core datapath itself (not just its internal multiplier array) to process multiple independent `(i,j)` output tiles concurrently, trading further DSP/BRAM budget for additional throughput beyond a single core's II=1 ceiling.
- [ ] **Double buffering (ping-pong operand loading)** — overlap the next invocation's operand load with the current invocation's compute/write-back stage, removing the fixed load-stage latency currently paid at the start of every kernel invocation, particularly valuable if the kernel is called repeatedly in a loop from software.

---

## 📚 References

- AMD/Xilinx, *Vitis High-Level Synthesis User Guide (UG1399)*, 2023.2 release documentation.
- AMD/Xilinx, *Vivado Design Suite User Guide: Designing IP Subsystems (UG994)*, 2023.2 release documentation.
- AMD/Xilinx, *AXI Reference Guide (UG1037)* — AXI4, AXI4-Lite, and AXI4-Stream protocol specifications as implemented in Xilinx IP.
- AMD/Xilinx, *MicroBlaze Processor Reference Guide (UG984)*.
- Digilent Inc., *Nexys A7 Reference Manual* — board-level clocking, memory, and I/O documentation for the XC7A100T target.
- IEEE Std 754-2019, *IEEE Standard for Floating-Point Arithmetic* — basis for the single-precision arithmetic behavior discussed throughout the Version 1 and Version 2 sections.

---

## 📄 License

This project is released under the [MIT License](LICENSE). Synthesis reports, block design exports, and bitstreams included in this repository are provided for reference and reproducibility; they are specific to the Nexys A7-100T and Vitis/Vivado 2023.2 toolchain versions documented above.

---

<p align="center">
  <sub>Built with Vitis HLS 2023.2 and Vivado 2023.2 · Targeting the Digilent Nexys A7-100T · Documented the way it was actually debugged</sub>
</p>
