#define DUCKDB_EXTENSION_MAIN

#include "clamp_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <algorithm>

namespace duckdb {

//------------------------------------------------------------------------------
// ClampOperator: Implements the core clamp logic for numeric types
//------------------------------------------------------------------------------
struct ClampOperator {
	// Template function to clamp a value between min_val and max_val
	// Throws an exception if min_val > max_val
	template <class T>
	static inline T Operation(T val, T min_val, T max_val) {
		// Handle NaN propagation
		if (std::isnan(val) || std::isnan(min_val) || std::isnan(max_val)) {
			return std::numeric_limits<T>::quiet_NaN();
		}

		// Validate bounds: min_val must not be greater than max_val
		if (min_val > max_val) {
			throw InvalidInputException("CLAMP error: Minimum bound (%s) cannot be greater than maximum bound (%s).",
			                            std::to_string(min_val), std::to_string(max_val));
		}
		// Clamp value: If val < min_val, return min_val; if val > max_val, return max_val; else return val
		return std::max(min_val, std::min(val, max_val));
	}
};

//------------------------------------------------------------------------------
// ClampFunction: DuckDB executor wrapper for ClampOperator
//------------------------------------------------------------------------------
template <class T>
static void ClampFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	// Uses DuckDB's TernaryExecutor to apply ClampOperator::Operation to each row
	// args.data[0]: value to clamp
	// args.data[1]: minimum bound
	// args.data[2]: maximum bound
	// result: output vector
	// args.size(): number of rows
	TernaryExecutor::Execute<T, T, T, T>(args.data[0], args.data[1], args.data[2], result, args.size(),
	                                     ClampOperator::Operation<T>);
}

//------------------------------------------------------------------------------
// SaturateOperator: Clamps a value to [0,1]
//------------------------------------------------------------------------------
struct SaturateOperator {
	// Template function to clamp a value between 0 and 1
	template <class T>
	static inline T Operation(T val) {
		if (val < T(0)) {
			return T(0);
		} else if (val > T(1)) {
			return T(1);
		} else {
			return val;
		}
	}
};

//------------------------------------------------------------------------------
// SaturateFunction: DuckDB executor wrapper for SaturateOperator
//------------------------------------------------------------------------------
template <class T>
static void SaturateFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	// args.data[0]: value to saturate
	UnaryExecutor::Execute<T, T>(args.data[0], result, args.size(), SaturateOperator::Operation<T>);
}

//------------------------------------------------------------------------------
// WrapOperator: Wrap a value x into the range [min_val, max_val) using modular
// arithmetic. This is useful for cyclic values like angles.
//
// Definition:
//   WRAP(x, min_val, max_val) = min_val + ((x - min_val) % (max_val - min_val))
//
// % is the floored modulus operator, which ensures the result is always in the
// range [0, max_val - min_val).
//------------------------------------------------------------------------------
// Helper for Integers
// Uses standard modulus operator and adjusts for negative results
template <class T>
static inline typename std::enable_if<std::is_integral<T>::value, T>::type ModuloLogic(T offset, T range) {
	T result = offset % range;
	if (result < 0)
		result += range;
	return result;
}

// Helper for Floating Point
// Uses std::fmod and adjusts for negative results
template <class T>
static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type ModuloLogic(T offset, T range) {
	T result = std::fmod(offset, range);
	if (result < 0)
		result += range;
	return result;
}

// Main WrapOperator that uses the appropriate ModuloLogic based on the type
struct WrapOperator {
	template <class T>
	static inline T Operation(T val, T min_val, T max_val) {
		// Use standard is_floating_point<T>::value for C++11 compatibility
		if (std::is_floating_point<T>::value) {
			if (std::isnan(static_cast<double>(val)) || std::isnan(static_cast<double>(min_val)) ||
			    std::isnan(static_cast<double>(max_val))) {
				return std::numeric_limits<T>::quiet_NaN();
			}
		}

		if (min_val >= max_val) {
			throw InvalidInputException("WRAP error: Minimum bound must be less than maximum bound.");
		}

		// The compiler picks the correct ModuloLogic overload at compile time
		return min_val + ModuloLogic<T>(val - min_val, max_val - min_val);
	}
};
//------------------------------------------------------------------------------
// WrapFunction: DuckDB executor wrapper for WrapOperator
//------------------------------------------------------------------------------
template <class T>
static void WrapFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	// Uses DuckDB's TernaryExecutor to apply WrapOperator::Operation to each row
	// args.data[0]: value to wrap
	// args.data[1]: minimum bound
	// args.data[2]: maximum bound
	// result: output vector
	// args.size(): number of rows
	TernaryExecutor::Execute<T, T, T, T>(args.data[0], args.data[1], args.data[2], result, args.size(),
	                                     WrapOperator::Operation<T>);
}

