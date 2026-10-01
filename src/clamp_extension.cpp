#define DUCKDB_EXTENSION_MAIN

#include "clamp_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

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
			throw InvalidInputException("Error: Minimum bound (%s) cannot be greater than maximum bound (%s).",
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

		// Validate bounds: min_val must not be greater than or equal to max_val
		if (min_val >= max_val) {
			throw InvalidInputException(
			    "Error: Minimum bound (%s) cannot be greater than or equal to maximum bound (%s).",
			    std::to_string(min_val), std::to_string(max_val));
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
// FractOperator: Decimal part of a number (x - floor(x))
//------------------------------------------------------------------------------
// Helper for floating point types (double, float)
template <class T>
static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type FractLogic(T a) {
	return a - std::floor(a);
}

// Helper for non-floating point types (int64_t, etc.)
template <class T>
static inline typename std::enable_if<!std::is_floating_point<T>::value, T>::type FractLogic(T a) {
	return 0;
}

struct FractOperator {
	template <class T>
	static inline T Operation(T a) {
		// No NaNs in integers, but let the compiler handle the check for floats
		if (std::is_floating_point<T>::value) {
			if (std::isnan(static_cast<double>(a))) {
				return std::numeric_limits<T>::quiet_NaN();
			}
		}

		return FractLogic<T>(a);
	}
};

//------------------------------------------------------------------------------
// FractFunction: DuckDB executor wrapper for FractOperator
//------------------------------------------------------------------------------
template <class T>
static void FractFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	UnaryExecutor::Execute<T, T>(args.data[0], result, args.size(), FractOperator::Operation<T>);
}

//------------------------------------------------------------------------------
// StepOperator: Step function
//------------------------------------------------------------------------------
struct StepOperator {
	template <class T>
	static inline T Operation(T edge, T x) {
		// No NaNs in integers, but let the compiler handle the check for floats
		if (std::is_floating_point<T>::value) {
			if (std::isnan(static_cast<double>(edge)) || std::isnan(static_cast<double>(x))) {
				return std::numeric_limits<T>::quiet_NaN();
			}
		}

		return x < edge ? T(0) : T(1);
	}
};

//------------------------------------------------------------------------------
// StepFunction: DuckDB executor wrapper for StepOperator
//------------------------------------------------------------------------------
template <class T>
static void StepFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	// Uses DuckDB's BinaryExecutor to apply StepOperator::Operation to each row
	// args.data[0]: edge (threshold)
	// args.data[1]: value to test
	// result: output vector
	// args.size(): number of rows
	BinaryExecutor::Execute<T, T, T>(args.data[0], args.data[1], result, args.size(), StepOperator::Operation<T>);
}

//------------------------------------------------------------------------------
// PingPongOperator: Triangle Wave Generator
//------------------------------------------------------------------------------
// Based on the Blender/GLSL math logic. It creates a continuous oscillation
// between min_val and max_val. As 'val' increases, the result moves from
// min to max, then reverses back to min.
//------------------------------------------------------------------------------

// Helper for Integers
// Uses integer division and modulus to create a triangle wave pattern
template <class T>
static inline typename std::enable_if<std::is_integral<T>::value, T>::type PingPongLogic(T val, T min_val, T max_val) {
	// Calculate range. We use uint64_t for the range to safely
	// handle the case where max is max_int and min is min_int.
	uint64_t u_range = static_cast<uint64_t>(max_val) - static_cast<uint64_t>(min_val);
	if (u_range == 0)
		return min_val;

	// Calculate offset. We use __int128 if available for absolute safety,
	// but for standard BIGINT, we can use careful logic with the range.
	int64_t offset = static_cast<int64_t>(val) - static_cast<int64_t>(min_val);

	// Euclidean Division: Calculate quotient and remainder such that
	// remainder is always in [0, u_range).
	int64_t q = offset / static_cast<int64_t>(u_range);
	int64_t r = offset % static_cast<int64_t>(u_range);

	// Adjust for negative offsets to ensure a continuous wave across the origin
	if (r < 0) {
		r += u_range;
		q -= 1;
	}

	// Parity Check (The Bounce)
	// If the quotient is even (0, 2, -2...), we are moving UP from min.
	// If the quotient is odd (1, 3, -1, -3...), we are moving DOWN from max.
	if (std::abs(q) % 2 == 0) {
		return static_cast<T>(static_cast<uint64_t>(min_val) + r);
	} else {
		return static_cast<T>(static_cast<uint64_t>(max_val) - r);
	}
}

