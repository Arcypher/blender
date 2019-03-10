#pragma once

#include <atomic>
#include <utility>
#include "BLI_utildefines.h"

namespace BLI {

	class RefCountedBase {
	private:
		std::atomic<int> m_refcount;

	protected:
		virtual ~RefCountedBase() {};

		RefCountedBase()
			: m_refcount(1) {}

	public:
		void incref()
		{
			m_refcount.fetch_add(1);
		}

		void decref()
		{
			int new_value = m_refcount.fetch_sub(1) - 1;
			BLI_assert(new_value >= 0);
			if (new_value == 0) {
				delete this;
			}
		}

		int refcount() const
		{
			return m_refcount;
		}
	};

	template<typename T>
	class AutoRefCount {
	private:
		T *m_object;

		AutoRefCount(T *object)
			: m_object(object) {}

		inline void incref()
		{
			m_object->incref();
		}

		inline void decref()
		{
			m_object->decref();
		}

	public:
		AutoRefCount()
			: AutoRefCount(new T()) {}

		template<typename ...Args>
		static AutoRefCount<T> New(Args&&... args)
		{
			T *object = new T(std::forward<Args>(args)...);
			return AutoRefCount<T>(object);
		}

		static AutoRefCount<T> FromPointer(T *object)
		{
			return AutoRefCount<T>(object);
		}

		AutoRefCount(const AutoRefCount &other)
		{
			m_object = other.m_object;
			this->incref();
		}

		AutoRefCount(AutoRefCount &&other)
		{
			m_object = other.m_object;
			other.m_object = nullptr;
		}

		~AutoRefCount()
		{
			/* Can be nullptr when previously moved. */
			if (m_object != nullptr) {
				this->decref();
			}
		}

		AutoRefCount &operator=(const AutoRefCount &other)
		{
			if (this == &other) {
				return *this;
			}
			else if (m_object == other.m_object) {
				return *this;
			}
			else {
				this->decref();
				m_object = other.m_object;
				this->incref();
				return *this;
			}
		}

		AutoRefCount &operator=(AutoRefCount &&other)
		{
			if (this == &other) {
				return *this;
			}
			else if (m_object == other.m_object) {
				other.m_object = nullptr;
				return *this;
			}
			else {
				this->decref();
				m_object = other.m_object;
				other.m_object = nullptr;
				return *this;
			}
		}

		T *ptr() const
		{
			return m_object;
		}

		T *operator->() const
		{
			return this->ptr();
		}

		friend bool operator==(const AutoRefCount &a, const AutoRefCount &b)
		{
			return a.m_object == b.m_object;
		}

		friend bool operator!=(const AutoRefCount &a, const AutoRefCount &b)
		{
			return !(a == b);
		}
	};

} /* namespace BLI */