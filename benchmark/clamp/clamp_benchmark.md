# Clamp Function Benchmark on TPC-DS

## Overview
This benchmark evaluates the correctness, query planning, and performance characteristics of a `clamp(x, min, max)` scalar function compared against two common alternatives:

- A `CASE WHEN` expression
- `LEAST(GREATEST(x, min), max)`

The goal is to validate semantic equivalence (where applicable), planner behavior, and runtime performance on a realistic analytical workload using DuckDB and TPC-DS data.

---

## Benchmark Setup

```sql
D INSTALL tpcds;
D LOAD tpcds;
D CALL dsdgen(sf=1);
```

- **Database**: DuckDB
- **Dataset**: TPC-DS `store_sales`
- **Scale Factor**: SF=1
- **Expression Under Test**:

```sql
x := ss_ext_discount_amt / NULLIF(ss_ext_sales_price, 0)
```

Clamp range:

```text
[0.0, 1.0]
```

---

## 1. Correctness Validation

### Query

All three expressions were compared row-by-row using `IS DISTINCT FROM` semantics:

- `CASE WHEN x < 0 THEN 0 WHEN x > 1 THEN 1 ELSE x END`
- `clamp(x, 0.0, 1.0)`
- `LEAST(GREATEST(x, 0.0), 1.0)`

```sql
D SELECT
·   count(*) FILTER (WHERE a IS DISTINCT FROM b) AS case_vs_clamp,
·   count(*) FILTER (WHERE a IS DISTINCT FROM c) AS case_vs_least
· FROM (
·   SELECT
·     CASE
·       WHEN x < 0 THEN 0
·       WHEN x > 1 THEN 1
·       ELSE x
·     END AS a,
·     clamp(x, 0.0, 1.0) AS b,
·     LEAST(GREATEST(x, 0.0), 1.0) AS c
·   FROM (
·     SELECT ss_ext_discount_amt / NULLIF(ss_ext_sales_price, 0) AS x
·     FROM store_sales
·   )
· );
```

### Results

| Comparison             | Mismatched Rows |
|------------------------|-----------------|
| CASE           vs CLAMP| 0               |
| LEAST/GREATEST vs CLAMP| 221,566         |

### Interpretation

- `clamp` is **semantically identical** to the explicit `CASE` formulation.
- `LEAST(GREATEST(...))` diverges in a significant number of rows most likely due to **NULL propagation semantics**.
- This shows that `clamp` can provide safer and more intuitive behavior for bounded expressions involving NULLs.

---

## 2. Query Planning Analysis

### Observations

`EXPLAIN ANALYZE` shows that all three variants:

- Perform a **sequential scan** of `store_sales`
- Apply a projection on top of the scan
- Produce identical estimated cardinalities
- Do **not** introduce additional operators or branches

```sql

D EXPLAIN ANALYZE
  SELECT
    clamp(x, 0.0, 1.0)
  FROM (
    SELECT ss_ext_discount_amt / NULLIF(ss_ext_sales_price, 0) AS x
    FROM store_sales
  );
{
    "total_bytes_written": 0,
    "total_bytes_read": 0,
    "result_set_size": 0,
    "cumulative_rows_scanned": 0,
    "cpu_time": 0.0,
    "blocked_thread_time": 0.0,
    "rows_returned": 0,
    "system_peak_temp_dir_size": 0,
    "cumulative_cardinality": 0,
    "system_peak_buffer_memory": 0,
    "extra_info": {},
    "query_name": "",
    "latency": 0.0,
    "children": [
        {
            "total_bytes_written": 0,
            "result_set_size": 0,
            "operator_timing": 0.00006805300000000008,
            "operator_rows_scanned": 0,
            "cumulative_rows_scanned": 0,
            "operator_cardinality": 0,
            "operator_type": "EXPLAIN_ANALYZE",
            "total_bytes_read": 0,
            "operator_name": "EXPLAIN_ANALYZE",
            "cpu_time": 0.0,
            "extra_info": {},
            "cumulative_cardinality": 0,
            "children": [
                {
                    "cumulative_cardinality": 0,
                    "extra_info": {
                        "Projections": "clamp(x, 0.0, 1.0)",
                        "Estimated Cardinality": "2880404"
                    },
                    "operator_name": "PROJECTION",
                    "cpu_time": 0.0,
                    "cumulative_rows_scanned": 0,
                    "operator_rows_scanned": 0,
                    "operator_timing": 0.01986384,
                    "result_set_size": 23043232,
                    "total_bytes_read": 0,
                    "operator_type": "PROJECTION",
                    "total_bytes_written": 0,
                    "operator_cardinality": 2880404,
                    "children": [
                        {
                            "total_bytes_written": 0,
                            "result_set_size": 23043232,
                            "operator_timing": 0.056056723,
                            "operator_rows_scanned": 0,
                            "cumulative_rows_scanned": 0,
                            "operator_cardinality": 2880404,
                            "operator_type": "PROJECTION",
                            "total_bytes_read": 0,
                            "operator_name": "PROJECTION",
                            "cpu_time": 0.0,
                            "extra_info": {
                                "Projections": "x",
                                "Estimated Cardinality": "2880404"
                            },
                            "cumulative_cardinality": 0,
                            "children": [
                                {
                                    "cumulative_cardinality": 0,
                                    "extra_info": {
                                        "Table": "store_sales",
                                        "Type": "Sequential Scan",
                                        "Projections": [
                                            "ss_ext_discount_amt",
                                            "ss_ext_sales_price"
                                        ],
                                        "Estimated Cardinality": "2880404"
                                    },
                                    "operator_name": "SEQ_SCAN ",
                                    "cpu_time": 0.0,
                                    "cumulative_rows_scanned": 0,
                                    "operator_rows_scanned": 23043232,
                                    "operator_timing": 0.002904011000000001,
                                    "result_set_size": 23043232,
                                    "total_bytes_read": 0,
                                    "operator_type": "TABLE_SCAN",
                                    "total_bytes_written": 0,
                                    "operator_cardinality": 2880404,
                                    "children": []
                                }
                            ]
                        }
                    ]
                }
            ]
        }
    ]
}Run Time (s): real 0.014 user 0.064396 sys 0.001032

```

