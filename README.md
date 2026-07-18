# Configurable Matrix Multiplication Accelerator

### Runtime-Configurable Floating-Point MatMul Engine in Vitis HLS, Integrated into a MicroBlaze SoC on the Nexys A7-100T

<p align="center">
  <img src="https://img.shields.io/badge/Vitis%20HLS-2023.2-blueviolet?style=for-the-badge" alt="Vitis HLS">
  <img src="https://img.shields.io/badge/Vivado-2023.2-orange?style=for-the-badge" alt="Vivado">
  <img src="https://img.shields.io/badge/Target-Nexys%20A7--100T-green?style=for-the-badge" alt="Target Board">
  <img src="https://img.shields.io/badge/Language-C%2B%2B-blue?style=for-the-badge" alt="C++">
</p>

<p align="center">
  <img src="https://img.shields.io/badge/II-1-brightgreen?style=flat-square" alt="II=1">
  <img src="https://img.shields.io/badge/Speedup-84x-red?style=flat-square" alt="Speedup">
  <img src="https://img.shields.io/badge/Interface-AXI4%20%2F%20AXI4--Lite-informational?style=flat-square" alt="AXI">
  <img src="https://img.shields.io/badge/CPU-MicroBlaze-yellow?style=flat-square" alt="MicroBlaze">
  <img src="https://img.shields.io/badge/License-MIT-lightgrey?style=flat-square" alt="License">
</p>

A floating-point matrix multiplication accelerator taken through three architectural generations in Vitis HLS 2023.2, exported as a reusable AXI IP core, and integrated into a MicroBlaze embedded system in Vivado 2023.2, targeting the Digilent Nexys A7-100T (XC7A100T).

**Result: latency reduced from 178,212 cycles (II=5) to 2,120 cycles (II=1) — a 98.8% reduction, approximately 84x.**

---

## Table of Contents

