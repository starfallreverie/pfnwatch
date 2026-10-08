// project/core/kernel/kernel.hpp
#pragma once

namespace core {

	class kernel
	{
	public:
		bool initialize( );

		void* m_ntoskrnl_base;
		unsigned long m_ntoskrnl_size;
		void* m_pfn_database;

		unsigned long long m_pte_base;
		unsigned long long m_pde_base;
		unsigned long long m_ppe_base;
		unsigned long long m_pxe_base;
		unsigned long m_self_ref_index;
		unsigned long m_hot_pml4_index;

	private:
		void* find_pfn_database( );
		bool resolve_pte_base( );
		void resolve_hot_pml4( );
	};

} // namespace core