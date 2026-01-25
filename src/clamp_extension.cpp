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
// LoadInternal: Registers the clamp function(s) with DuckDB
//------------------------------------------------------------------------------
static void LoadInternal(ExtensionLoader &loader) {
	// Create a function set named "clamp" to support type overloading
	ScalarFunctionSet clamp("clamp");

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

	// Register the function set with DuckDB
	loader.RegisterFunction(clamp);
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
