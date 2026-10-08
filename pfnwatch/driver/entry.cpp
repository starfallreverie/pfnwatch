// entry.cpp
#include "project/global.hpp"

namespace handlers {

	static NTSTATUS device_add( WDFDRIVER driver, PWDFDEVICE_INIT device_init )
	{
		return g.m_dispatcher.device_add( driver, device_init );
	}

	static void driver_unload( WDFDRIVER driver )
	{
		g.m_dispatcher.driver_unload( driver );
	}

} // namespace entry

extern "C" NTSTATUS DriverEntry( PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path )
{
	WDF_DRIVER_CONFIG config{};
	WDF_DRIVER_CONFIG_INIT( &config, handlers::device_add );
	config.EvtDriverUnload = handlers::driver_unload;

	return ::WdfDriverCreate( driver_object, registry_path, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE );
}