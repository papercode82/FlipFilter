# FlipFilter P4 Implementation

This directory provides the P4_16 source code for implementing FlipFilter on a programmable switch.

FlipFilter extends the filtering range of bitmap-based filters by enabling reversible bit flipping in shared bitmaps. The P4 implementation demonstrates that this mechanism can be mapped to programmable switch hardware.

## Target Platform

The implementation targets Intel Tofino programmable switches. It is intended to demonstrate the feasibility of realizing FlipFilter in the data plane and supporting practical line-rate deployment.
