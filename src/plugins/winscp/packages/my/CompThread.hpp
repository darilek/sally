// CodeGear C++Builder
// Copyright (c) 1995, 2025 by Embarcadero Technologies, Inc.
// All rights reserved

// (DO NOT EDIT: machine generated header) 'CompThread.pas' rev: 37.00 (Windows)

#ifndef CompThreadHPP
#define CompThreadHPP

#pragma delphiheader begin
#pragma option push
#if defined(__BORLANDC__) && !defined(__clang__)
#pragma option -w-      // All warnings off
#pragma option -Vx      // Zero-length empty class member 
#endif
#pragma pack(push,8)
#include <System.hpp>
#include <SysInit.hpp>
#include <System.Classes.hpp>
#include <Winapi.Windows.hpp>

//-- user supplied -----------------------------------------------------------

namespace Compthread
{
//-- forward type declarations -----------------------------------------------
class DELPHICLASS TCompThread;
//-- type declarations -------------------------------------------------------
class PASCALIMPLEMENTATION TCompThread : public System::TObject
{
	typedef System::TObject inherited;
	
private:
	Winapi::Windows::THandle FHandle;
	System::TThreadID FThreadID;
	bool FTerminated;
	bool FSuspended;
	bool FFreeOnTerminate;
	bool FFinished;
	int FReturnValue;
	System::Classes::TNotifyEvent FOnTerminate;
	System::Classes::TThreadMethod FMethod;
	System::TObject* FSynchronizeException;
	void __fastcall CallOnTerminate();
	System::Classes::TThreadPriority __fastcall GetPriority();
	void __fastcall SetPriority(System::Classes::TThreadPriority Value);
	void __fastcall SetSuspended(bool Value);
	
protected:
	virtual void __fastcall DoTerminate();
	virtual void __fastcall Execute() = 0 ;
	void __fastcall Synchronize(System::Classes::TThreadMethod Method);
	__property int ReturnValue = {read=FReturnValue, write=FReturnValue, nodefault};
	__property bool Terminated = {read=FTerminated, nodefault};
	
public:
	__fastcall TCompThread(bool CreateSuspended);
	__fastcall virtual ~TCompThread();
	void __fastcall Resume();
	void __fastcall Suspend();
	virtual void __fastcall Terminate();
	bool __fastcall WaitFor(unsigned Milliseconds = (unsigned)(0xffffffff));
	__property bool FreeOnTerminate = {read=FFreeOnTerminate, write=FFreeOnTerminate, nodefault};
	__property Winapi::Windows::THandle Handle = {read=FHandle};
	__property System::Classes::TThreadPriority Priority = {read=GetPriority, write=SetPriority, nodefault};
	__property bool Suspended = {read=FSuspended, write=SetSuspended, nodefault};
	__property System::TThreadID ThreadID = {read=FThreadID, nodefault};
	__property System::Classes::TNotifyEvent OnTerminate = {read=FOnTerminate, write=FOnTerminate};
};


//-- var, const, procedure ---------------------------------------------------
}	/* namespace Compthread */
#if !defined(DELPHIHEADER_NO_IMPLICIT_NAMESPACE_USE) && !defined(NO_USING_NAMESPACE_COMPTHREAD)
using namespace Compthread;
#endif
#pragma pack(pop)
#pragma option pop

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// CompThreadHPP