// Helper for Floating Point
// Uses fmod and floating point arithmetic to create a triangle wave pattern
template <class T>
static inline typename std::enable_if<std::is_floating_point<T>::value, T>::type PingPongLogic(T val, T min_val,
                                                                                               T max_val) {
	double d_val = static_cast<double>(val);
	double d_min = static_cast<double>(min_val);
	double d_max = static_cast<double>(max_val);

	double range = d_max - d_min;
	double period = range * 2.0;

	// Use fmod for doubles instead of %
	double t = std::fmod(d_val - d_min, period);
	if (t < 0)
		t += period;

	if (t > range) {
		return static_cast<T>(d_min + (period - t));
	}
	return static_cast<T>(d_min + t);
}

struct PingPongOperator {
	template <class T>
	static inline T Operation(T val, T min_val, T max_val) {
		// Use standard is_floating_point<T>::value for C++11 compatibility
		if (std::is_floating_point<T>::value) {
			if (std::isnan(static_cast<double>(val)) || std::isnan(static_cast<double>(min_val)) ||
			    std::isnan(static_cast<double>(max_val))) {
				return std::numeric_limits<T>::quiet_NaN();
			}
		}

		// Validate bounds: min_val must not be greater than or equal to max_val
		if (min_val >= max_val) {
			throw InvalidInputException(
			    "Error: Minimum bound (%s) cannot be greater than or equal to maximum bound (%s).",
			    std::to_string(min_val), std::to_string(max_val));
		}

		// The compiler picks the correct PingPongLogic overload at compile time
		return PingPongLogic<T>(val, min_val, max_val);
	}
};

//------------------------------------------------------------------------------
// PingPongFunction: DuckDB executor wrapper for PingPongOperator
//------------------------------------------------------------------------------
template <class T>
static void PingPongFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	// Uses DuckDB's TernaryExecutor to apply PingPongOperator::Operation to each row
	// args.data[0]: value to pingpong
	// args.data[1]: minimum bound
	// args.data[2]: maximum bound
	// result: output vector
	// args.size(): number of rows
	TernaryExecutor::Execute<T, T, T, T>(args.data[0], args.data[1], args.data[2], result, args.size(),
	                                     PingPongOperator::Operation<T>);
}

//------------------------------------------------------------------------------
// FunctionDoc / RegisterDocumented: attach catalog metadata to a function set
//------------------------------------------------------------------------------
// The bare loader.RegisterFunction(ScalarFunctionSet) overload cannot carry any
// documentation. Registering through CreateScalarFunctionInfo lets each function
// expose a description, real parameter names, examples and categories via
// duckdb_functions(), which is how tools and AI agents discover what it does.
struct FunctionDoc {
	vector<string> parameter_names;
	string description;
	vector<string> examples;
	vector<string> categories;
	// Name of the function this one is an alias of (empty if not an alias)
	string alias_of;
};

