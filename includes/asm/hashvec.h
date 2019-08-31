#ifndef __ASM_HASHVEC_H__
#define __ASM_HASHVEC_H__

#include <vector>
#include <unordered_map>
#include <string>

namespace ASM {
	struct hashvec_traits {
		static constexpr bool icase = false;
	};

	struct hashvec_traits_icase : hashvec_traits {
		static constexpr bool icase = true;
	};

	template <typename T, typename traits = hashvec_traits>
	class HashVec {
		struct mapped_type : public T {
			const std::string key;
			const int index;
			mapped_type(std::string key, int index, const T& value) : T(value), key(key), index(index) {}
			mapped_type& operator=(const T& value) {
				T::operator=(value);
				return *this;
			}
		};
		struct init_type {
			const std::string key;
			T value;
		};
		std::unordered_map<std::string, int> map;
		std::vector<mapped_type> vec;
	public:
		HashVec() = default;
		HashVec(std::initializer_list<init_type> list) {
			for (auto& elem : list) {
				put(elem.key, elem.value);
			}
		}
		void put(const std::string& key, const T& value) {
			vec.push_back(mapped_type(key, vec.size(), value));
			map[key] = vec.size() - 1;
		}

		bool has(const std::string& key) {
			if constexpr (traits::icase)
				return map.count(utils::tolower(key));
			else
				return map.count(key);
		}

		auto begin() const {
			return vec.begin();
		}
		auto end() const {
			return vec.end();
		}
		auto begin() {
			return vec.begin();
		}
		auto end() {
			return vec.end();
		}
		auto size() const {
			return vec.size();
		}
		mapped_type& operator[](unsigned int index) {
			return vec[index];
		}
		mapped_type& operator[](const std::string& key) {
			if (!has(key))
				put(key, T{});

			if constexpr (traits::icase)
				return vec[map[utils::tolower(key)]];
			else
				return vec[map[key]];
		}

		template <typename U>
		friend std::ostream& operator<<(std::ostream& stream, const HashVec<U>& hashvec);
	};
}

#endif