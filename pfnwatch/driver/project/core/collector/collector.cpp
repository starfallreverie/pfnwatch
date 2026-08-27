// project/core/collector/collector.cpp
#include "../../global.hpp"

namespace core {

	NTSTATUS collector::assign( unsigned long pid )
	{
		PEPROCESS process{ nullptr };
		const auto status = ::PsLookupProcessByProcessId( reinterpret_cast< void* >( static_cast< uintptr_t >( pid ) ), &process );

		if ( !NT_SUCCESS( status ) ) {
			return status;
		}

		if ( this->m_target_process ) {
			::ObfDereferenceObject( this->m_target_process );
		}

		this->m_target_process = process;
		this->m_tick_count = 0u;

		this->resolve_image_range( );

		return STATUS_SUCCESS;
	}

	bool collector::collect( )
	{
		if ( !this->m_target_process ) {
			return false;
		}

		if ( this->m_pfn_bitmap and this->m_tick_count % 100u != 0u )
		{
			this->m_tick_count++;
			return false;
		}

		this->ensure_bitmap( );
		if ( !this->m_pfn_bitmap )
		{
			this->m_tick_count++;
			return false;
		}

		::memset( this->m_pfn_bitmap, 0, this->m_pfn_bitmap_size );
		this->m_image_pfn_count = 0u;

		KAPC_STATE apc_state{};
		::KeStackAttachProcess( static_cast< PEPROCESS >( this->m_target_process ), &apc_state );

		const auto* pxe = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pxe_base );
		for ( unsigned long i{ 0u }; i < 256u; i++ )
		{
			if ( !( pxe[ i ] & 1ull ) ) {
				continue;
			}

			this->walk_pdpt( static_cast< unsigned long long >( i ) << 39ull );
		}

		::KeUnstackDetachProcess( &apc_state );

