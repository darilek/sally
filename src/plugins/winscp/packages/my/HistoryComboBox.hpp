// CodeGear C++Builder
// Copyright (c) 1995, 2025 by Embarcadero Technologies, Inc.
// All rights reserved

// (DO NOT EDIT: machine generated header) 'HistoryComboBox.pas' rev: 37.00 (Windows)

#ifndef HistoryComboBoxHPP
#define HistoryComboBoxHPP

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

//-- user supplied -----------------------------------------------------------

namespace Historycombobox
{
//-- forward type declarations -----------------------------------------------
class DELPHICLASS THistoryComboBox;
//-- type declarations -------------------------------------------------------
enum DECLSPEC_DENUM Historycombobox__1 : unsigned char { soExit, soDropDown };

typedef System::Set<Historycombobox__1, Historycombobox__1::soExit, Historycombobox__1::soDropDown> THistorySaveOn;

typedef void __fastcall (__closure *THistoryComboBoxGetData)(THistoryComboBox* Sender, void * &Data);

typedef void __fastcall (__closure *THistoryComboBoxSetData)(THistoryComboBox* Sender, void * Data);

class PASCALIMPLEMENTATION THistoryComboBox : public Vcl::Stdctrls::TComboBox
{
	typedef Vcl::Stdctrls::TComboBox inherited;
	
private:
	THistorySaveOn FSaveOn;
	int FMaxHistorySize;
	THistoryComboBoxGetData FOnGetData;
	THistoryComboBoxSetData FOnSetData;
	void __fastcall SetMaxHistorySize(int AMaxHistorySize);
	bool __fastcall StoreSaveOn();
	int __fastcall GetMaxItemWidth();
	
protected:
	DYNAMIC void __fastcall DoExit();
	DYNAMIC void __fastcall DropDown();
	DYNAMIC void __fastcall KeyDown(System::Word &Key, System::Classes::TShiftState Shift);
	DYNAMIC void __fastcall Change();
	
public:
	__fastcall virtual THistoryComboBox(System::Classes::TComponent* AOwner);
	virtual void __fastcall SaveToHistory();
	
__published:
	__property THistorySaveOn SaveOn = {read=FSaveOn, write=FSaveOn, stored=StoreSaveOn, nodefault};
	__property int MaxHistorySize = {read=FMaxHistorySize, write=SetMaxHistorySize, default=30};
	__property THistoryComboBoxGetData OnGetData = {read=FOnGetData, write=FOnGetData};
	__property THistoryComboBoxSetData OnSetData = {read=FOnSetData, write=FOnSetData};
public:
	/* TCustomComboBox.Destroy */ inline __fastcall virtual ~THistoryComboBox() { }
	
public:
	/* TWinControl.CreateParented */ inline __fastcall THistoryComboBox(HWND ParentWindow) : Vcl::Stdctrls::TComboBox(ParentWindow) { }
	
};


//-- var, const, procedure ---------------------------------------------------
#define DefaultHistorySaveOn (System::Set<Historycombobox__1, Historycombobox__1::soExit, Historycombobox__1::soDropDown>() << Historycombobox__1::soExit << Historycombobox__1::soDropDown )
static _DELPHI_CONST System::Int8 DefaultMaxHistorySize = System::Int8(0x1e);
extern DELPHI_PACKAGE void __fastcall Register();
extern DELPHI_PACKAGE void __fastcall SaveToHistory(System::Classes::TStrings* Strings, System::UnicodeString T, void * Data = (void *)(0x0), int MaxHistorySize = 0x1e);
}	/* namespace Historycombobox */
#if !defined(DELPHIHEADER_NO_IMPLICIT_NAMESPACE_USE) && !defined(NO_USING_NAMESPACE_HISTORYCOMBOBOX)
using namespace Historycombobox;
#endif
#pragma pack(pop)
#pragma option pop

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// HistoryComboBoxHPP
