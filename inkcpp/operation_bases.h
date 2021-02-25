#pragma once

#include "system.h"

namespace ink::runtime::internal {
	class string_table;

	/// base class for operations to acquire data and provide flags and
	/// constructor
	template<typename ...>
	class operation_base {
	public:
		static constexpr bool enabled = false;
		template<typename T>
		operation_base(const T&) { static_assert(always_false<T>::value, "use undefined base!"); }
	};

	template<>
	class operation_base<void> {
	public:
		static constexpr bool enabled = true;
		template<typename T>
		operation_base(const T&) {}
	};

	// base class for operations which needs a string_table
	template<>
	class operation_base<string_table> {
	public:
		static constexpr bool enabled = true;
		template<typename T>
		operation_base(const T& t) : _string_table{*t_get<string_table*>(t)} {
			static_assert(has_type<string_table*,T>::value, "Executioner "
					"constructor needs a string table to instantiate "
					"some operations!");
		}

	protected:
		string_table& _string_table;
	};
}
