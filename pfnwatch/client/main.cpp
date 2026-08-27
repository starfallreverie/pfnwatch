// client/main.cpp
#include "project/global.hpp"

int main( )
{
	g.m_console.initialize( );

	if ( !g.m_driver.open( ) )
	{
		std::printf( "failed to open device\n" );
		return 1;
	}

	const auto pid = ::GetCurrentProcessId( );
	if ( !g.m_driver.assign_process( pid ) )
	{
		std::printf( "failed to assign process\n" );
		g.m_driver.close( );
		return 1;
	}

	g.m_reader.start( );

	std::unordered_map<std::uint64_t, core::tracked_detection> tracked;
	auto* snapshot = new shared::ioctl::detection[ 4096 ];
	auto last_draw = ::GetTickCount64( );

	while ( true )
	{
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

		const auto count = g.m_driver.read_report( snapshot, 4096u );
		for ( std::uint32_t i{ 0u }; i < count; i++ )
		{
			const auto& entry = snapshot[ i ];
			auto it = tracked.find( entry.pfn );

			if ( it != tracked.end( ) )
			{
				it->second.virtual_address = entry.virtual_address;
				it->second.pte_value = entry.pte_value;
				it->second.target_address = entry.target_address;
				it->second.reference_count = entry.reference_count;
				it->second.cache_type = entry.cache_type;
				it->second.total_ticks += entry.tick_count;
			}
			else {
				tracked[ entry.pfn ] = { entry.pfn, entry.virtual_address, entry.pte_value, entry.target_address, entry.reference_count, entry.cache_type, entry.tick_count };
			}
		}

		const auto now = ::GetTickCount64( );
		if ( now - last_draw < 250ull ) {
			continue;
		}

		last_draw = now;

		std::vector<core::tracked_detection> detections;
		detections.reserve( tracked.size( ) );

		for ( const auto& [pfn, entry] : tracked ) {
			detections.push_back( entry );
		}

		std::sort( detections.begin( ), detections.end( ), [ ]( const auto& a, const auto& b ) { return a.total_ticks > b.total_ticks; } );

		if ( !detections.empty( ) ) {
			g.m_console.render( detections );
		}
	}

	g.m_console.shutdown( );
	g.m_driver.close( );
	delete[ ] snapshot;

	return 0;
}