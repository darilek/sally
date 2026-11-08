// CodeGear C++Builder
// Copyright (c) 1995, 2025 by Embarcadero Technologies, Inc.
// All rights reserved

// (DO NOT EDIT: machine generated header) 'UnixPathComboBox.pas' rev: 37.00 (Windows)

#ifndef UnixPathComboBoxHPP
#define UnixPathComboBoxHPP

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
#include <Winapi.Messages.hpp>
#include <System.SysUtils.hpp>
#include <System.Classes.hpp>
#include <Vcl.Graphics.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.Dialogs.hpp>
#include <Vcl.StdCtrls.hpp>
#include <System.UITypes.hpp>
#include <Vcl.Menus.hpp>

//-- user supplied -----------------------------------------------------------

namespace Unixpathcombobox
{
//-- forward type declarations -----------------------------------------------
class DELPHICLASS TUnixPathComboBox;
//-- type declarations -------------------------------------------------------
class PASCALIMPLEMENTATION TUnixPathComboBox : public Vcl::Stdctrls::TCustomComboBox
{
	typedef Vcl::Stdctrls::TCustomComboBox inherited;
	
private:
	System::AnsiString FUnixPath;
	System::AnsiString FRootName;
	Vcl::Controls::TImageList* FImageList;
	bool FNotifyChange;
	bool F_NotifyChange;
	void __fastcall SetUnixPath(System::AnsiString AUnixPath);
	void __fastcall SetRootName(System::AnsiString ARootName);
	void __fastcall UpdateItems();
	HIDESBASE MESSAGE void __fastcall CNDrawItem(Winapi::Messages::TWMDrawItem &Message);
	HIDESBASE MESSAGE void __fastcall CMFontChanged(Winapi::Messages::TMessage &Message);
	HIDESBASE MESSAGE void __fastcall WMCHAR(Winapi::Messages::TMessage &Message);
	
protected:
	virtual void __fastcall CreateWnd();
	DYNAMIC void __fastcall Change();
	DYNAMIC void __fastcall Click();
	
public:
	__fastcall virtual TUnixPathComboBox(System::Classes::TComponent* AOwner);
	__fastcall virtual ~TUnixPathComboBox();
	__property bool NotifyChange = {read=F_NotifyChange, write=F_NotifyChange, nodefault};
	
__published:
	__property Align = {default=0};
	__property Color = {default=-16777211};
	__property Ctl3D;
	__property DragMode = {default=0};
	__property DragCursor = {default=-12};
	__property DropDownCount = {default=8};
	__property Anchors = {default=3};
	__property Enabled = {default=1};
	__property Font;
	__property ParentColor = {default=0};
	__property ParentCtl3D = {default=1};
	__property ParentFont = {default=1};
	__property ParentShowHint = {default=1};
	__property PopupMenu;
	__property ShowHint;
	__property TabOrder = {default=-1};
	__property TabStop = {default=1};
	__property Visible = {default=1};
	__property OnClick;
	__property OnChange;
	__property OnDblClick;
	__property OnDragDrop;
	__property OnDragOver;
	__property OnDropDown;
	__property OnEndDrag;
	__property OnEnter;
	__property OnExit;
	__property OnKeyDown;
	__property OnKeyPress;
	__property OnKeyUp;
	__property OnStartDrag;
	__property System::AnsiString UnixPath = {read=FUnixPath, write=SetUnixPath};
	__property System::AnsiString RootName = {read=FRootName, write=SetRootName};
public:
	/* TWinControl.CreateParented */ inline __fastcall TUnixPathComboBox(HWND ParentWindow) : Vcl::Stdctrls::TCustomComboBox(ParentWindow) { }
	
};


//-- var, const, procedure ---------------------------------------------------
extern DELPHI_PACKAGE void __fastcall Register();
}	/* namespace Unixpathcombobox */
#if !defined(DELPHIHEADER_NO_IMPLICIT_NAMESPACE_USE) && !defined(NO_USING_NAMESPACE_UNIXPATHCOMBOBOX)
using namespace Unixpathcombobox;
#endif
#pragma pack(pop)
#pragma option pop

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// UnixPathComboBoxHPP
