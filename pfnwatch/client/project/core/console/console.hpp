// project/console/console.hpp
#pragma once

namespace core {

	struct tracked_detection
	{
		unsigned long long pfn;
		unsigned long long virtual_address;
		unsigned long long pte_value;
		unsigned long long target_address;
		unsigned short reference_count;
		unsigned char cache_type;
		unsigned long total_ticks;
	};

	class console
	{
	public:
		void initialize( );
		void shutdown( );
		void render( const std::vector<tracked_detection>& detections );

	private:
		void render_header( );
		void render_row( const tracked_detection& detection );
		void render_footer( unsigned long count );

		void resolve_target( const tracked_detection& detection, char* out, unsigned long max_length );
		void format_flags( unsigned long long pte, char* out, unsigned long max_length );
		const char* tick_color( unsigned long ticks );
	};

} // namespace core