- [Project Overview](#project-overview)
- [Project Goals](#project-goals)
- [Features](#features)
- [Repository Structure](#repository-structure)
- [Project Flow](#project-flow)
- [Hardware Architecture](#hardware-architecture)
- [Version 1 — Baseline](#version-1--baseline)
- [Version 2 — Optimization Experiments](#version-2--optimization-experiments)
- [Version 3 — Final Architecture](#version-3--final-architecture)
- [Resource Utilization](#resource-utilization)
- [Version 1 vs Version 3](#version-1-vs-version-3)
- [Vivado Integration](#vivado-integration)
- [Optimization Timeline](#optimization-timeline)
- [Lessons Learned](#lessons-learned)
- [Future Work](#future-work)
- [References](#references)

---

## Project Overview

This repository documents the design, optimization, and system integration of a configurable floating-point matrix multiplication accelerator built in Vitis HLS 2023.2, packaged as a reusable AXI IP core, and integrated into a Vivado 2023.2 block design around a MicroBlaze soft processor.

| Stage | Core Idea | II | Latency (cycles) |
|-------|-----------|:---:|---:|
| Version 1 | Triple-nested loop, floating-point accumulation | 5 | 178,212 |
| Version 2 | 15 incremental optimization experiments | 5 → 1 | Varies per experiment |
| Version 3 | Fully partitioned multiplier array + reduction tree + DATAFLOW + 3x AXI masters | 1 | 2,120 |

The core finding: pragma-level tuning alone (Experiments 1–10) moved II from 5 to approximately 2. A single architectural change — replacing the linear accumulator with a balanced reduction tree (Experiment 11) — reached II=1. Architecture beat pragma tuning by a wide margin.

---

## Project Goals

| # | Goal | Status |
|---|------|:---:|
| 1 | Reduce end-to-end latency of an N x N floating-point matrix multiply | Done |
| 2 | Achieve Initiation Interval (II) = 1 on the innermost compute loop | Done |
| 3 | Preserve runtime-configurable matrix dimensions | Done |
| 4 | Export the HLS kernel as a reusable IP core | Done |
| 5 | Integrate the IP into a MicroBlaze-based embedded system | Done |
| 6 | Generate a working bitstream and validate on hardware | Done |

---

## Features

- Runtime-configurable matrix dimensions (no recompilation for different N x N sizes up to a synthesis-time bound)
- II = 1 pipelined core datapath
- IEEE-754 single-precision floating point throughout
- Hierarchical reduction-tree accumulation
- Dual interface model: AXI4 for bulk data, AXI4-Lite for control
- Reusable, versioned Vivado IP
- Full MicroBlaze SoC integration
- 15 documented optimization experiments with synthesis-level comparisons
- Checked-in `csynth` and implementation reports for every version

---

## Repository Structure

```text
matrix-mult-accelerator/
│
├── README.md
│
├── hls/
│   ├── version1_baseline/
│   │   ├── src/matmul.cpp
│   │   ├── tb/matmul_tb.cpp
│   │   ├── directives.tcl
│   │   └── reports/csynth_v1.rpt
│   │
│   ├── version2_experiments/
│   │   ├── exp01_local_buffer_a/  ... exp15_three_axi_masters/
│   │   └── SUMMARY.md
│   │
│   ├── version3_final/
│   │   ├── src/matmul_top.cpp, mult_array.cpp, reduction_tree.cpp
│   │   ├── tb/matmul_top_tb.cpp
│   │   ├── directives.tcl
│   │   └── reports/csynth_v3.rpt, export_ip/
│   │
│   └── common/golden_model.py
│
├── vivado/
│   ├── block_design/matmul_soc_bd.tcl, matmul_soc_bd.pdf
│   ├── constraints/nexys_a7_100t.xdc
│   ├── ip_repo/matmul_accel_v3_0/
│   └── bitstream/matmul_soc.bit
│
├── sw/
│   ├── microblaze_app/src/main.c
│   └── xsdb_scripts/program_and_run.tcl
│
├── docs/
└── LICENSE
```

---

## Project Flow

```text
 Algorithm Design (C++ golden model, NumPy reference)
        │
 HLS Baseline (V1)  ->  C-sim, C/RTL cosim, II=5
        │
 Iterative Optimization (V2)  ->  15 experiments, II 5 -> ~1
        │
 Final Architecture (V3)  ->  reduction tree + full partition + DATAFLOW, II=1
        │
 IP Export  ->  Vitis HLS "Export RTL" -> packaged AXI IP
        │
 Vivado Integration  ->  MicroBlaze SoC (interconnect, BRAM, clocking, reset)
        │
 Bitstream + Bring-up  ->  Synthesis -> Implementation -> Bitstream -> xsdb program
        │
 Hardware Validation  ->  MicroBlaze firmware drives AXI-Lite regs, compares output vs. software
```

---

## Hardware Architecture

```text
                         ┌────────────────────────────────────────┐
                         │            MicroBlaze SoC                │
   ┌──────────────┐      │   ┌─────────────┐      ┌──────────────┐ │
   │  MicroBlaze   │      │   │    AXI       │      │  Matrix Mult  │ │
   │  (Soft Core)  │◄────►│   │ Interconnect │◄────►│  Accelerator  │ │
   └──────┬────────┘      │   └──────┬──────┘      └──────┬───────┘ │
          │                │          │                     │        │
          │                │   ┌──────▼──────┐              │        │
          │                │   │  AXI BRAM    │◄─────────────┘        │
          │                │   │  Controller  │  (3x AXI4 Masters:    │
          │                │   └──────┬──────┘   A, B, C write)      │
          │                │          │                              │
          │                │   ┌──────▼──────┐                       │
          │                │   │ Block Memory │                       │
          │                │   │  Generator   │                       │
          │                │   └─────────────┘                       │
          │                │  AXI4-Lite: control/status/dimensions    │
          └────────────────┴──────────────────────────────────────────┘
```

| Signal Group | Protocol | Purpose |
|---|---|---|
| `s_axi_control` | AXI4-Lite | `ap_start`, `ap_done`, `ap_idle`, dimensions (N, M, K), base addresses |
| `m_axi_gmem_a` | AXI4 (Master) | Streams Matrix A |
| `m_axi_gmem_b` | AXI4 (Master) | Streams Matrix B |
| `m_axi_gmem_c` | AXI4 (Master) | Writes back Matrix C |

---

## Version 1 — Baseline

Triple-nested loop, floating-point accumulation, local buffering for both operands, single AXI4 master, bare `#pragma HLS PIPELINE` on the innermost loop.

```cpp
row: for (int i = 0; i < N; i++) {
    col: for (int j = 0; j < N; j++) {
        float acc = 0.0f;
        reduce: for (int k = 0; k < N; k++) {
            #pragma HLS PIPELINE
            acc += bufA[i][k] * bufB[k][j];   // loop-carried dependency
        }
        C[i][j] = acc;
    }
}
```

**Why II = 5:** IEEE-754 float addition is a multi-cycle operation (exponent align, mantissa add, normalize, round) resolving to approximately 5 cycles of latency in this design. Because `acc` is read and written every iteration, iteration `k+1` cannot start until iteration `k`'s addition fully drains. Total reduce-loop cost scales as `N^3 x II`.

### Version 1 Results

| Metric | Value |
|---------|------:|
| Estimated Clock | 7.3 ns |
| Latency | 178,212 cycles |
| Latency (ns) | 1.78E+06 |
| II | 5 |
| BRAM | 8 |
| DSP | 17 |
| FF | 3,627 |
| LUT | 4,129 |

At ~137 MHz, 178,212 cycles = ~1.78 ms per matrix multiply.

---

## Version 2 — Optimization Experiments

Fifteen sequential experiments, each measured against the prior baseline. Experiments 1–10 are pragma-level tuning on the V1 loop structure; Experiments 11–15 are architectural changes.

| # | Experiment | Change | II Result | Verdict |
|---|---|---|:---:|---|
| 1 | Local Buffer A | Buffer Matrix A on-chip; B still read live over AXI | II still gated by AXI latency | Partial fix — memory-bound only half solved |
| 2 | Local Buffer A+B | Buffer both operands on-chip | II = 5 (memory-independent) | Memory bottleneck fully removed; arithmetic bottleneck exposed |
| 3 | Pipeline | Apply `#pragma HLS PIPELINE` to reduce loop | II = 5 | Confirms dependency-bound floor; ~4-5x better than unpipelined |
| 4 | ARRAY_PARTITION | Partition `bufA`/`bufB` across BRAM ports | II = 5 (unchanged) | No effect — bottleneck was never memory bandwidth |
| 5 | UNROLL | Unroll reduce loop, factor = 4 | II ≈ 10 (worse) | Serializes the dependency chain within one iteration |
| 6 | ARRAY_PARTITION + UNROLL | Combine Exp. 4 + Exp. 5 | II unchanged from Exp. 5 | Confirms two pragmas solving two absent problems fix nothing |
| 7 | Separate Product Array | Multiply into array `prod[N]`, decoupled from accumulation | Multiply: II=1; Sum (naive): II=5 | Isolates the bottleneck to the reduction stage alone |
| 8 | Separate Multiply + Accumulate | Two independently pipelined stages | Multiply: II=1; Accumulate: II=5 | Quantifies: reduction stage ≈ 80%+ of total latency |
| 9 | Four Partial Accumulators | 4 independent accumulator chains, combined at the end | II ≈ 2 | First real improvement — independent chains overlap in time |
| 10 | Accumulator Banks | Parameterized bank of K accumulators, `k % K` indexing | II decreases with K, then plateaus | Final linear combine of K partial sums becomes new bottleneck |
| 11 | Balanced Reduction Tree | Binary tree reduction, every level independently pipelined | **II = 1** | Breakthrough — critical path becomes O(log N) instead of O(N) |
| 12 | Function Decomposition | Split into `multiply_array()`, `reduce_tree()`, top-level function | II = 1 (unchanged) | Enabling refactor; required precondition for DATAFLOW |
| 13 | `#pragma HLS INLINE OFF` | Prevent auto-inlining of decomposed functions | II = 1 (unchanged) | Preserves function boundaries required by DATAFLOW |
| 14 | `#pragma HLS DATAFLOW` | Overlap multiply/reduce stages across output elements | Inner II = 1 (unchanged); outer latency reduced | Solves a different axis: outer-sequence overlap, not inner II |
| 15 | Three AXI Master Interfaces | Independent `m_axi` ports for A, B, C | No HLS-level change; system-level latency reduced post-integration | Removes bus-arbitration serialization once integrated in Vivado |

### Key Numerical Findings

- Experiments 1–10 (pragma tuning): II moved from 5 to approximately 2 at best (Experiment 9).
- Experiment 11 alone (architecture change): II moved from ~2 to 1.
- `UNROLL` in isolation (Experiment 5) made II worse by roughly 2x versus baseline.
- `ARRAY_PARTITION` in isolation (Experiment 4) produced a 0% change in II.
- The reduction stage accounted for approximately 80% of total per-element latency prior to Experiment 11 (Experiment 8 measurement).

---

## Version 3 — Final Architecture

```text
 ┌───────────────────────────────────────────────────────────────────────┐
 │                          matmul_top()  [DATAFLOW region]                │
 │   m_axi_gmem_a ──►┌───────────────┐                                     │
 │   m_axi_gmem_b ──►│  Load Stage    │──┐                                │
 │                    └───────────────┘  │                                │
 │                                         ▼                                │
 │                              ┌─────────────────────┐                    │
 │                              │  Multiplier Array     │  Fully parallel  │
 │                              │  (ARRAY_PARTITION      │  N x N multiplies│
 │                              │   complete, INLINE OFF)│                  │
 │                              └──────────┬───────────┘                    │
 │                                         ▼                                │
 │                       ┌───────────────────────────────┐                 │
 │                       │   Hierarchical Reduction Tree    │  log2(N)      │
 │                       │   (balanced binary tree, II=1)   │  depth        │
 │                       └───────────────┬───────────────┘                 │
 │                                         ▼                                │
 │   m_axi_gmem_c ◄── Write-Back Stage                                     │
 │   s_axi_control (AXI4-Lite): ap_start / ap_done / N / M / K / addrs      │
 └───────────────────────────────────────────────────────────────────────┘
```

| Element | Implementation | Purpose |
|---|---|---|
| Complete ARRAY_PARTITION | `dim=1,2`, `type=complete` on operand buffers | Every element independently addressable per cycle |
| Fully parallel multiplier array | One multiply per k-index, unrolled, dependency-free | Produces all N partial products in a single cycle |
| Hierarchical reduction tree | Balanced binary tree, log2(N) levels, each pipelined | O(log N) critical path instead of O(N) |
| Three AXI4 master interfaces | Independent bundles for A, B, C | Removes bus-arbitration serialization |
| AXI4-Lite control | `s_axi_control` bundle | Exposes ap_start/ap_done + runtime N/M/K/addresses |
| Runtime-configurable dimensions | Loop bounds driven by register-mapped N/M/K, bounded by MAX_N | No recompilation needed for smaller matrix sizes |
| MicroBlaze integration | Packaged as versioned Vivado IP | Full embedded system control via bare-metal C driver |

### Version 3 Results

| Metric | Value |
|---------|------:|
| Estimated Clock | 7.923 ns |
| Latency | 2,120 cycles |
| Latency (ns) | 2.120E4 |
| II | 1 |
| BRAM | 76 |
| DSP | 169 |
| FF | 27,534 |
| LUT | 19,426 |

At ~126 MHz, 2,120 cycles = ~21.2 μs per matrix multiply.

---

## Resource Utilization

Post-implementation utilization on the Nexys A7-100T (XC7A100T, csg324-1):

| Resource | Used | Available | Utilization |
|----------|-----:|----------:|------------:|
| BRAM_18K | 76 | 270 | 28% |
| DSP | 169 | 240 | 70% |
| FF | 27,534 | 126,800 | 21% |
| LUT | 19,426 | 63,400 | 30% |
| URAM | 0 | 0 | 0% |

```text
BRAM_18K  ████████░░░░░░░░░░░░░░░░░░░  28%
DSP       ██████████████████░░░░░░░░  70%
FF        █████░░░░░░░░░░░░░░░░░░░░░  21%
LUT       ████████░░░░░░░░░░░░░░░░░░  30%
URAM      ░░░░░░░░░░░░░░░░░░░░░░░░░░   0%
```

**DSP increase (17 → 169, ~10x):** Version 1 time-multiplexed a small number of DSP-mapped multipliers across all N^3 operations. Version 3 instantiates a physically distinct multiplier per k-index, active simultaneously. The throughput gain requires a proportional increase in parallel arithmetic units — there is no way to collapse N^3 x II cycles into a log2(N)-depth, II=1 pipeline without spending DSPs on physical parallelism.

**Resource–performance tradeoff:** Latency and resource usage trade directly against each other along the parallelism axis. Version 1: minimal resources (7% DSP), maximal latency. Version 3: 70% DSP, ~84x lower latency. Resource spending only pays off when targeted at an identified bottleneck — Experiment 4's ARRAY_PARTITION added resources for 0% latency benefit; Experiment 11's reduction tree spent resources exactly where the dependency graph allowed a return.

---

## Version 1 vs Version 3

| Metric | Version 1 | Version 3 | Change |
|---|---:|---:|---:|
| Estimated Clock | 7.3 ns | 7.923 ns | +8.5% |
| Latency (cycles) | 178,212 | 2,120 | -98.8% |
| Latency (ns) | 1.78E+06 | 2.120E+04 | ~84x faster |
| II | 5 | 1 | 5x improvement |
| BRAM | 8 | 76 | +850% |
| DSP | 17 | 169 | +894% |
| FF | 3,627 | 27,534 | +659% |
| LUT | 4,129 | 19,426 | +370% |

### Latency Reduction — Calculation

```text
Version 1 latency:   178,212 cycles
Version 3 latency:     2,120 cycles

Cycles saved:        178,212 - 2,120 = 176,092

Percentage reduction:
  (178,212 - 2,120) / 178,212 x 100%  =  98.81%

Speedup factor:
  178,212 / 2,120  ≈  84.06x
```

**Summary:** ~84x latency reduction (98.8% fewer cycles) for an ~8-9x increase in DSP/FF/LUT usage. DSP utilization rose from ~7% to ~70% of the device; BRAM from ~3% to ~28%. Clock period was nearly unchanged (+8.5%), indicating the critical path remained reasonable despite full partitioning and parallelization.

---

## Vivado Integration

The Version 3 HLS kernel was exported via Vitis HLS Export RTL and instantiated in a MicroBlaze-based Vivado 2023.2 block design.

| IP Block | Role |
|---|---|
| MicroBlaze | Runs the bare-metal control application |
| AXI Interconnect | Routes AXI4/AXI4-Lite traffic between MicroBlaze, accelerator ports, and BRAM |
| AXI BRAM Controller | AXI4 slave interface onto on-chip Block RAM |
| Block Memory Generator | Underlying dual-port BRAM primitive |
| Clocking Wizard | Generates system clock domain(s) from the 100 MHz onboard oscillator |
| Processor System Reset | Synchronized reset sequencing for MicroBlaze and AXI peripherals |
| Utility Vector Logic | Bit-width adaptation/signal glue logic |
| Matrix Multiplication Accelerator IP | Version 3 core, AXI4-Lite control + 3x AXI4 masters |

**Data path:** MicroBlaze writes A and B into BRAM, then writes N/M/K and base addresses to the accelerator's AXI4-Lite control registers and asserts `ap_start`. The three AXI4 master ports independently issue burst reads/writes through the AXI Interconnect to the shared AXI BRAM Controller. The accelerator asserts `ap_done`; MicroBlaze then reads C back from BRAM.

**AXI4 vs AXI4-Lite:** AXI4 (burst-capable) is used for the three data ports because operand/result transfers are large and contiguous, amortizing address-phase overhead across many beats. AXI4-Lite is used for control because it carries small, single-beat register reads/writes — the standard HLS pairing (`m_axi` for data, `s_axilite` for control).

---

## Optimization Timeline

```text
 ══════════════════════════════════════════════════════════════════
  V1              V2 (Experiments 1-15)                        V3
 ══════════════════════════════════════════════════════════════════

 II=5 ●
      │ Exp 1  Local Buffer A            (memory: partial fix)
      │ Exp 2  Local Buffer A+B          (memory: fully solved)
      │ Exp 3  Pipeline                   II=5
      │ Exp 4  ARRAY_PARTITION            II=5   (no change)
 II=5 ┤ Exp 5  UNROLL                     II≈10  (worse)
      │ Exp 6  ARRAY_PARTITION+UNROLL     II≈10  (confirms diagnosis)
      │ Exp 7  Separate Product Array     (multiply II=1, reduce II=5)
      │ Exp 8  Separate Mult+Accumulate   (reduction ≈80% of latency)
 II≈2 ┤ Exp 9  Four Partial Accumulators  II≈2   (first real gain)
      │ Exp 10 Accumulator Banks          (combine step new bottleneck)
 II=1 ┤ Exp 11 Balanced Reduction Tree    II=1   *** breakthrough ***
      │ Exp 12 Function Decomposition     (enables DATAFLOW)
      │ Exp 13 INLINE OFF                 (preserves boundaries)
      │ Exp 14 DATAFLOW                   (outer-level overlap)
      │ Exp 15 Three AXI Masters          (system-level win, post-integration)
      ▼                                                        ▼
 178,212 cycles      [ 15 experiments ]              2,120 cycles, II=1
                                                        ~84x speedup
 ══════════════════════════════════════════════════════════════════
```

---

## Lessons Learned

| # | Lesson | Supporting Data |
|---|---|---|
| 1 | Diagnose bottleneck class before applying a pragma | Exp. 4: ARRAY_PARTITION, 0% II change |
| 2 | UNROLL on a scalar reduction can worsen II | Exp. 5: II roughly doubled |
| 3 | Float accumulation has fixed multi-cycle latency; no pragma shortens it | Exp. 1-3: II floor of 5 regardless of pipelining |
| 4 | Loop-carried dependencies need a different reduction structure, not tuning | Exp. 4-6 vs Exp. 11 |
| 5 | Balanced reduction tree: O(N) critical path becomes O(log N) | Exp. 11: II 2 → 1 |
| 6 | Partial fixes are useful stepping stones | Exp. 9-10 informed Exp. 11 |
| 7 | Local buffering must cover all operands, not one | Exp. 1 (partial) vs Exp. 2 (complete) |
| 8 | Function decomposition + INLINE OFF are silent DATAFLOW prerequisites | Exp. 12-13: no direct effect, required for Exp. 14 |
| 9 | DATAFLOW and inner-loop II are independent optimization axes | Exp. 14: inner II unchanged, outer latency reduced |
| 10 | C-sim does not model system bus contention | Exp. 15: no HLS-level change, real gain after Vivado integration |
| 11 | Resource growth is justified only against a diagnosed bottleneck | 9x DSP growth (V3) vs 0% gain (Exp. 4) |
| 12 | Runtime-configurable dimensions and full parallelism can coexist | MAX_N-sized hardware, runtime-controlled trip counts |
| 13 | AXI4 for bulk data, AXI4-Lite for control is the correct default | Standard HLS `m_axi`/`s_axilite` pairing |
| 14 | Negative results are as valuable as positive ones | Exp. 4-6 validated the diagnosis behind Exp. 11 |
| 15 | Architecture change outperforms pragma tuning | 10 experiments: II 5→2; 1 experiment: II 2→1 |

---

## Future Work

| Item | Description |
|---|---|
| Fixed-point arithmetic | Evaluate Q-format datapath to shorten accumulator latency vs. IEEE-754 float |
| AXI DMA integration | Replace CPU-orchestrated transfers with a dedicated DMA engine |
| DDR-backed operand storage | Extend beyond on-chip BRAM using onboard DDR2 with tiling/streaming |
| Systolic array architecture | Evaluate as an alternative to multiplier-array + reduction-tree for large N |
| Multiple processing elements | Replicate the II=1 core to process multiple output tiles concurrently |
| Double buffering | Overlap next-invocation operand load with current compute/write-back |

---

## References

- AMD/Xilinx, *Vitis High-Level Synthesis User Guide (UG1399)*, 2023.2.
- AMD/Xilinx, *Vivado Design Suite User Guide: Designing IP Subsystems (UG994)*, 2023.2.
- AMD/Xilinx, *AXI Reference Guide (UG1037)*.
- AMD/Xilinx, *MicroBlaze Processor Reference Guide (UG984)*.
- Digilent Inc., *Nexys A7 Reference Manual*.
- IEEE Std 754-2019, *IEEE Standard for Floating-Point Arithmetic*.

---

## License

Released under the [MIT License](LICENSE). Synthesis reports, block design exports, and bitstreams are specific to the Nexys A7-100T and Vitis/Vivado 2023.2 toolchain versions documented above.
