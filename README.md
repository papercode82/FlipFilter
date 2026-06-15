# FlipFilter

FlipFilter is a filtering design for flow spread measurement under tight memory budgets. Compact sketches must track many flows concurrently, and their accuracy can be severely affected by numerous small-spread flows. FlipFilter mitigates this interference by suppressing small-spread flows before they enter downstream sketches.

This repository contains the C++ implementation used for evaluation, a P4-16 implementation for programmable switches, and small demo traces.

```text
FlipFilter/
+-- Cpp/                 C++ implementation
+-- P4 Implementation/   P4-16 implementation for programmable switches
+-- dataset/             Small demo traces and dataset notes
+-- README.md
```

## C++ Implementation

The `Cpp/` directory includes FlipFilter and the baseline filters used in the evaluation:

- FlipFilter
- Couper
- CouponFilter
- LogLogFilter_Spread

The C++ driver runs the experiments for per-flow spread estimation and super spreader detection with the default configuration.

## P4 Implementation

The `P4 Implementation/` directory provides a P4-16 implementation of FlipFilter on an Intel Tofino programmable switch.

## Datasets

The `dataset/` directory contains small CAIDA and StackOverflow demo traces for quick testing. Full datasets should be obtained from their official sources. See `dataset/README.md` for details.
