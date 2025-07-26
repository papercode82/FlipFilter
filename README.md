# FlipFilter

**FlipFilter** is a lightweight and memory-efficient filter designed for accurate flow spread measurement in high-speed networks. It targets the problem of filtering low-spread flows to reduce unnecessary overhead in downstream measurement tasks such as per-flow spread estimation and super spreader detection.

This repository provides a full implementation of FlipFilter in both C++ and P4, along with demo datasets to facilitate evaluation and reproducibility.

---

## Repository Structure

> FlipFilter/  
> ├── Cpp/ # C++ implementation of FlipFilter and associated experiments  
> ├── P4 Implementation/ # P4-16 implementation <br>├── dataset/ # Sample datasets and download instructions  
> └── README.md # Project documentation

---

## C++ Implementation (`Cpp/`)

This folder contains the full C++ implementation of FlipFilter, including:

- Core filter logic for FlipFilter, Couper, CouponFilter, and LogLogFilter
- Integration with downstream sketch-based estimators (e.g., vHLL, rSkt)
- Evaluation for per-flow spread estimation and super spreader detection
- Configurable memory settings and thresholds

See the `./Cpp/README.md` file for build and run instructions.

---

## P4 Implementation (`P4 Implementation/`)

We provide a P4-16 implementation of FlipFilter for deployment on programmable network switches (e.g., Barefoot Tofino ASIC).

- `FlipFilter.p4`: P4 source code for implementation

---

## Datasets (`dataset/`)

This folder provides small demo traces for testing FlipFilter on three real-world datasets. Each sample corresponds to a real scenario.