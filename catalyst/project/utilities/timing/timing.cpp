#include <stdafx.hpp>

namespace timing {

	limiter::limiter( const std::uint32_t target_tps ) : m_start_time{ }, m_target_frame_duration( 0 ), m_qpc_frequency( 0 )
	{
		if ( target_tps == 0 )
		{
			this->m_target_frame_duration = 0;
		}
		else
		{
			LARGE_INTEGER frequency;
			QueryPerformanceFrequency( &frequency );

			this->m_qpc_frequency = frequency.QuadPart;
			this->m_target_frame_duration = this->m_qpc_frequency / target_tps;
		}

		QueryPerformanceCounter( &this->m_start_time );
	}

	void limiter::limit( ) noexcept
	{
		LARGE_INTEGER current;
		QueryPerformanceCounter( &current );

		const auto elapsed_ticks = current.QuadPart - this->m_start_time.QuadPart;
		const auto remaining_ticks = this->m_target_frame_duration - elapsed_ticks;

		if ( remaining_ticks > 0 )
		{
			const auto remaining_ms = ( remaining_ticks * 1000 ) / this->m_qpc_frequency;

			if ( remaining_ms > 1 )
			{
				std::this_thread::sleep_for( std::chrono::milliseconds( remaining_ms - 1 ) );
			}

			do
			{
				_mm_pause( );
				QueryPerformanceCounter( &current );
			} while ( ( current.QuadPart - this->m_start_time.QuadPart ) < this->m_target_frame_duration );
		}

		QueryPerformanceCounter( &this->m_start_time );
	}

	void limiter::set_target( const std::uint32_t target_tps ) noexcept
	{
		if ( target_tps == 0 )
		{
			this->m_target_frame_duration = 0;
		}
		else
		{
			this->m_target_frame_duration = this->m_qpc_frequency / target_tps;
		}
	}

	void stopwatch::start( ) noexcept
	{
		m_start = std::chrono::steady_clock::now( );
	}

	void stopwatch::stop( ) noexcept
	{
		const auto now = std::chrono::steady_clock::now( );
		m_milliseconds.store( std::chrono::duration<float, std::milli>( now - m_start ).count( ), std::memory_order_release );
	}

	float stopwatch::elapsed_duration( ) noexcept
	{
		return m_milliseconds.load( std::memory_order_acquire );
	}

	thread_local std::chrono::steady_clock::time_point timing::stopwatch::m_start{};
	thread_local std::atomic<float> timing::stopwatch::m_milliseconds{};

} // namespace timing