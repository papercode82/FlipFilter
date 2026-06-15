# C++ Implementation

This directory contains the C++ implementation of FlipFilter.

## Requirements

- A C++17-compatible compiler
- CMake 3.10 or later

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

On Windows, you can also open this folder in CLion and build it directly from the IDE.

## Run

After building, run the generated executable from the build directory:

```bash
./FlipFilter_Cpp
```

On Windows, run the generated `.exe` file from the build directory.

The default `main.cpp` runs the experiments on the demo traces provided in `dataset/`.
It writes CSV results to the output directory configured in `main.cpp`.

## Dataset Notes

The dataset files in this repository are only small demo traces.
They are meant to verify that the code compiles and runs correctly, not to serve as the full experimental datasets.

To test with your own data, you should either:

- modify the dataset file paths in `main.cpp`, or
- add your own dataset loading function for your custom file format.

For example, you can replace the demo path in the corresponding loader and keep the rest of the pipeline unchanged.

## Dataset Selection

Change `DATASET_CHOICE` in `main.cpp` to select a demo dataset:

```cpp
const DatasetChoice DATASET_CHOICE = DatasetChoice::CAIDA;
```

Available values are:

```text
DatasetChoice::CAIDA
DatasetChoice::StackOverflow
```

## Output

The driver typically produces CSV files for per-flow spread estimation and super spreader detection.
You can further edit `main.cpp` if you want to change the output directory, experiment setup, or the datasets being loaded.
