/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "../stl_ext.h"
#include "./type_traits.h"
#include <cstdlib>

namespace STL_STD_EXT_NAMESPACE_EX {

	//
	// wrapper for classes with Print interface or printf_P() method
	//
	// converts:
	//
	// nullptr to "<nullptr>"
	// missing arguments to "<noarg>"
	// String and std::string to consts char *
	// IPAddress to consts char *
	//
	//
	// TODO
	// restrict access to data with _HeapStart, _HeapEnd
	// try toString() and to_string() for objects
	//
	template<typename _Print, size_t _HeapStart = 0, size_t HeapEnd = ~0UL>
	class printf_wrapper {
	public:
		printf_wrapper(_Print &output) : _output(output) {}

		template<typename ..._Args>
		void printf_P(const char *format, const _Args&...args) {
			_collect(args...);
			if (_args.size() <= 8) {
				_output.printf_P(format, _get(0), _get(1), _get(2), _get(3), _get(4), _get(5), _get(6), _get(7));
			}
			else if (_args.size() <= 16) {
				_output.printf_P(format, _get(0), _get(1), _get(2), _get(3), _get(4), _get(5), _get(6), _get(7), _get(8), _get(9), _get(10), _get(11), _get(12), _get(13), _get(14), _get(15));
			}
			else {
				_output.printf_P(format, _get(0), _get(1), _get(2), _get(3), _get(4), _get(5), _get(6), _get(7), _get(8), _get(9), _get(10), _get(11), _get(12), _get(13), _get(14), _get(15), _get(16), _get(17), _get(18), _get(19), _get(20), _get(21), _get(22), _get(23), _get(24), _get(25), _get(26), _get(27), _get(28));
			}

			_args.clear();
			_strings.clear();
		}

	private:
		uintptr_t _get(size_t pos) {
			if (pos >= _args.size()) {
				return (uintptr_t)PSTR("<noarg>");
			}
			return _args[pos];
		}

		void _collect() {
		}

		void _process(const char value) {
			_process((intptr_t)value);
		}

		void _process(const int8_t value) {
			_process((intptr_t)value);
		}

		void _process(const int16_t value) {
			_process((intptr_t)value);
		}

		void _process(const std::string &str) {
			_process((uintptr_t)str.c_str());
		}

		void _process(const String &str) {
			_process((uintptr_t)str.c_str());
		}

		void _process(const IPAddress &addr) {
			_strings.emplace_back(std::move(addr.toString()));
			_process((uintptr_t)_strings.back().c_str());
		}

		void _process(const __FlashStringHelper *str) {
			if (str == nullptr) {
				_collect((uintptr_t)PSTR("<nullptr>"));
			}
			else {
				_collect((uintptr_t)str);
			}
		}

		template<typename _Ta>
		void _process(const _Ta &arg) {
			static_assert(sizeof(arg) <= sizeof(uintptr_t) || sizeof(arg) == sizeof(uint64_t), "invalid type");
			if (sizeof(arg) <= sizeof(uintptr_t)) {
				_args.push_back((uintptr_t)arg);
			}
			else if (sizeof(arg) == sizeof(uint64_t)) {
				std::copy_n(reinterpret_cast<const uintptr_t *>(std::addressof(arg)), 2, std::back_inserter(_args));
			}
		}

		template<typename _Ta, typename ..._Args>
		void _collect(const _Ta &arg, const _Args&...args) {
			_process(arg);
			_collect(args...);
		}

		constexpr size_t _sizeof(size_t size) {
			return size;
		}

		template<typename _Ta>
		constexpr size_t _sizeof_arg(const _Ta &arg) {
			return sizeof(arg) <= sizeof(uintptr_t) ? sizeof(uintptr_t) : (sizeof(arg) == sizeof(uint64_t) ? sizeof(uint64_t) : 0);
		}

		template<typename _Ta, typename ..._Args>
		constexpr size_t _sizeof(size_t size, const _Ta &arg, const _Args&...args) {
			return _sizeof(_sizeof_arg(arg) + size, args...);
		}


		_Print _output;
		std::vector<uintptr_t> _args;
		std::vector<String> _strings;
	};

}

