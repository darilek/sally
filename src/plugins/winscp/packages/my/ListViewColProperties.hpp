// CodeGear C++Builder
// Copyright (c) 1995, 2025 by Embarcadero Technologies, Inc.
// All rights reserved

// (DO NOT EDIT: machine generated header) 'ListViewColProperties.pas' rev: 37.00 (Windows)

#ifndef ListViewColPropertiesHPP
#define ListViewColPropertiesHPP

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
#include <Vcl.ComCtrls.hpp>

//-- user supplied -----------------------------------------------------------

namespace Listviewcolproperties
{
//-- forward type declarations -----------------------------------------------
class DELPHICLASS TCustomListViewColProperties;
class DELPHICLASS TListViewColProperties;
//-- type declarations -------------------------------------------------------
class PASCALIMPLEMENTATION TCustomListViewColProperties : public System::Classes::TPersistent
{
	typedef System::Classes::TPersistent inherited;
	
	
private:
	typedef System::DynamicArray<int> _TCustomListViewColProperties__1;
	
	typedef System::DynamicArray<bool> _TCustomListViewColProperties__2;
	
	
private:
	bool FChanged;
	int FMaxWidth;
	int FMinWidth;
	System::Classes::TNotifyEvent FOnChange;
	int FUpdating;
	_TCustomListViewColProperties__1 FWidths;
	_TCustomListViewColProperties__2 FVisible;
	Vcl::Comctrls::TListColumns* __fastcall GetColumns();
	int __fastcall GetCount();
	System::UnicodeString __fastcall GetOrderStr();
	void __fastcall CheckBounds(int Index);
	void __fastcall SetMaxWidth(int Value);
	void __fastcall SetMinWidth(int Value);
	void __fastcall SetWidthsStr(System::UnicodeString Value);
	System::UnicodeString __fastcall GetWidthsStr();
	void __fastcall SetOrderStr(System::UnicodeString Value);
	
protected:
	Vcl::Comctrls::TCustomListView* FListView;
	bool FListViewManaged;
	System::Classes::TAlignment __fastcall GetAlignments(int Index);
	int __fastcall GetOrder(int Index);
	virtual System::UnicodeString __fastcall GetParamsStr();
	bool __fastcall GetVisible(int Index);
	int __fastcall GetWidths(int Index);
	void __fastcall SetAlignments(int Index, System::Classes::TAlignment Value);
	void __fastcall SetVisible(int Index, bool Value);
	void __fastcall SetWidths(int Index, int Value);
	void __fastcall SetOrder(int Index, int Value);
	System::UnicodeString __fastcall GetCaptions(int Index);
	virtual void __fastcall Changed();
	virtual void __fastcall SetCaptions(int Index, System::UnicodeString Value);
	virtual void __fastcall SetParamsStr(System::UnicodeString Value);
	void __fastcall UpdateFromListView();
	__property Vcl::Comctrls::TListColumns* Columns = {read=GetColumns, stored=false};
	__property int Count = {read=GetCount, stored=false, nodefault};
	
public:
	__fastcall TCustomListViewColProperties(Vcl::Comctrls::TCustomListView* ListView, int ColCount);
	void __fastcall EndUpdate();
	void __fastcall BeginUpdate();
	void __fastcall ListViewWndCreated();
	__property System::Classes::TAlignment Alignments[int Index] = {read=GetAlignments, write=SetAlignments};
	__property System::UnicodeString Captions[int Index] = {read=GetCaptions, write=SetCaptions};
	__property int MaxWidth = {read=FMaxWidth, write=SetMaxWidth, default=1000};
	__property int MinWidth = {read=FMinWidth, write=SetMinWidth, default=20};
	__property int Widths[int Index] = {read=GetWidths, write=SetWidths};
	__property bool Visible[int Index] = {read=GetVisible, write=SetVisible};
	void __fastcall RecreateColumns();
	__property System::Classes::TNotifyEvent OnChange = {read=FOnChange, write=FOnChange};
	__property int Order[int Index] = {read=GetOrder, write=SetOrder};
	__property System::UnicodeString OrderStr = {read=GetOrderStr, write=SetOrderStr, stored=false};
	__property System::UnicodeString ParamsStr = {read=GetParamsStr, write=SetParamsStr, stored=false};
	__property System::UnicodeString WidthsStr = {read=GetWidthsStr, write=SetWidthsStr, stored=false};
public:
	/* TPersistent.Destroy */ inline __fastcall virtual ~TCustomListViewColProperties() { }
	
};


class PASCALIMPLEMENTATION TListViewColProperties : public TCustomListViewColProperties
{
	typedef TCustomListViewColProperties inherited;
	
__published:
	__property MaxWidth = {default=1000};
	__property MinWidth = {default=20};
public:
	/* TCustomListViewColProperties.Create */ inline __fastcall TListViewColProperties(Vcl::Comctrls::TCustomListView* ListView, int ColCount) : TCustomListViewColProperties(ListView, ColCount) { }
	
public:
	/* TPersistent.Destroy */ inline __fastcall virtual ~TListViewColProperties() { }
	
};


//-- var, const, procedure ---------------------------------------------------
static _DELPHI_CONST System::Word DefaultListViewMaxWidth = System::Word(0x3e8);
static _DELPHI_CONST System::Int8 DefaultListViewMinWidth = System::Int8(0x14);
extern DELPHI_PACKAGE System::UnicodeString __fastcall CutToChar(System::UnicodeString &Str, System::WideChar Ch, bool Trim);
}	/* namespace Listviewcolproperties */
#if !defined(DELPHIHEADER_NO_IMPLICIT_NAMESPACE_USE) && !defined(NO_USING_NAMESPACE_LISTVIEWCOLPROPERTIES)
using namespace Listviewcolproperties;
#endif
#pragma pack(pop)
#pragma option pop

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// ListViewColPropertiesHPP
