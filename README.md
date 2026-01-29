# Clamp

This repository is based on the official DuckDB
`extension-template`:
https://github.com/duckdb/extension-template

If you’re interested in building and distributing your own DuckDB
extension, that repository is a great starting point.

---

# DuckDB Clamp Extension

The Clamp extension adds a small family of scalar functions to DuckDB for
restricting values to a range.

It currently provides:

- `clamp(value, min, max)` — clamp to an arbitrary range
- `saturate(value)` — clamp to the `[0, 1]` range
- `clamp01(value)` — alias for `saturate`, common in graphics and ML

All functions are implemented as native DuckDB scalar functions with
vectorized execution for high performance.

---

## Features

- Native DuckDB scalar functions (`ScalarFunctionSet`)
- Vectorized execution via DuckDB executors
  - `TernaryExecutor` for `clamp`
  - `UnaryExecutor` for `saturate` / `clamp01`
- Supports:
  - `BIGINT` (`int64_t`) — `clamp`, `saturate1`, `clamp01`
  - `DOUBLE` — `clamp`, `saturate`, `clamp01`
- NULL-safe:
  - returns `NULL` if any input argument is `NULL`
- Strict validation:
  - `clamp` throws an error if `min > max`

---

## Installation

### From source

1. Build the extension using DuckDB’s extension build system (see
   **Building** below).
2. Place the compiled `clamp.duckdb_extension` binary in DuckDB’s
   extension directory or load it explicitly.

### Tips for speedy builds

DuckDB extensions currently rely on DuckDB's build system to provide easy
testing and distributing. This does however come at the downside of
requiring the template to build DuckDB and its unittest binary every time
you build your extension. To mitigate this, we highly recommend installing
ccache and ninja. This will ensure you only need to build core DuckDB once
and allows for rapid rebuilds.

To clean build using ninja and ccache ensure both are installed and run:

```bash
rm -rf build
GEN=ninja make
```

To load the extension:

```sql
LOAD 'build/release/extension/clamp/clamp.duckdb_extension';
SELECT clamp(15, 10, 20);
```

---

## Usage

Load the extension:

```sql
INSTALL clamp;
LOAD clamp;
```

### clamp(value, min, max)

Restricts a value to an inclusive minimum and maximum bound.

```sql
-- Value within bounds
SELECT clamp(15, 10, 20);
-- 15

-- Below minimum
SELECT clamp(5, 10, 20);
-- 10

-- Above maximum
SELECT clamp(25, 10, 20);
-- 20

-- Floating point support
SELECT clamp(3.14, 2.71, 4.0);
-- 3.14

-- NULL propagation
SELECT clamp(NULL, 10, 20);
-- NULL
```

If the minimum bound is greater than the maximum bound, the function throws
an error:

```sql
SELECT clamp(15, 20, 10);
-- CLAMP error: Minimum bound (20) cannot be greater than maximum bound (10).
```

---

### saturate(value)

A specialized clamp that restricts a value to the `[0, 1]` range.

This function is commonly used in graphics, normalization, and machine
learning workflows.

```sql
SELECT saturate(-0.25);
-- 0.0

SELECT saturate(0.5);
-- 0.5

SELECT saturate(1.75);
-- 1.0

SELECT saturate(NULL);
-- NULL
```

---

### clamp01(value)

An alias for `saturate(value)`.

This name is widely used in graphics and shader languages and is provided
for familiarity and readability.

```sql
SELECT clamp01(-1.0);
-- 0.0

SELECT clamp01(0.25);
-- 0.25

SELECT clamp01(2.0);
-- 1.0
```

---

## Function Signatures

```
clamp(value, min, max)
saturate(value)
clamp01(value)
```

### Parameters

- `value`
  The value to restrict.

- `min`
  Lower bound (inclusive). Used only by `clamp`.

- `max`
  Upper bound (inclusive). Used only by `clamp`.

All arguments must be of the same type.

---

## Supported Types

| Function   | BIGINT | DOUBLE |
|-----------|--------|--------|
| clamp     | ✓      | ✓      |
| saturate  | ✓      | ✓      |
| clamp01   | ✓      | ✓      |

---

## Implementation Notes

- The core clamp logic is implemented in `ClampOperator::Operation<T>`.
- `saturate` and `clamp01` use a specialized unary operator with fixed
  bounds for efficiency and clarity.
- DuckDB’s vectorized executors apply operations across data chunks,
  enabling efficient batch execution.
- NULL handling uses DuckDB’s `DEFAULT_NULL_HANDLING`, ensuring standard
  SQL semantics.

---

## Testing

SQL tests live in:

```
test/sql/clamp.test
```

Run the test suite with:

```bash
make test
```

---

## Potential Future Additions

While this extension currently provides `clamp`, `saturate`, and
`clamp01`, it intentionally leaves room for other mathematically
range-based utilities that frequently appear in numerical computing,
analytics, and graphics domains.

Possible future additions include:

- **Wrap / Modulo Clamp**  
  Wraps values around a range instead of clamping them (e.g., angles,
  cyclic time windows, periodic domains).

- **Ping-Pong**  
  Reflects values back and forth between bounds, useful for oscillating
  ranges or bounded waveforms.

- **Lerp (Linear Interpolation)**  
  Interpolates between two values using a normalized parameter.

- **Min-Max Normalization**  
  Scales values from an arbitrary range into a target range.

- **Clip**  
  A semantic alias for `clamp`, matching terminology used in NumPy and
  PyTorch.

These functions share similar characteristics:

- Simple scalar logic
- Deterministic and vectorizable execution
- No external dependencies

---

## Legal Disclaimer

THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT.

IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### MIT License

Copyright (c) 2026

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

