#pragma once

namespace timing {

	class limiter
	{
	public:
		explicit limiter( const std::uint32_t target_tps );
		void limit( ) noexcept;
		void set_target( std::uint32_t target_tps ) noexcept;

	private:
		LARGE_INTEGER m_start_time{};
		std::int64_t m_target_frame_duration{};
		std::int64_t m_qpc_frequency{};
	};

	class stopwatch
	{
	public:
		static void start( ) noexcept;
		static void stop( ) noexcept;

		[[nodiscard]] static float elapsed_duration( ) noexcept;

	private:
		static thread_local std::chrono::steady_clock::time_point m_start;
		static thread_local std::atomic<float> m_milliseconds;
	};

} // namespace timing
