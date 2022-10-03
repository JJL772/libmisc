
#pragma once

#include <chrono>
#include <stack>
#include <iostream>

#include "bits/libmisc.hpp"

namespace LIBMISC_NAMESPACE {

	// Dead simple timer class
	template<class TimerT = std::chrono::high_resolution_clock>
	class timer {
	public:
		// Start recording the time fresh
		inline void begin() {
			auto now = TimerT::now();
			m_start = now;
		}

		// Stop recording the time
		inline void end() {
			record_time();
		}

		// Get the current time
		template<typename DurationT>
		inline double get() const {
			record_time();
			return std::chrono::duration_cast<DurationT>(m_dur).count();
		}

		inline double get_ms() const { return get<std::chrono::milliseconds>(); }
		inline double get_us() const { return get<std::chrono::microseconds>(); }
		inline double get_seconds() const { return get<std::chrono::seconds>(); }
		inline double get_hours() const { return get<std::chrono::hours>(); }

		inline void display(std::ostream& stream = std::cout) {
			stream << get_seconds() << "." << get_ms() - (get_seconds()*1000) << " seconds (" << get_ms() << " ms)" << std::endl;
		}

	private:
		inline void record_time() const {
			auto now = TimerT::now();
			m_dur = now - m_start;
		}

	private:
		using timePointT = std::chrono::time_point<TimerT>;
		using durationT = std::chrono::duration<double>;

		mutable durationT m_dur;
		timePointT m_start;
	};

}
