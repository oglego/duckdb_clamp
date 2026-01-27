# Clamp

This repository is based on the official DuckDB
`extension-template`:
https://github.com/duckdb/extension-template

If you’re interested in building and distributing your own DuckDB
extension, that repository is a great starting point.

---

# DuckDB Clamp Extension

The Clamp extension adds a `clamp` scalar function to DuckDB that
restricts a value to a specified minimum and maximum bound.

The function is implemented as a native DuckDB scalar function using
`TernaryExecutor`, providing efficient vectorized execution over DuckDB
data chunks.

---

## Features

- Native DuckDB scalar function (`ScalarFunctionSet`)
- Vectorized execution via `TernaryExecutor`
- Supports:
  - `BIGINT` (`int64_t`)
  - `DOUBLE`
- Returns:
  - the input value if it lies within bounds
  - the nearest bound if the value is outside the range
- NULL-safe:
  - returns `NULL` if any argument is `NULL`
- Strict validation:
  - throws an error if `min > max`

---

## Installation

### From source

1. Build the extension using DuckDB’s extension build system (see
   **Building** below).
2. Place the compiled `clamp.duckdb_extension` binary in DuckDB’s
   extension directory or load it explicitly.

### Tips for speedy builds

DuckDB extensions currently rely on DuckDB's build system to provide easy testing and distributing. This does however come at the downside of requiring the template to build DuckDB and its unittest binary every time you build your extension. To mitigate this, we highly recommend installing ccache and ninja. This will ensure you only need to build core DuckDB once and allows for rapid rebuilds.

To clean build using ninja and ccache ensure both are installed and run:

```bash
rm -rf build
GEN=ninja make
```

To load the extension:

```bash
./build/release/duckdb
D LOAD 'build/release/extension/clamp/clamp.duckdb_extension';
D SELECT clamp(15, 10, 20);
┌───────────────────┐
│ clamp(15, 10, 20) │
│       int64       │
├───────────────────┤
│        15         │
└───────────────────┘
D 
```
---

## Usage

Load the extension:

INSTALL clamp;

LOAD clamp;

Use the `clamp` function:

-- Value within bounds
```sql
SELECT clamp(15, 10, 20);
┌───────────────────┐
│ clamp(15, 10, 20) │
│       int64       │
├───────────────────┤
│        15         │
└───────────────────┘
```

-- Below minimum
```sql
SELECT clamp(5, 10, 20);
┌──────────────────┐
│ clamp(5, 10, 20) │
│      int64       │
├──────────────────┤
│        10        │
└──────────────────┘
```

-- Above maximum
```sql
SELECT clamp(25, 10, 20);
┌───────────────────┐
│ clamp(25, 10, 20) │
│       int64       │
├───────────────────┤
│        20         │
└───────────────────┘
```

-- Floating point support
```sql
SELECT clamp(3.14, 2.71, 4.0);
┌────────────────────────┐
│ clamp(3.14, 2.71, 4.0) │
│         double         │
├────────────────────────┤
│          3.14          │
└────────────────────────┘
```

-- NULL propagation
```sql
SELECT clamp(NULL, 10, 20);
┌─────────────────────┐
│ clamp(NULL, 10, 20) │
│        int64        │
├─────────────────────┤
│        NULL         │
└─────────────────────┘
```

### Error case

If the minimum bound is greater than the maximum bound, the function
throws an error:

```sql
SELECT clamp(15, 20, 10);
Invalid Input Error:
CLAMP error: Minimum bound (20) cannot be greater than maximum bound (10).
```

---

## Function Signature

clamp(value, min, max)

### Parameters

- value  
  The value to clamp.

- min  
  Lower bound (inclusive).

- max  
  Upper bound (inclusive).

All arguments must be of the same type.

---

## Supported Types

BIGINT  - supported  
DOUBLE  - supported

---

## Implementation Notes

- The core clamp logic is implemented in `ClampOperator::Operation<T>`.
- DuckDB’s `TernaryExecutor` applies the operation across vectors,
  enabling efficient batch execution.
- NULL handling uses DuckDB’s DEFAULT_NULL_HANDLING, ensuring standard
  SQL semantics.
- Bound validation (`min > max`) is performed per row and raises an
  InvalidInputException.

---

## Testing

SQL tests live in:

test/sql/clamp.test

Run the test suite with:

make test

---

## Potential Future Additions

While this extension currently provides a `clamp(value, min, max)` scalar function, it intentionally leaves room for other *mathematically restrictive or range-based utilities* that frequently appear in numerical computing, analytics, and graphics domains.

Possible future additions include:

- **Saturate**  
  A specialized clamp that restricts values to the `[0, 1]` range. Common in graphics, normalization, and ML feature scaling.

- **Wrap / Modulo Clamp**  
  Wraps values around a range instead of clamping them (e.g., angles, cyclic time windows, periodic domains).

- **Ping-Pong**  
  Reflects values back and forth between bounds, useful for oscillating ranges or bounded waveforms.

- **Lerp (Linear Interpolation)**  
  Interpolates between two values using a normalized parameter, often used alongside clamping or saturation.

- **Min-Max Normalization**  
  Scales values from an arbitrary range into a target range (commonly `[0, 1]`).

- **Clip**  
  A semantic alias for clamp, matching terminology used in NumPy, PyTorch, and other scientific ecosystems.

These functions share similar characteristics:
- Simple scalar logic
- No external dependencies
- Deterministic and vectorizable execution

As such, they would fit naturally into this extension if there is interest from the DuckDB community. The current implementation is intentionally minimal and designed to serve as a foundation for these kinds of additions.

## Legal Disclaimer

THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
NONINFRINGEMENT.

IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES, OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT, OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

USE OF THIS EXTENSION IS AT YOUR OWN RISK.

### MIT License

Copyright (c) 2026

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.