### Key Takeaway

The DuckDB planner treats `clamp` as a simple scalar projection. 

---

## Conclusions

- **Correctness**: `clamp` matches `CASE` semantics exactly
- **Safety**: `LEAST(GREATEST(...))` is not semantically equivalent due to NULL handling
- **Planner-friendly**: No negative impact on query plans or execution strategy

### Recommendation

`clamp(x, min, max)` helps with being:

- More readable
- Safer with NULLs
- Equivalent performance

This benchmark supports inclusion and use of `clamp` as a first-class scalar function in DuckDB.

```sql
D SELECT
    clamp(x, 0.0, 1.0)
  FROM (
    SELECT ss_ext_discount_amt / NULLIF(ss_ext_sales_price, 0) AS x
    FROM store_sales
  );
┌──────────────────────┐
│  clamp(x, 0.0, 1.0)  │
│        double        │
├──────────────────────┤
│  0.45000000000000007 │
│ 0.029998892067857583 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                 NULL │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                 NULL │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                   ·  │
│                   ·  │
│                   ·  │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│   0.9199095022624435 │
│                  0.0 │
│  0.16999999999999998 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│                  0.0 │
│   0.3399878218510786 │
│                 NULL │
│                  0.0 │
│                  0.0 │
│   0.7799946489316975 │
├──────────────────────┤
│     2880404 rows     │
│ (2.88 million rows)  │
│      (40 shown)      │
└──────────────────────┘
Run Time (s): real 0.025 user 0.099189 sys 0.013725
D 
  SELECT
    LEAST(GREATEST(x, 0.0), 1.0)
  FROM (
    SELECT ss_ext_discount_amt / NULLIF(ss_ext_sales_price, 0) AS x
    FROM store_sales
  );
┌──────────────────────────────┐
│ least(greatest(x, 0.0), 1.0) │
│            double            │
├──────────────────────────────┤
│          0.45000000000000007 │
│         0.029998892067857583 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                           ·  │
│                           ·  │
│                           ·  │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│           0.9199095022624435 │
│                          0.0 │
│          0.16999999999999998 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│           0.3399878218510786 │
│                          0.0 │
│                          0.0 │
│                          0.0 │
│           0.7799946489316975 │
├──────────────────────────────┤
│         2880404 rows         │
│ 2.88 million rows   40 shown │
└──────────────────────────────┘
Run Time (s): real 0.025 user 0.116170 sys 0.003242
D 
  SELECT
    CASE
      WHEN x < 0 THEN 0
      WHEN x > 1 THEN 1
      ELSE x
    END
  FROM (
    SELECT ss_ext_discount_amt / NULLIF(ss_ext_sales_price, 0) AS x
    FROM store_sales
  );
┌──────────────────────────────────────────────────────────────────┐
│ CASE  WHEN ((x < 0)) THEN (0) WHEN ((x > 1)) THEN (1) ELSE x END │
│                              double                              │
├──────────────────────────────────────────────────────────────────┤
│                                              0.45000000000000007 │
│                                             0.029998892067857583 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                             NULL │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                             NULL │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                               ·  │
│                                                               ·  │
│                                                               ·  │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                               0.9199095022624435 │
│                                                              0.0 │
│                                              0.16999999999999998 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                                              0.0 │
│                                               0.3399878218510786 │
│                                                             NULL │
│                                                              0.0 │
│                                                              0.0 │
│                                               0.7799946489316975 │
├──────────────────────────────────────────────────────────────────┤
│            2880404 rows (2.88 million rows, 40 shown)            │
└──────────────────────────────────────────────────────────────────┘
Run Time (s): real 0.025 user 0.110444 sys 0.002865
D 
```