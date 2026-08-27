// project/core/scanner/scanner.cpp
#include "../../global.hpp"

namespace core {

	void scanner::tick( )
	{
		if ( g.m_collector.collect( ) ) {
			this->cache_hot_pt_pages( );
		}

		if ( !g.m_collector.ready( ) ) {
			return;
		}

		this->scan_kernel_ptes( );
	}

	void scanner::reset( )
	{
		this->m_detection_count = 0u;
		this->m_cold_cursor = 256u;
	}

	void scanner::cleanup( )
	{
		if ( this->m_detections )
		{
			::ExFreePoolWithTag( this->m_detections, 'pfnw' );
			this->m_detections = nullptr;
			this->m_detection_capacity = 0u;
		}

		if ( this->m_cached_pts )
		{
			::ExFreePoolWithTag( this->m_cached_pts, 'pfnw' );
			this->m_cached_pts = nullptr;
			this->m_cached_pt_count = 0u;
			this->m_cached_pt_capacity = 0u;
		}
	}

	void scanner::reset_detections( )
	{
		this->m_detection_count = 0u;
	}

	void scanner::scan_kernel_ptes( )
	{
		this->scan_cached_hot( );
		this->scan_cold_entry( );
	}

	void scanner::cache_hot_pt_pages( )
	{
		this->m_cached_pt_count = 0u;

		const auto* pxe = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pxe_base );
		if ( !( pxe[ g.m_kernel.m_hot_pml4_index ] & 1ull ) ) {
			return;
		}

		auto va_base = static_cast< unsigned long long >( g.m_kernel.m_hot_pml4_index ) << 39ull;
		if ( va_base & ( 1ull << 47ull ) ) {
			va_base |= 0xffff000000000000ull;
		}