//------------------------------------------------------------------------------
// LoadInternal: Registers the clamp function(s) with DuckDB
//------------------------------------------------------------------------------
static void LoadInternal(ExtensionLoader &loader) {
	// Create a function set named "clamp" to support type overloading
	ScalarFunctionSet clamp("clamp");

	// ------------------------------------------------------------------------------
	// CLAMP
	// ------------------------------------------------------------------------------

	// Define clamp for DOUBLE type
	auto double_fun = ScalarFunction({LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::DOUBLE}, // argument types
	                                 LogicalType::DOUBLE,                                             // return type
	                                 ClampFunction<double>                                            // implementation
	);
	// Specify null handling: returns NULL if any input is NULL
	double_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;

	// Define clamp for BIGINT (int64_t) type
	auto bigint_fun = ScalarFunction({LogicalType::BIGINT, LogicalType::BIGINT, LogicalType::BIGINT},
	                                 LogicalType::BIGINT, ClampFunction<int64_t>);
	bigint_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;

	// Add both type-specific implementations to the function set
	clamp.AddFunction(double_fun);
	clamp.AddFunction(bigint_fun);

	// ------------------------------------------------------------------------------
	// SATURATE
	// ------------------------------------------------------------------------------
	ScalarFunctionSet saturate("saturate");

	// Define saturate for DOUBLE type
	auto double_sat_fun = ScalarFunction({LogicalType::DOUBLE}, LogicalType::DOUBLE, SaturateFunction<double>);
	double_sat_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;
	saturate.AddFunction(double_sat_fun);

	// Define saturate for BIGINT (int64_t) type
	auto bigint_sat_fun = ScalarFunction({LogicalType::BIGINT}, LogicalType::BIGINT, SaturateFunction<int64_t>);
	bigint_sat_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;
	saturate.AddFunction(bigint_sat_fun);

	// Create an alias for saturate
	ScalarFunctionSet saturate_alias("clamp01");
	saturate_alias.AddFunction(double_sat_fun);
	saturate_alias.AddFunction(bigint_sat_fun);

	// ------------------------------------------------------------------------------
	// WRAP
	// ------------------------------------------------------------------------------
	ScalarFunctionSet wrap("wrap");

	// Define wrap for DOUBLE type
	auto double_wrap_fun = ScalarFunction({LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::DOUBLE},
	                                      LogicalType::DOUBLE, WrapFunction<double>);
	double_wrap_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;

	// Define wrap for BIGINT (int64_t) type
	auto bigint_wrap_fun = ScalarFunction({LogicalType::BIGINT, LogicalType::BIGINT, LogicalType::BIGINT},
	                                      LogicalType::BIGINT, WrapFunction<int64_t>);
	bigint_wrap_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;

	wrap.AddFunction(double_wrap_fun);
	wrap.AddFunction(bigint_wrap_fun);

	// Register the function set with DuckDB
	loader.RegisterFunction(clamp);
	loader.RegisterFunction(saturate);
	loader.RegisterFunction(saturate_alias);
	loader.RegisterFunction(wrap);
}

//------------------------------------------------------------------------------
// ClampExtension: Extension interface implementation
//------------------------------------------------------------------------------
void ClampExtension::Load(ExtensionLoader &loader) {
	// Called by DuckDB to load the extension
	LoadInternal(loader);
}

// Returns the name of the extension
std::string ClampExtension::Name() {
	return "clamp";
}

// Returns the version of the extension (uses macro if defined)
std::string ClampExtension::Version() const {
#ifdef EXT_VERSION_CLAMP
	return EXT_VERSION_CLAMP;
#else
	return "0.1.0";
#endif
}

} // namespace duckdb

//------------------------------------------------------------------------------
// DuckDB Extension Entry Point (C linkage)
//------------------------------------------------------------------------------
extern "C" {
DUCKDB_CPP_EXTENSION_ENTRY(clamp, loader) {
	// Entry point called by DuckDB to load the extension
	duckdb::LoadInternal(loader);
}
}
