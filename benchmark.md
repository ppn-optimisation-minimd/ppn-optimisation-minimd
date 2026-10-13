# MiniMD Benchmark

## Configuration

- Version: MiniMD reference 2.0
- Machine: MacBook (Apple Silicon, 8 GB RAM)
- MPI processes: 1
- OpenMP threads: 1
- System size: 100 x 100 x 100
- Number of atoms: 4,000,000
- Force model: Lennard-Jones (LJ)
- Intrinsics: disabled

## Results

I ran the reference version with 10 and 15 steps, 3 times each.

| Run | 10 steps (s) | 15 steps (s) |
|-----|-------------:|-------------:|
|  1  |     8.686    |     14.306   |
|  2  |     8.422    |     14.452   |
|  3  |     8.521    |     14.710   |
|  Average  | 8.543  |     14.489   |

The results are taken from `t_total` in MiniMD's Performance Summary.

The full logs are saved in `results/baseline/`.

## Timing

I checked the timing code in `baseline/ref/ljs.cpp`.

`TIME_TOTAL` starts before `integrate.run()` and stops after it. This means `t_total` measures the simulation loop, not the initial setup.

MiniMD also reports:

- `t_force`: force calculation
- `t_neigh`: neighbor list
- `t_comm`: communication
- `t_other`: remaining time

## Performance

For 15 steps:

- Average total time: 14.489 s
- Average force time: 13.716 s
- Force calculation: about 94.7% of total time

Most of the time is spent on force calculation, so this is the first part to look at when optimizing MiniMD.

These numbers come from MiniMD's own timers. I haven't run MAQAO profiling yet.

## Next Steps

- Check `force_lj.cpp`.
- Run MAQAO on the reference version.
- Compare the reference, manually optimized and AI-optimized versions using the same settings.