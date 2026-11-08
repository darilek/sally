// CodeGear C++Builder
// Copyright (c) 1995, 2025 by Embarcadero Technologies, Inc.
// All rights reserved

// (DO NOT EDIT: machine generated header) 'GrayedCheckBox.pas' rev: 37.00 (Windows)

#ifndef GrayedCheckBoxHPP
#define GrayedCheckBoxHPP

#pragma delphiheader begin
#pragma option push
#if defined(__BORLANDC__) && !defined(__clang__)
#pragma option -w-      // All warnings off
#pragma option -Vx      // Zero-length empty class member 
#endif
#pragma pack(push,8)
#include <System.hpp>
#include <SysInit.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Controls.hpp>
#include <System.Classes.hpp>

//-- user supplied -----------------------------------------------------------

namespace Grayedcheckbox
{
//-- forward type declarations -----------------------------------------------
class DELPHICLASS TGrayedCheckBox;
//-- type declarations -------------------------------------------------------
class PASCALIMPLEMENTATION TGrayedCheckBox : public Vcl::Stdctrls::TCheckBox
{
	typedef Vcl::Stdctrls::TCheckBox inherited;
	
protected:
	virtual void __fastcall Toggle();
public:
	/* TCustomCheckBox.Create */ inline __fastcall virtual TGrayedCheckBox(System::Classes::TComponent* AOwner) : Vcl::Stdctrls::TCheckBox(AOwner) { }
	
public:
	/* TWinControl.CreateParented */ inline __fastcall TGrayedCheckBox(HWND ParentWindow) : Vcl::Stdctrls::TCheckBox(ParentWindow) { }
	/* TWinControl.Destroy */ inline __fastcall virtual ~TGrayedCheckBox() { }
	
};


//-- var, const, procedure ---------------------------------------------------
extern DELPHI_PACKAGE void __fastcall Register();
}	/* namespace Grayedcheckbox */
#if !defined(DELPHIHEADER_NO_IMPLICIT_NAMESPACE_USE) && !defined(NO_USING_NAMESPACE_GRAYEDCHECKBOX)
using namespace Grayedcheckbox;
#endif
#pragma pack(pop)
#pragma option pop

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// GrayedCheckBoxHPP