		this->m_tick_count++;
		return true;
	}

	void collector::cleanup( )
	{
		if ( this->m_target_process )
		{
			::ObfDereferenceObject( this->m_target_process );
			this->m_target_process = nullptr;
		}

		if ( this->m_pfn_bitmap )
		{
			::ExFreePoolWithTag( this->m_pfn_bitmap, 'pfnw' );
			this->m_pfn_bitmap = nullptr;
		}

		if ( this->m_image_pfns )
		{
			::ExFreePoolWithTag( this->m_image_pfns, 'pfnw' );
			this->m_image_pfns = nullptr;
			this->m_image_pfn_count = 0u;
			this->m_image_pfn_capacity = 0u;
		}

		this->m_image_base = 0ull;
		this->m_image_end = 0ull;
	}

	bool collector::ready( ) const
	{
		return this->m_pfn_bitmap != nullptr;
	}

	bool collector::check_pfn( unsigned long long pfn ) const
	{
		if ( pfn >= this->m_pfn_bitmap_size * 8ull ) {
			return false;
		}

		return ( this->m_pfn_bitmap[ pfn >> 3ull ] >> ( pfn & 7ull ) ) & 1u;
	}

	bool collector::verify_pfn( unsigned long long pfn ) const
	{
		const auto* database = static_cast< const nt::mmpfn* >( g.m_kernel.m_pfn_database );
		const auto& entry = database[ pfn ];

		return entry.u3.e1.page_location == nt::mmlists::active_and_valid;
	}

	unsigned long long collector::resolve_target_va( unsigned long long pfn ) const
	{
		for ( unsigned long i{ 0u }; i < this->m_image_pfn_count; i++ )
		{
			if ( this->m_image_pfns[ i ].pfn == pfn ) {
				return this->m_image_pfns[ i ].va;
			}
		}

		const auto* database = static_cast< const nt::mmpfn* >( g.m_kernel.m_pfn_database );
		const auto pte_addr = reinterpret_cast< unsigned long long >( database[ pfn ].pte_address );

		return ( pte_addr - g.m_kernel.m_pte_base ) << 9ull;
	}

	void collector::resolve_image_range( )
	{
		this->m_image_base = 0ull;
		this->m_image_end = 0ull;

		const auto base = reinterpret_cast< unsigned long long >( ::PsGetProcessSectionBaseAddress( static_cast< PEPROCESS >( this->m_target_process ) ) );
		if ( !base ) {
			return;
		}

		KAPC_STATE apc_state{};
		::KeStackAttachProcess( static_cast< PEPROCESS >( this->m_target_process ), &apc_state );

		__try
		{
			const auto* dos = reinterpret_cast< const IMAGE_DOS_HEADER* >( base );
			const auto* nt = reinterpret_cast< const IMAGE_NT_HEADERS* >( base + dos->e_lfanew );

			if ( dos->e_magic == IMAGE_DOS_SIGNATURE and nt->Signature == IMAGE_NT_SIGNATURE )
			{
				this->m_image_base = base;
				this->m_image_end = base + nt->OptionalHeader.SizeOfImage;
			}
		}
		__except ( EXCEPTION_EXECUTE_HANDLER )
		{
		}

		::KeUnstackDetachProcess( &apc_state );
	}

	void collector::ensure_bitmap( )
	{
		if ( this->m_pfn_bitmap ) {
			return;
		}

		unsigned long long max_pfn{ 0x100000ull };

		const auto* ranges = ::MmGetPhysicalMemoryRanges( );
		if ( ranges )
		{
			for ( unsigned long i{ 0u }; ranges[ i ].BaseAddress.QuadPart or ranges[ i ].NumberOfBytes.QuadPart; i++ )
			{
				const auto end = ( ranges[ i ].BaseAddress.QuadPart + ranges[ i ].NumberOfBytes.QuadPart ) >> 12ull;
				if ( static_cast< unsigned long long >( end ) > max_pfn ) {
					max_pfn = static_cast< unsigned long long >( end );
				}
			}

			::ExFreePool( const_cast< PHYSICAL_MEMORY_RANGE* >( ranges ) );
		}

		this->m_pfn_bitmap_size = ( max_pfn + 7ull ) / 8ull;
		this->m_pfn_bitmap = static_cast< unsigned char* >( ::ExAllocatePool2( POOL_FLAG_NON_PAGED, this->m_pfn_bitmap_size, 'pfnw' ) );
	}

	void collector::walk_pdpt( unsigned long long va_base )
	{
		const auto pml4_index = ( va_base >> 39ull ) & 0x1ffull;
		const auto* table = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_ppe_base + pml4_index * 0x1000ull );

		for ( unsigned long i{ 0u }; i < 512u; i++ )
		{
			if ( !( table[ i ] & 1ull ) ) {
				continue;
			}

			if ( table[ i ] & 0x80ull )
			{
				const auto base_pfn = ( table[ i ] >> 12ull ) & 0xffffffffffull;
				const auto large_va = va_base | ( static_cast< unsigned long long >( i ) << 30ull );

				for ( unsigned long long j{ 0ull }; j < 0x40000ull; j++ ) {
					this->mark_pfn( base_pfn + j, large_va + ( j << 12ull ) );
				}
				continue;
			}

			this->walk_pd( va_base | ( static_cast< unsigned long long >( i ) << 30ull ) );
		}
	}

	void collector::walk_pd( unsigned long long va_base )
	{
		const auto flat_index = ( va_base >> 30ull ) & 0x3ffffull;
		const auto* table = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pde_base + flat_index * 0x1000ull );

		for ( unsigned long i{ 0u }; i < 512u; i++ )
		{
			if ( !( table[ i ] & 1ull ) ) {
				continue;
			}

			if ( table[ i ] & 0x80ull )
			{
				const auto base_pfn = ( table[ i ] >> 12ull ) & 0xffffffffffull;
				const auto large_va = va_base | ( static_cast< unsigned long long >( i ) << 21ull );

				for ( unsigned long long j{ 0ull }; j < 0x200ull; j++ ) {
					this->mark_pfn( base_pfn + j, large_va + ( j << 12ull ) );
				}
				continue;
			}

			this->walk_pt( va_base | ( static_cast< unsigned long long >( i ) << 21ull ) );
		}
	}

	void collector::walk_pt( unsigned long long va_base )
	{
		const auto flat_index = ( va_base >> 21ull ) & 0x7ffffffull;
		const auto* table = reinterpret_cast< const unsigned long long* >( g.m_kernel.m_pte_base + flat_index * 0x1000ull );

		for ( unsigned long i{ 0u }; i < 512u; i++ )
		{
			if ( !( table[ i ] & 1ull ) ) {
				continue;
			}

			const auto pfn = ( table[ i ] >> 12ull ) & 0xffffffffffull;
			const auto va = va_base | ( static_cast< unsigned long long >( i ) << 12ull );

			this->mark_pfn( pfn, va );
		}
	}

	void collector::mark_pfn( unsigned long long pfn, unsigned long long va )
	{
		if ( pfn >= this->m_pfn_bitmap_size * 8ull ) {
			return;
		}

		const auto* database = static_cast< const nt::mmpfn* >( g.m_kernel.m_pfn_database );
		const auto& entry = database[ pfn ];

		if ( entry.u3.e1.page_location != nt::mmlists::active_and_valid ) {
			return;
		}

		if ( entry.u4.prototype_pte and ( va < this->m_image_base or va >= this->m_image_end ) ) {
			return;
		}

		this->m_pfn_bitmap[ pfn >> 3ull ] |= ( 1u << ( pfn & 7ull ) );

		if ( entry.u4.prototype_pte ) {
			this->record_image_pfn( pfn, va );
		}
	}

	void collector::record_image_pfn( unsigned long long pfn, unsigned long long va )
	{
		if ( this->m_image_pfn_count >= this->m_image_pfn_capacity )
		{
			const auto new_capacity = this->m_image_pfn_capacity ? this->m_image_pfn_capacity * 2u : 1024u;
			auto* new_buffer = static_cast< image_pfn* >( ::ExAllocatePool2( POOL_FLAG_NON_PAGED, new_capacity * sizeof( image_pfn ), 'pfnw' ) );

			if ( !new_buffer ) {
				return;
			}

			if ( this->m_image_pfns )
			{
				::memcpy( new_buffer, this->m_image_pfns, this->m_image_pfn_count * sizeof( image_pfn ) );
				::ExFreePoolWithTag( this->m_image_pfns, 'pfnw' );
			}

			this->m_image_pfns = new_buffer;
			this->m_image_pfn_capacity = new_capacity;
		}

		auto& entry = this->m_image_pfns[ this->m_image_pfn_count ];
		entry.pfn = pfn;
		entry.va = va;
		this->m_image_pfn_count++;
	}

} // namespace core