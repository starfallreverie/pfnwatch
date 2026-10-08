// project/common/nt.hpp
#pragma once

extern "C" PVOID NTAPI RtlPcToFileHeader( PVOID pc_value, PVOID* base_of_image );
extern "C" PVOID NTAPI PsGetProcessSectionBaseAddress( PEPROCESS process );

namespace nt {

	enum mmlists : unsigned long
	{
		zeroed_page_list = 0,
		free_page_list = 1,
		standby_page_list = 2,
		modified_page_list = 3,
		modified_no_write_page_list = 4,
		bad_page_list = 5,
		active_and_valid = 6,
		transition_page = 7,
	};

	struct mmpfn_entry1
	{
		unsigned char page_location : 3;
		unsigned char write_in_progress : 1;
		unsigned char modified : 1;
		unsigned char read_in_progress : 1;
		unsigned char cache_attribute : 2;
	};

	static_assert( sizeof( mmpfn_entry1 ) == 0x1 );

	struct mmpfn_entry3
	{
		unsigned char priority : 3;
		unsigned char on_protected_standby : 1;
		unsigned char in_page_error : 1;
		unsigned char system_charged_page : 1;
		unsigned char removal_requested : 1;
		unsigned char parity_error : 1;
	};

	static_assert( sizeof( mmpfn_entry3 ) == 0x1 );

	struct mmpfn
	{
		union
		{
			unsigned long long flink : 36;
			void* next;
		} u1;

		void* pte_address;
		unsigned long long original_pte;
		unsigned long long u2;

		union
		{
			struct
			{
				unsigned short reference_count;
				mmpfn_entry1 e1;
				mmpfn_entry3 e3;
			};
			unsigned long entire_field;
		} u3;

		unsigned short node_blink_low;

		unsigned char unused : 4;
		unsigned char unused2 : 4;

		union
		{
			unsigned char view_count;
			unsigned char node_flink_low;
			struct
			{
				unsigned char modified_list_bucket_index : 4;
				unsigned char anchor_large_page_size : 2;
			};
		};

		union
		{
			struct
			{
				unsigned long long pte_frame : 36;
				unsigned long long resident_page : 1;
				unsigned long long unused1 : 1;
				unsigned long long unused2 : 1;
				unsigned long long partition : 10;
				unsigned long long file_only : 1;
				unsigned long long pfn_exists : 1;
				unsigned long long spare : 9;
				unsigned long long page_identity : 3;
				unsigned long long prototype_pte : 1;
			};
			unsigned long long entire_field;
		} u4;
	};

	static_assert( sizeof( mmpfn ) == 0x30 );

} // namespace nt