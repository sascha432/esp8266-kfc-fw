
/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <functional>

namespace FormUI {

	namespace Utility {

		template<typename _Ta>
		class ForwardList {
		public:
			template<typename _Pr>
			static _Ta *find(_Ta *iterator, _Pr pred) {
				if (iterator) {
					do {
						if (pred((*iterator))) {
							return iterator;
						}
						iterator = iterator->__next();
					} while (iterator);
				}
				return nullptr;
			}

			template<typename _Pr>
			static _Ta *next(_Ta *iterator, _Pr pred) {
				auto next = iterator->__next();
				if (next) {
					return next->find(next, pred);
				}
				return nullptr;
			}

			static void clear(_Ta **list) {
				if (*list) {
					(*list)->__clear(*list);
					*list = nullptr;
				}
			}

			static void push_back(_Ta **list, _Ta *item)
			{
				item->__next() = nullptr;
				if (*list) {
					_find_last(*list).__next() = item;
				}
				else {
					*list = item;
				}
			}

			static void push_front(_Ta **list, _Ta *item)
			{
				item->__next() = *list;
				*list = item;
			}

		protected:
			static void __clear(_Ta *iterator) {
				_Ta *next;
				do {
					next = iterator->__next();
					delete iterator;
				} while ((iterator = next) != nullptr);
			}

			_Ta *__next() const {
				return _next;
			}

			_Ta *&__next() {
				return _next;
			}

			static _Ta &_find_last(_Ta *iterator) {
				while (iterator->__next()) {
					iterator = iterator->__next();
				}
				return *iterator;
			}

		private:
			_Ta *_next;
		};

	}

}