static void RegisterDocumented(ExtensionLoader &loader, ScalarFunctionSet set, const FunctionDoc &doc) {
	CreateScalarFunctionInfo info(std::move(set));
	// Matches what the bare RegisterFunction overload does internally; the default
	// on CreateInfo is ERROR_ON_CONFLICT.
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	info.alias_of = doc.alias_of;

	// A single description with no parameter_types applies to every overload
	// (DOUBLE and BIGINT) of the function.
	FunctionDescription desc;
	desc.parameter_names = doc.parameter_names;
	desc.description = doc.description;
	desc.examples = doc.examples;
	desc.categories = doc.categories;
	info.descriptions.push_back(std::move(desc));

	loader.RegisterFunction(std::move(info));
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

	// Add alias for clamp
	ScalarFunctionSet clamp_alias("clip");
	clamp_alias.AddFunction(double_fun);
	clamp_alias.AddFunction(bigint_fun);

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

	// ------------------------------------------------------------------------------
	// PINGPONG
	// ------------------------------------------------------------------------------
	ScalarFunctionSet pingpong("pingpong");

	// Define pingpong for DOUBLE type
	auto double_pingpong_fun =
	    ScalarFunction({LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::DOUBLE}, // argument types
	                   LogicalType::DOUBLE,                                             // return type
	                   PingPongFunction<double>                                         // implementation
	    );
	// Specify null handling: returns NULL if any input is NULL
	double_pingpong_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;

	// Define pingpong for BIGINT (int64_t) type
	auto bigint_pingpong_fun = ScalarFunction({LogicalType::BIGINT, LogicalType::BIGINT, LogicalType::BIGINT},
	                                          LogicalType::BIGINT, PingPongFunction<int64_t>);
	bigint_pingpong_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;

	// Add both type-specific implementations to the function set
	pingpong.AddFunction(double_pingpong_fun);
	pingpong.AddFunction(bigint_pingpong_fun);

	// ------------------------------------------------------------------------------
	// FRACT
	// ------------------------------------------------------------------------------
	ScalarFunctionSet fract("fract");

	// Define fract for DOUBLE type
	auto double_fract_fun = ScalarFunction({LogicalType::DOUBLE}, LogicalType::DOUBLE, FractFunction<double>);
	double_fract_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;
	fract.AddFunction(double_fract_fun);

	// Define fract for BIGINT (int64_t) type
	auto bigint_fract_fun = ScalarFunction({LogicalType::BIGINT}, LogicalType::BIGINT, FractFunction<int64_t>);
	bigint_fract_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;
	fract.AddFunction(bigint_fract_fun);

	// ------------------------------------------------------------------------------
	// STEP
	// ------------------------------------------------------------------------------
	ScalarFunctionSet step("step");

	// Define step for DOUBLE type
	auto double_step_fun =
	    ScalarFunction({LogicalType::DOUBLE, LogicalType::DOUBLE}, LogicalType::DOUBLE, StepFunction<double>);
	double_step_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;
	step.AddFunction(double_step_fun);

	// Define step for BIGINT (int64_t) type
	auto bigint_step_fun =
	    ScalarFunction({LogicalType::BIGINT, LogicalType::BIGINT}, LogicalType::BIGINT, StepFunction<int64_t>);
	bigint_step_fun.null_handling = FunctionNullHandling::DEFAULT_NULL_HANDLING;
	step.AddFunction(bigint_step_fun);

	// ------------------------------------------------------------------------------
	// REGISTER FUNCTIONS
	// ------------------------------------------------------------------------------

	// Register each function set together with its documentation
	RegisterDocumented(
	    loader, clamp,
	    {{"value", "min_val", "max_val"},
	     "Restricts value to the range [min_val, max_val]. Returns min_val if value is below it and max_val if value "
	     "is above it. Returns NaN if any argument is NaN and raises an error if min_val > max_val.",
	     {"clamp(15, 0, 10)"},
	     {"numeric"},
	     ""});
	RegisterDocumented(loader, clamp_alias,
	                   {{"value", "min_val", "max_val"},
	                    "Alias of clamp. Restricts value to the range [min_val, max_val].",
	                    {"clip(15, 0, 10)"},
	                    {"numeric"},
	                    "clamp"});
	RegisterDocumented(loader, saturate,
	                   {{"value"},
	                    "Restricts value to the range [0, 1]. Returns 0 for values below 0 and 1 for values above 1.",
	                    {"saturate(1.5)"},
	                    {"numeric"},
	                    ""});
	RegisterDocumented(loader, saturate_alias,
	                   {{"value"},
	                    "Alias of saturate. Restricts value to the range [0, 1].",
	                    {"clamp01(1.5)"},
	                    {"numeric"},
	                    "saturate"});
	RegisterDocumented(
	    loader, wrap,
	    {{"value", "min_val", "max_val"},
	     "Wraps value into the half-open range [min_val, max_val) using modular arithmetic, which is useful for "
	     "cyclic values such as angles. Raises an error if min_val >= max_val.",
	     {"wrap(370, 0, 360)"},
	     {"numeric"},
	     ""});
	RegisterDocumented(
	    loader, pingpong,
	    {{"value", "min_val", "max_val"},
	     "Bounces value back and forth between min_val and max_val, producing a triangle wave. Raises an error if "
	     "min_val >= max_val.",
	     {"pingpong(12, 0, 10)"},
	     {"numeric"},
	     ""});
	RegisterDocumented(loader, fract,
	                   {{"value"},
	                    "Returns the fractional part of value, computed as value - floor(value). For example the "
	                    "result for -1.25 is 0.75. Always returns 0 for integers.",
	                    {"fract(3.75)"},
	                    {"numeric"},
	                    ""});
	RegisterDocumented(loader, step,
	                   {{"edge", "value"},
	                    "Threshold function that returns 0 if value < edge, otherwise 1. Returns NaN if either "
	                    "argument is NaN.",
	                    {"step(0.5, 0.75)"},
	                    {"numeric"},
	                    ""});
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
