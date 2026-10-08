// project/core/reader/reader.hpp
#pragma once

namespace core {

	class reader
	{
	public:
		void start( );

	private:
		static unsigned long __stdcall routine( );
	};

} // namespace core