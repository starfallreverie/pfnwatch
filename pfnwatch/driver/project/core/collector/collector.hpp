// project/core/collector/collector.hpp
#pragma once

namespace core {

	class collector
	{
	public:
		NTSTATUS assign( unsigned long pid );
		bool collect( );
		void cleanup( );

		bool ready( ) const;
		bool check_pfn( unsigned long long pfn ) const;
		bool verify_pfn( unsigned long long pfn ) const;
		unsigned long long resolve_target_va( unsigned long long pfn ) const;

	private:
		void resolve_image_range( );
		void ensure_bitmap( );

		void walk_pdpt( unsigned long long va_base );
		void walk_pd( unsigned long long va_base );
		void walk_pt( unsigned long long va_base );

		void mark_pfn( unsigned long long pfn, unsigned long long va );
		void record_image_pfn( unsigned long long pfn, unsigned long long va );

	private:
		void* m_target_process;

		unsigned long long m_image_base;
		unsigned long long m_image_end;

		unsigned char* m_pfn_bitmap;
		unsigned long long m_pfn_bitmap_size;

		unsigned long m_tick_count;

		struct image_pfn
		{
			unsigned long long pfn;
			unsigned long long va;
		};

		image_pfn* m_image_pfns;
		unsigned long m_image_pfn_count;
		unsigned long m_image_pfn_capacity;
	};

} // namespace core