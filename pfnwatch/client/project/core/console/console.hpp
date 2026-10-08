// client/project/core/console/console.hpp
#pragma once

namespace core {

	struct tracked_detection
	{
		std::uint64_t pfn;
		std::uint64_t virtual_address;
		std::uint64_t pte_value;
		std::uint64_t target_address;
		std::uint16_t reference_count;
		std::uint8_t cache_type;
		std::uint32_t total_ticks;
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
		void render_footer( std::uint32_t count );
	};

} // namespace core