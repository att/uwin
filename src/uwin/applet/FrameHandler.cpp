#include <windows.h>

extern "C" EXCEPTION_DISPOSITION  __CxxFrameHandler(
     struct _EXCEPTION_RECORD *ExceptionRecord,
     void * EstablisherFrame,
     struct _CONTEXT *ContextRecord,
     void * DispatcherContext
     );

extern "C" EXCEPTION_DISPOSITION  __CxxFrameHandler3(
     struct _EXCEPTION_RECORD *ExceptionRecord,
     void * EstablisherFrame,
     struct _CONTEXT *ContextRecord,
     void * DispatcherContext
			 )
{
	return __CxxFrameHandler(ExceptionRecord, EstablisherFrame, ContextRecord, DispatcherContext);
}
