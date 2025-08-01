# intPSI - Private Set Intersection Research Repository

This repository contains implementations and experiments for various Private Set Intersection (PSI) protocols, including fuzzy PSI, OKVS-based PSI, and other state-of-the-art approaches.

## Directory Structure

### Core Directories

#### libraries
Contains cryptographic libraries and dependencies:
- cryptographic - Core cryptographic primitives
- psi-specific - PSI-focused libraries (APSI, libPSI, etc.)
- utilities - Helper libraries and tools

#### implementations
Main PSI protocol implementations:
- okvs-psi - OKVS (Oblivious Key-Value Stores) based PSI
- ultra-psi - Ultra-fast PSI implementation
- fuzzy-psi - Fuzzy PSI with distance-based matching
- apsi-variants - APSI and enhanced versions

#### projects
Development and testing projects:
- Experimental implementations
- Testing frameworks
- Build configurations

### Supporting Directories

#### results
Experimental results and performance data:
- benchmarks - Performance benchmark results
- comparisons - Protocol comparison data  
- charts - Visualization and graphs
- raw-data - Raw experimental data files

#### tools
Development utilities:
- scripts - Automation and helper scripts
- environments - Python virtual environments
- generators - Data generation tools

#### experiments
Experimental configurations:
- configurations - Test parameter settings
- datasets - Experimental datasets
- protocols - Protocol specifications

#### archive
Legacy and archived content:
- Historical implementations
- Deprecated code
- Backup files

## Quick Start

### Building Core Implementations

#### OKVS PSI
cd implementations/okvs-psi/
mkdir build && cd build
cmake ..
make -j$(nproc)

#### Ultra PSI  
cd implementations/ultra-psi/
mkdir build && cd build
cmake ..
make -j$(nproc)

#### Fuzzy PSI
cd implementations/fuzzy-psi/
mkdir build && cd build  
cmake ..
make -j$(nproc)

### Running Benchmarks
cd tools/scripts/
./run_benchmarks.sh

### Viewing Results
cd results/charts/
# View performance charts and comparisons

## Development Workflow

1. Implementation development in implementations/
2. Testing using projects/ framework
3. Results analysis in results/
4. Documentation in docs/

## Performance Overview

Recent benchmark results:
- OKVS PSI: 33.4x speedup, 9.9x communication reduction
- Ultra PSI: Optimal for datasets >= 2^12 elements  
- Fuzzy PSI: Efficient prefix-based distance matching

Detailed results available in results/ directory.

## Project Status

- OKVS PSI: Production ready
- Ultra PSI: Optimization phase
- Fuzzy PSI: Research implementation
- APSI variants: Testing phase

## Contributing

1. Develop in appropriate implementations/ subdirectory
2. Test using projects/ framework
3. Document results in results/
4. Update this README as needed

## References

- APSI: Asymmetric Private Set Intersection
- OKVS: Oblivious Key-Value Stores  
- VOLE: Vector Oblivious Linear Evaluation
- Fuzzy PSI: Distance-based Private Set Intersection

Last Updated: $(date)
Version: 2.0
