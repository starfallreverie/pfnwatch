// main.cpp
#include "project/global.hpp"

static std::unordered_map<unsigned long long, core::tracked_detection> g_tracked;

static void merge_snapshot( const shared::ioctl::detection* snapshot, unsigned long count )
{
	for ( unsigned long i{ 0u }; i < count; i++ )
	{
		const auto& d = snapshot[ i ];
		auto it = g_tracked.find( d.pfn );

		if ( it != g_tracked.end( ) )
		{
			it->second.virtual_address = d.virtual_address;
			it->second.pte_value = d.pte_value;
			it->second.target_address = d.target_address;
			it->second.reference_count = d.reference_count;
			it->second.cache_type = d.cache_type;
			it->second.total_ticks += d.tick_count;
		}
		else
		{
			g_tracked[ d.pfn ] = {
				d.pfn,
				d.virtual_address,
				d.pte_value,
				d.target_address,
				d.reference_count,
				d.cache_type,
				d.tick_count
			};
		}
	}
}

static std::vector<core::tracked_detection> sorted_detections( )
{
	std::vector<core::tracked_detection> result;
	result.reserve( g_tracked.size( ) );

	for ( const auto& [pfn, entry] : g_tracked ) {
		result.push_back( entry );
	}

	std::sort( result.begin( ), result.end( ), [ ]( const auto& a, const auto& b ) {
		return a.total_ticks > b.total_ticks;
	} );

	return result;
}

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

	auto* snapshot = new shared::ioctl::detection[ 4096 ];
	auto last_draw = ::GetTickCount64( );

	while ( true )
	{
		std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );

		const auto count = g.m_driver.read_report( snapshot, 4096u );
		if ( count > 0u ) {
			merge_snapshot( snapshot, count );
		}

		const auto now = ::GetTickCount64( );
		if ( now - last_draw < 250ull ) {
			continue;
		}

		last_draw = now;

		const auto detections = sorted_detections( );
		if ( detections.empty( ) ) {
			continue;
		}

		g.m_console.render( detections );
	}

	g.m_console.shutdown( );
	delete[ ] snapshot;
	g.m_driver.close( );
	return 0;
}