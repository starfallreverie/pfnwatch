// shared/ioctl.hpp
#pragma once

#ifdef _KERNEL_MODE
#include <ntdef.h>
#else
#include <winioctl.h>
#include <cstdint>
#endif

namespace shared::ioctl {

	enum class command : unsigned int
	{
		assign_process = CTL_CODE( 0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS ),
		read_virtual_memory = CTL_CODE( 0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS ),
		get_detection_count = CTL_CODE( 0x8000, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS ),
		read_report = CTL_CODE( 0x8000, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS ),
	};

	struct request
	{
		union
		{
			struct
			{
				unsigned long process_id;
			} assign_process;

			struct
			{
				unsigned long process_id;
				unsigned long long address;
				unsigned long size;
			} read_virtual_memory;
		};
	};

	struct detection
	{
		unsigned long long pfn;
		unsigned long long virtual_address;
		unsigned long long pte_value;
		unsigned long long target_address;
		unsigned short reference_count;
		unsigned char cache_type;
		unsigned long tick_count;
	};

} // namespace shared::ioctl