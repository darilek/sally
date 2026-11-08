// CodeGear C++Builder
// Copyright (c) 1995, 2025 by Embarcadero Technologies, Inc.
// All rights reserved

// (DO NOT EDIT: machine generated header) 'PasTools.pas' rev: 37.00 (Windows)

#ifndef PasToolsHPP
#define PasToolsHPP

#pragma delphiheader begin
#pragma option push
#if defined(__BORLANDC__) && !defined(__clang__)
#pragma option -w-      // All warnings off
#pragma option -Vx      // Zero-length empty class member 
#endif
#pragma pack(push,8)
#include <System.hpp>
#include <SysInit.hpp>
#include <Winapi.Windows.hpp>
#include <System.Types.hpp>
#include <System.Classes.hpp>
#include <Vcl.ComCtrls.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.Controls.hpp>

//-- user supplied -----------------------------------------------------------

namespace Pastools
{
//-- forward type declarations -----------------------------------------------
class DELPHICLASS TCustomControlScrollOnDragOver;
class DELPHICLASS TTreeViewScrollOnDragOver;
class DELPHICLASS TListViewScrollOnDragOver;
class DELPHICLASS TListBoxScrollOnDragOver;
//-- type declarations -------------------------------------------------------
typedef void __fastcall (__closure *TControlScrollBeforeUpdate)(System::TObject* ObjectToValidate);

typedef void __fastcall (__closure *TControlScrollAfterUpdate)();

class PASCALIMPLEMENTATION TCustomControlScrollOnDragOver : public System::TObject
{
	typedef System::TObject inherited;
	
private:
	TControlScrollBeforeUpdate FOnBeforeUpdate;
	TControlScrollAfterUpdate FOnAfterUpdate;
	Vcl::Extctrls::TTimer* FDragOverTimer;
	Vcl::Controls::TControl* FControl;
	_FILETIME FDragOverTime;
	_FILETIME FLastVScrollTime;
	int FVScrollCount;
	void __fastcall DragOverTimer(System::TObject* Sender);
	void __fastcall BeforeUpdate(System::TObject* ObjectToValidate);
	void __fastcall AfterUpdate();
	
public:
	__fastcall TCustomControlScrollOnDragOver(Vcl::Controls::TControl* Control, bool ScheduleDragOver);
	__fastcall virtual ~TCustomControlScrollOnDragOver();
	virtual void __fastcall StartDrag();
	virtual void __fastcall EndDrag();
	virtual void __fastcall DragOver(const System::Types::TPoint &Point) = 0 ;
	__property TControlScrollBeforeUpdate OnBeforeUpdate = {read=FOnBeforeUpdate, write=FOnBeforeUpdate};
	__property TControlScrollAfterUpdate OnAfterUpdate = {read=FOnAfterUpdate, write=FOnAfterUpdate};
};


class PASCALIMPLEMENTATION TTreeViewScrollOnDragOver : public TCustomControlScrollOnDragOver
{
	typedef TCustomControlScrollOnDragOver inherited;
	
private:
	Vcl::Comctrls::TTreeNode* FLastDragNode;
	_FILETIME FLastHScrollTime;
	
public:
	virtual void __fastcall StartDrag();
	virtual void __fastcall DragOver(const System::Types::TPoint &Point);
public:
	/* TCustomControlScrollOnDragOver.Create */ inline __fastcall TTreeViewScrollOnDragOver(Vcl::Controls::TControl* Control, bool ScheduleDragOver) : TCustomControlScrollOnDragOver(Control, ScheduleDragOver) { }
	/* TCustomControlScrollOnDragOver.Destroy */ inline __fastcall virtual ~TTreeViewScrollOnDragOver() { }
	
};


class PASCALIMPLEMENTATION TListViewScrollOnDragOver : public TCustomControlScrollOnDragOver
{
	typedef TCustomControlScrollOnDragOver inherited;
	
public:
	virtual void __fastcall DragOver(const System::Types::TPoint &Point);
public:
	/* TCustomControlScrollOnDragOver.Create */ inline __fastcall TListViewScrollOnDragOver(Vcl::Controls::TControl* Control, bool ScheduleDragOver) : TCustomControlScrollOnDragOver(Control, ScheduleDragOver) { }
	/* TCustomControlScrollOnDragOver.Destroy */ inline __fastcall virtual ~TListViewScrollOnDragOver() { }
	
};


class PASCALIMPLEMENTATION TListBoxScrollOnDragOver : public TCustomControlScrollOnDragOver
{
	typedef TCustomControlScrollOnDragOver inherited;
	
public:
	virtual void __fastcall DragOver(const System::Types::TPoint &Point);
public:
	/* TCustomControlScrollOnDragOver.Create */ inline __fastcall TListBoxScrollOnDragOver(Vcl::Controls::TControl* Control, bool ScheduleDragOver) : TCustomControlScrollOnDragOver(Control, ScheduleDragOver) { }
	/* TCustomControlScrollOnDragOver.Destroy */ inline __fastcall virtual ~TListBoxScrollOnDragOver() { }
	
};


//-- var, const, procedure ---------------------------------------------------
extern DELPHI_PACKAGE System::Classes::TComponent* __fastcall Construct(System::Classes::TComponentClass ComponentClass, System::Classes::TComponent* Owner);
extern DELPHI_PACKAGE bool __fastcall IsVista();
extern DELPHI_PACKAGE bool __fastcall IsExactly2008R2();
}	/* namespace Pastools */
#if !defined(DELPHIHEADER_NO_IMPLICIT_NAMESPACE_USE) && !defined(NO_USING_NAMESPACE_PASTOOLS)
using namespace Pastools;
#endif
#pragma pack(pop)
#pragma option pop

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// PasToolsHPP
