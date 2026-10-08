// project/core/scanner/scanner.hpp
#pragma once

namespace core {

	class scanner
	{
	public:
		void tick( );
		void reset( );
		void cleanup( );
		void reset_detections( );

		shared::ioctl::detection* m_detections;
		unsigned long m_detection_count;
		unsigned long m_detection_capacity;
		KSPIN_LOCK m_lock;

	private:
		void scan_kernel_ptes( );

		void cache_hot_pt_pages( );
		void scan_cached_hot( );
		void scan_cold_entry( );

		void scan_pdpt( unsigned long long va_base );
		void scan_pd( unsigned long long va_base );
		void scan_pt( unsigned long long va_base );

		void record_detection( unsigned long long pfn, unsigned long long virtual_address, unsigned long long pte_value );

	private:
		unsigned long m_cold_cursor;

		struct cached_pt
		{
			unsigned long long table_va;
			unsigned long long va_base;
		};

		cached_pt* m_cached_pts;
		unsigned long m_cached_pt_count;
		unsigned long m_cached_pt_capacity;
	};

} // namespace core