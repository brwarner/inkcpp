#pragma once

#include "value.h"
#include "casting.h"
#include "stack.h"

#include <tuple>

namespace ink::runtime::internal {

	constexpr size_t command_num_args(Command cmd) {
		if (cmd >= Command::BINARY_OPERATORS_START && cmd <= Command::BINARY_OPERATORS_END) {
			return 2;
		} else if (cmd >= Command::UNARY_OPERATORS_START && cmd <= Command::UNARY_OPERATORS_END) {
			return 1;
		} else {
			return 0;
		}
	}
	template<Command cmd>
	static constexpr size_t CommandNumArguments = command_num_args(cmd);

	template<Command cmd, value_type ty>
	class operation {
	public:
		template<typename T>
		operation(const T& t) {}
		void operator()(eval_stack&, value*) {
			throw ink_exception("operation not implemented!");
		}
	};

	template<Command cmd, value_type ty = value_type::BEGIN>
	class typed_executer {
	public:
		template<typename T>
		typed_executer(const T& t) : _typed_exe{t}, _op{t} {}

		void operator()(value_type t, eval_stack& s, value* v) {
			if (t == ty) { _op(s, v); }
			else { _typed_exe(t, s, v); }
		}
	private:
		typed_executer<cmd, ty+1> _typed_exe;
		operation<cmd, ty> _op;
	};
	template<Command cmd>
	class typed_executer<cmd, value_type::END> {
	public:
		template<typename T>
		typed_executer(const T& t) {}

		void operator()(value_type, eval_stack&, value*) {
			throw ink_exception("Operation for value not supported!");
		}
	};

	template<Command cmd = Command::OP_BEGIN>
	class executer_imp {
	public:
		template<typename T>
		executer_imp(const T& t) : _exe{t}, _typed_exe{t}{}

		void operator()(Command c, eval_stack& s) {
			if (c == cmd) {
				static constexpr size_t N = CommandNumArguments<cmd>;
				value args[N];
				for (int i = CommandNumArguments<cmd>-1; i >= 0 ; --i) {
					args[i] = s.pop();
				}
				value_type ty = casting::common_base<N>(args);
				_typed_exe(ty, s, args);
			} else { _exe(c, s); }
		}
	private:
		executer_imp<cmd + 1> _exe;
		typed_executer<cmd> _typed_exe;
	};
	template<>
	class executer_imp<Command::OP_END> {
	public:
		template<typename T>
		executer_imp(const T& t) {}
		void operator()(Command, eval_stack&) {
			throw ink_exception("requested command was not found!");
		}
	};

	class executer {
	public:
		template<typename ... Args>
		executer(Args& ... args) : _executer{std::tuple<Args*...>(&args...)} {}
		void operator()(Command cmd, eval_stack& stack) {
			_executer(cmd, stack);
		}
	private:
	executer_imp<Command::OP_BEGIN> _executer;
	};
}
