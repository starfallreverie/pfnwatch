// project/core/kernel/kernel.cpp
#include "../../global.hpp"

namespace core {

	bool kernel::initialize( )
	{
		::RtlPcToFileHeader( reinterpret_cast< void* >( &::MmGetSystemRoutineAddress ), &this->m_ntoskrnl_base );
		if ( !this->m_ntoskrnl_base ) {
			return false;
		}

		const auto* dos = static_cast< IMAGE_DOS_HEADER* >( this->m_ntoskrnl_base );
		const auto* nt = reinterpret_cast< IMAGE_NT_HEADERS* >( reinterpret_cast< uintptr_t >( this->m_ntoskrnl_base ) + dos->e_lfanew );

		this->m_ntoskrnl_size = nt->OptionalHeader.SizeOfImage;

		const auto pfn_database = this->find_pfn_database( );
		if ( !pfn_database ) {
			return false;
		}

		this->m_pfn_database = pfn_database;

		if ( !this->resolve_pte_base( ) ) {
			return false;
		}

		this->resolve_hot_pml4( );

		return true;
	}

	void* kernel::find_pfn_database( )
	{
		UNICODE_STRING name{};
		::RtlInitUnicodeString( &name, L"MmGetVirtualForPhysical" );

		const auto* function = static_cast< const unsigned char* >( ::MmGetSystemRoutineAddress( &name ) );
		if ( !function ) {
			return nullptr;
		}

		for ( unsigned long i{ 0u }; i < 32u; i++ )
		{
			if ( function[ i ] != 0x48u ) {
				continue;
			}

			if ( function[ i + 1 ] == 0xb8u )
			{
				const auto immediate = *reinterpret_cast< const uintptr_t* >( &function[ i + 2 ] );
				return reinterpret_cast< void* >( immediate & ~0xfull );
			}

			if ( function[ i + 1 ] == 0x8bu and ( function[ i + 2 ] & 0xc7u ) == 0x05u )
			{
				const auto offset = *reinterpret_cast< const int* >( &function[ i + 3 ] );
				return *reinterpret_cast< void** >( reinterpret_cast< uintptr_t >( &function[ i + 7 ] ) + offset );
			}
		}

		return nullptr;
	}

	bool kernel::resolve_pte_base( )
	{
		const auto cr3 = ::__readcr3( ) & ~0xfffull;
		const auto cr3_pfn = cr3 >> 12ull;

		auto* pml4 = static_cast< unsigned long long* >( ::ExAllocatePool2( POOL_FLAG_NON_PAGED, 0x1000u, 'pfnw' ) );
		if ( !pml4 ) {
			return false;
		}

		PHYSICAL_ADDRESS pa{};
		pa.QuadPart = static_cast< long long >( cr3 );

		MM_COPY_ADDRESS source{};
		source.PhysicalAddress = pa;

		size_t bytes_copied{ 0 };
		const auto status = ::MmCopyMemory( pml4, source, 0x1000u, MM_COPY_MEMORY_PHYSICAL, &bytes_copied );

		if ( !NT_SUCCESS( status ) )
		{
			::ExFreePoolWithTag( pml4, 'pfnw' );
			return false;
		}

		auto index = ~0ul;
		for ( unsigned long i{ 0u }; i < 512u; i++ )
		{
			if ( !( pml4[ i ] & 1ull ) ) {
				continue;
			}

			if ( ( ( pml4[ i ] >> 12ull ) & 0xffffffffffull ) == cr3_pfn )
			{
				index = i;
				break;
			}
		}

		::ExFreePoolWithTag( pml4, 'pfnw' );

		if ( index == ~0ul ) {
			return false;
		}

		this->m_self_ref_index = index;

		auto base = static_cast< unsigned long long >( index ) << 39ull;
		if ( base & ( 1ull << 47ull ) ) {
			base |= 0xffff000000000000ull;
		}

		this->m_pte_base = base;
		this->m_pde_base = base + ( static_cast< unsigned long long >( index ) << 30ull );
		this->m_ppe_base = this->m_pde_base + ( static_cast< unsigned long long >( index ) << 21ull );
		this->m_pxe_base = this->m_ppe_base + ( static_cast< unsigned long long >( index ) << 12ull );

		return true;
	}

	void kernel::resolve_hot_pml4( )
	{
		PHYSICAL_ADDRESS pa{};
		pa.QuadPart = 0x1000ll;

		auto* mapped = ::MmMapIoSpace( pa, 0x1000u, MmNonCached );
		if ( !mapped ) {
			return;
		}

		this->m_hot_pml4_index = static_cast< unsigned long >( ( reinterpret_cast< unsigned long long >( mapped ) >> 39ull ) & 0x1ffull );

		::MmUnmapIoSpace( mapped, 0x1000u );
	}

} // namespace core