		const auto pml4_index = ( va_base >> 39ull ) & 0x1ffull;
		const auto* pdpt = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_ppe_base + pml4_index * 0x1000ull );

		for ( unsigned long i{ 0u }; i < 512u; i++ )
		{
			if ( !( pdpt[ i ] & 1ull ) or ( pdpt[ i ] & 0x80ull ) ) {
				continue;
			}

			const auto pdpt_va = va_base | ( static_cast< unsigned long long >( i ) << 30ull );
			const auto pd_flat = ( pdpt_va >> 30ull ) & 0x3ffffull;
			const auto* pd = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pde_base + pd_flat * 0x1000ull );

			for ( unsigned long j{ 0u }; j < 512u; j++ )
			{
				if ( !( pd[ j ] & 1ull ) or ( pd[ j ] & 0x80ull ) ) {
					continue;
				}

				const auto pd_va = pdpt_va | ( static_cast< unsigned long long >( j ) << 21ull );
				const auto pt_flat = ( pd_va >> 21ull ) & 0x7ffffffull;

				if ( this->m_cached_pt_count >= this->m_cached_pt_capacity )
				{
					const auto new_capacity = this->m_cached_pt_capacity ? this->m_cached_pt_capacity * 2u : 4096u;
					auto* new_buffer = static_cast< cached_pt* >( ::ExAllocatePool2( POOL_FLAG_NON_PAGED, new_capacity * sizeof( cached_pt ), 'pfnw' ) );

					if ( !new_buffer ) {
						return;
					}

					if ( this->m_cached_pts )
					{
						::memcpy( new_buffer, this->m_cached_pts, this->m_cached_pt_count * sizeof( cached_pt ) );
						::ExFreePoolWithTag( this->m_cached_pts, 'pfnw' );
					}

					this->m_cached_pts = new_buffer;
					this->m_cached_pt_capacity = new_capacity;
				}

				auto& entry = this->m_cached_pts[ this->m_cached_pt_count ];
				entry.table_va = g.m_kernel.m_pte_base + pt_flat * 0x1000ull;
				entry.va_base = pd_va;
				this->m_cached_pt_count++;
			}
		}
	}

	void scanner::scan_cached_hot( )
	{
		for ( unsigned long i{ 0u }; i < this->m_cached_pt_count; i++ )
		{
			const auto va_base = this->m_cached_pts[ i ].va_base;

			const auto pd_flat = ( va_base >> 30ull ) & 0x3ffffull;
			const auto pd_index = ( va_base >> 21ull ) & 0x1ffull;
			const auto* pd = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pde_base + pd_flat * 0x1000ull );

			if ( !( pd[ pd_index ] & 1ull ) or ( pd[ pd_index ] & 0x80ull ) ) {
				continue;
			}

			const auto* table = reinterpret_cast< const unsigned long long* >( this->m_cached_pts[ i ].table_va );

			for ( unsigned long j{ 0u }; j < 512u; j++ )
			{
				if ( !( table[ j ] & 1ull ) ) {
					continue;
				}

				const auto pfn = ( table[ j ] >> 12ull ) & 0xffffffffffull;

				if ( g.m_collector.check_pfn( pfn ) ) {
					this->record_detection( pfn, va_base | ( static_cast< unsigned long long >( j ) << 12ull ), table[ j ] );
				}
			}
		}
	}

	void scanner::scan_cold_entry( )
	{
		while ( this->m_cold_cursor == g.m_kernel.m_hot_pml4_index or this->m_cold_cursor == g.m_kernel.m_self_ref_index )
		{
			this->m_cold_cursor++;
			if ( this->m_cold_cursor >= 512u ) {
				this->m_cold_cursor = 256u;
			}
		}

		const auto* pxe = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pxe_base );
		if ( pxe[ this->m_cold_cursor ] & 1ull )
		{
			auto va_base = static_cast< unsigned long long >( this->m_cold_cursor ) << 39ull;
			if ( va_base & ( 1ull << 47ull ) ) {
				va_base |= 0xffff000000000000ull;
			}

			this->scan_pdpt( va_base );
		}

		this->m_cold_cursor++;
		if ( this->m_cold_cursor >= 512u ) {
			this->m_cold_cursor = 256u;
		}
	}

	void scanner::scan_pdpt( unsigned long long va_base )
	{
		const auto pml4_index = ( va_base >> 39ull ) & 0x1ffull;
		const auto* table = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_ppe_base + pml4_index * 0x1000ull );

		for ( unsigned long i{ 0u }; i < 512u; i++ )
		{
			if ( !( table[ i ] & 1ull ) ) {
				continue;
			}

			if ( table[ i ] & 0x80ull ) {
				continue;
			}

			this->scan_pd( va_base | ( static_cast< unsigned long long >( i ) << 30ull ) );
		}
	}

	void scanner::scan_pd( unsigned long long va_base )
	{
		const auto flat_index = ( va_base >> 30ull ) & 0x3ffffull;
		const auto* table = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pde_base + flat_index * 0x1000ull );

		for ( unsigned long i{ 0u }; i < 512u; i++ )
		{
			if ( !( table[ i ] & 1ull ) ) {
				continue;
			}

			if ( table[ i ] & 0x80ull ) {
				continue;
			}

			this->scan_pt( va_base | ( static_cast< unsigned long long >( i ) << 21ull ) );
		}
	}

	void scanner::scan_pt( unsigned long long va_base )
	{
		const auto flat_index = ( va_base >> 21ull ) & 0x7ffffffull;
		const auto* table = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pte_base + flat_index * 0x1000ull );

		for ( unsigned long i{ 0u }; i < 512u; i++ )
		{
			if ( !( table[ i ] & 1ull ) ) {
				continue;
			}

			const auto pfn = ( table[ i ] >> 12ull ) & 0xffffffffffull;

			if ( g.m_collector.check_pfn( pfn ) ) {
				this->record_detection( pfn, va_base | ( static_cast< unsigned long long >( i ) << 12ull ), table[ i ] );
			}
		}
	}

	void scanner::record_detection( unsigned long long pfn, unsigned long long virtual_address, unsigned long long pte_value )
	{
		if ( !g.m_collector.verify_pfn( pfn ) ) {
			return;
		}

		const auto cache = pte_value ? static_cast< unsigned char >( ( ( pte_value >> 5ull ) & 4ull ) | ( ( pte_value >> 3ull ) & 3ull ) ) : static_cast< unsigned char >( 0xffu );

		for ( unsigned long i{ 0u }; i < this->m_detection_count; i++ )
		{
			if ( this->m_detections[ i ].pfn == pfn )
			{
				this->m_detections[ i ].virtual_address = virtual_address;
				this->m_detections[ i ].pte_value = pte_value;
				this->m_detections[ i ].cache_type = cache;
				this->m_detections[ i ].tick_count++;
				return;
			}
		}

		if ( this->m_detection_count >= this->m_detection_capacity )
		{
			const auto new_capacity = this->m_detection_capacity ? this->m_detection_capacity * 2u : 64u;
			auto* new_buffer = static_cast< shared::ioctl::detection* >( ::ExAllocatePool2( POOL_FLAG_NON_PAGED, new_capacity * sizeof( shared::ioctl::detection ), 'pfnw' ) );

			if ( !new_buffer ) {
				return;
			}

			if ( this->m_detections )
			{
				::memcpy( new_buffer, this->m_detections, this->m_detection_count * sizeof( shared::ioctl::detection ) );
				::ExFreePoolWithTag( this->m_detections, 'pfnw' );
			}

			this->m_detections = new_buffer;
			this->m_detection_capacity = new_capacity;
		}

		const auto* database = static_cast< const nt::mmpfn* >( g.m_kernel.m_pfn_database );

		auto& entry = this->m_detections[ this->m_detection_count ];
		entry.pfn = pfn;
		entry.virtual_address = virtual_address;
		entry.pte_value = pte_value;
		entry.target_address = g.m_collector.resolve_target_va( pfn );
		entry.reference_count = database[ pfn ].u3.reference_count;
		entry.cache_type = cache;
		entry.tick_count = 1u;

		this->m_detection_count++;
	}

} // namespace core