# MiniMD Benchmark - Mac ARM64

## Configuration

- Version: MiniMD ref
- Machine: MacBook (Apple Silicon, 8 GB RAM)
- Compiler: Apple Clang (-O3)
- MPI processes: 1
- OpenMP threads: 1
- System size: 100 x 100 x 100
- Number of atoms: 4,000,000
- AVX and SIMD: disabled

## Results

| Run | 10 steps (s) | 15 steps (s) |
|-----|-------------:|-------------:|
|  1  |     9.420    |    15.531    |
|  2  |     9.963    |    15.530    |
|  3  |     9.407    |    15.615    |
| Average | 9.597    |    15.559    |

## Notes

- Tests were run with run_benchmark.sh.
- Force computation takes more than 90% of the iteration time.
- The 15-step results were stable in this series.

## Next steps

- Look at force_lj.cpp.
- Try profiling with MAQAO.
- Compare the original and optimized versions.
