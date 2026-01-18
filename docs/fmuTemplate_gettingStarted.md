# Getting started with fmuTemplate

**Welcome.**

This overview will give you all information necessary to start implementing your own model equations, compile it to an FMU and test it, using this template.

The Project is structured as follows.

## The Folders

  | Directory | Contents |
  |-----------|----------|
  | `docs/` | References and documentation |
  | `examples/` | Example implementations |
  | `external/` | Third-party dependencies (FMI headers, Unity) |
  | `include/` | Project header files |
  | `scripts/` | Build and packaging scripts |
  | `src/` | Core FMU implementation |
  | `src/models/` | **Model implementations (add yours here)** |
  | `test/` | Unit and integration tests |

### How to Build the FMU

Run the following to build a specific **model_name**, which needs to match a folder name under `src/models`

```bash
rm -rf build
mkdir -p build && cd build
cmake -DMODEL=<model_name> ..
cmake --build .
```

## How to add your own Model


