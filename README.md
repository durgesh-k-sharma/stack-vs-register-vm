# Stack vs. register virtual machines

This study implements two bytecode interpreters for the same small language and compares their execution time, bytecode size, and instruction counts. The design is informed by *The Implementation of Lua 5.0* by Roberto Ierusalimschy, Luiz Henrique de Figueiredo, and Waldemar Celes. It is an independent comparison, not a reproduction of the paper's Lua benchmarks.

## Build and run

```sh
make
./vmstudy
```

Run one workload with `./vmstudy sum` (or `fibonacci`, `ackermann`, `sieve`, `heapsort`, or `matrix`). `./vmstudy --list` prints the workload inputs.

The runner parses and compiles each source program once, runs one warm-up for each VM, then times nine executions per VM using `CLOCK_MONOTONIC`. It alternates which VM runs first. Timing covers VM execution through return, including workload array allocation and access. The runner checks that the VMs return the same result on warm-up and every timed run.

## Language

Workloads in `workloads/` use a compact C-like syntax. Values are signed 64-bit integers or references to integer arrays. Arithmetic uses `+`, `-`, `*`, `/`, and `%`; it wraps at 64 bits, signed division truncates toward zero, and division by zero is an error. Comparisons return 0 or 1, and nonzero integers are true. `array(length)` creates a zero-filled array of at most 10,000,000 elements; indexed reads and writes check bounds. Functions have fixed parameter and return types, may recurse, and receive arrays by reference. Locals have function scope, names cannot be redeclared in one function, and a function that falls through returns 0.

Example:

```text
fn sum_to(n: int) -> int {
    let total: int = 0;
    let i: int = 1;
    while (i <= n) {
        total = total + i;
        i = i + 1;
    }
    return total;
}

fn main() -> int {
    return sum_to(100);
}
```

The common parser and type checker build one typed AST. Two backends lower that AST to bytecode. The stack VM moves expression operands through an operand stack and uses load/store instructions for local values. The register VM names local and result slots directly. Stack instructions use compact variable-length encodings; register instructions use one 32-bit word per operation. Both use the same tagged integer/array value representation and the same 64-bit literal pool. Register instructions can use either a register or a constant-pool operand. The current register format supports up to 128 source registers and 128 distinct integer constants per workload.

## Measurements

- **Static instructions** count emitted operations, not operand bytes.
- **Dynamic instructions** count executed VM operations for one workload run, including calls, returns, and array operations.
- **Stream bytes** count encoded instructions. **Total bytes** add the shared 64-bit integer constant pool; function/debug/compiler metadata is excluded.
- **Execution time** is the median of nine runs. The table also gives the minimum and maximum. Register speedup is stack median divided by register median; values above 1 mean the register VM ran faster.

The benchmark inputs are sum(20,000,000), Fibonacci(30), Ackermann(3, 8), 100 sieve passes over 8,192 values, heapsort of 50,000 values, and 50 by 50 matrix multiplication. See [RESULTS.md](RESULTS.md) for the measured environment and comparison.

## Paper context

Lua 5.0 changed from a stack VM to a register VM. The paper describes explicit register operands and four-byte instructions as a trade-off against shorter stack instructions. Fewer executed instructions and less copying of tagged values can offset that larger instruction width. Its benchmark section separates the VM change from Lua 5.0's table-array optimization; the results here use one language implementation and one workload definition for both VMs.

The supplied source is *The Implementation of Lua 5.0*; the VM discussion and benchmark are in PDF pages 11–16.
