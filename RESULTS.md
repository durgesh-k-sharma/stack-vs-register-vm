# Results

## Environment and method

- System: Linux 6.18.33.2-microsoft-standard-WSL2, x86_64
- Compiler: GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1), `-O2 -std=c11 -Wall -Wextra -Wpedantic`
- Timer: `CLOCK_MONOTONIC`
- One warm-up and nine measured runs per VM and workload; VM order alternates between runs
- Timed region: VM execution through `main` return, including array allocation and access; source parsing and compilation are outside the timed region
- The runner checks equal results on both warm-up and all measured executions

“Static instructions” counts emitted operations. “Dynamic instructions” is the executed operation count from a measured run. Stream bytes include encoded operands; total bytes add the 64-bit integer constant pool and exclude compiler/function metadata. The time range is the minimum and maximum of nine runs. Register speedup is stack median divided by register median.

## Measurements

| Workload (input) | Result | Static instructions (stack / register) | Dynamic instructions (stack / register) | Stream bytes (stack / register) | Total bytes (stack / register) | Stack median ms (min–max) | Register median ms (min–max) | Register speedup |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Sum (20,000,000) | 200000010000000 | 21 / 9 | 260000010 / 100000005 | 43 / 36 | 67 / 60 | 474.861 (470.299–483.540) | 138.516 (138.191–139.755) | 3.428× |
| Fibonacci (30) | 832040 | 23 / 16 | 26925369 / 17501490 | 43 / 64 | 75 / 96 | 59.048 (57.320–61.841) | 41.723 (41.555–43.706) | 1.415× |
| Ackermann (3, 8) | 2045 | 36 / 26 | 36220022 / 23682009 | 70 / 104 | 102 / 136 | 58.314 (57.426–60.180) | 51.114 (50.752–51.962) | 1.141× |
| Sieve (100 passes, 8,192 values) | 1028 | 100 / 43 | 41375119 / 16879409 | 214 / 172 | 262 / 220 | 63.985 (63.769–64.915) | 31.826 (31.545–32.223) | 2.010× |
| Heapsort (50,000 values) | 2482962095 | 188 / 83 | 49448430 / 21768725 | 370 / 332 | 426 / 388 | 83.273 (81.651–83.875) | 45.341 (44.451–45.669) | 1.837× |
| Matrix multiply (50 × 50) | 262937410 | 136 / 58 | 3553291 / 1572869 | 266 / 232 | 338 / 304 | 5.436 (5.264–5.506) | 2.833 (2.753–2.961) | 1.919× |

## Reading the results

The register backend emits fewer static instructions and executes fewer dynamic instructions on every workload. It is also faster on this host, with measured speedups from 1.141× on Ackermann to 3.428× on the sum loop. The sum loop spends most of its work in repeated VM operations, so reducing dispatches has the largest effect there.

The fixed-width encoding does not always produce smaller bytecode. Fibonacci and Ackermann have smaller stack instruction streams, and their total bytecode remains smaller after adding the shared constant pool. The other four workloads have smaller register bytecode because their larger instruction-count reductions outweigh the four-byte instruction width.

These results describe these two C interpreters on one WSL2 x86_64 host. They do not predict the performance of Lua or other VMs. The paper's Lua comparison also measured a separate table-array optimization; this study has no such second optimization condition.
