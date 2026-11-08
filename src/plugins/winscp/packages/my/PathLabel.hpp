// CodeGear C++Builder
// Copyright (c) 1995, 2025 by Embarcadero Technologies, Inc.
// All rights reserved

// (DO NOT EDIT: machine generated header) 'PathLabel.pas' rev: 37.00 (Windows)

#ifndef PathLabelHPP
#define PathLabelHPP

#pragma delphiheader begin
#pragma option push
#if defined(__BORLANDC__) && !defined(__clang__)
#pragma option -w-      // All warnings off
#pragma option -Vx      // Zero-length empty class member 
#endif
#pragma pack(push,8)
#include <System.hpp>
#include <SysInit.hpp>
#include <Winapi.Messages.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Controls.hpp>
#include <System.Classes.hpp>
#include <Vcl.Forms.hpp>
#include <Winapi.Windows.hpp>
#include <Vcl.Graphics.hpp>
#include <System.UITypes.hpp>
#include <Vcl.Menus.hpp>

//-- user supplied -----------------------------------------------------------

namespace Pathlabel
{
//-- forward type declarations -----------------------------------------------
class DELPHICLASS TCustomPathLabel;
class DELPHICLASS TPathLabel;
//-- type declarations -------------------------------------------------------
typedef void __fastcall (__closure *TPathLabelGetStatusEvent)(TCustomPathLabel* Sender, bool &Active);

typedef void __fastcall (__closure *TPathLabelPathClickEvent)(TCustomPathLabel* Sender, System::UnicodeString Path);

class PASCALIMPLEMENTATION TCustomPathLabel : public Vcl::Stdctrls::TCustomLabel
{
	typedef Vcl::Stdctrls::TCustomLabel inherited;
	
private:
	System::StaticArray<System::Uitypes::TColor, 6> FColors;
	int FIndentHorizontal;
	int FIndentVertical;
	bool FUnixPath;
	TPathLabelGetStatusEvent FOnGetStatus;
	TPathLabelPathClickEvent FOnPathClick;
	System::UnicodeString FDisplayPath;
	System::UnicodeString FDisplayHotTrack;
	System::UnicodeString FDisplayMask;
	bool FHotTrack;
	bool FMouseInView;
	bool FIsActive;
	System::UnicodeString FMask;
	bool FAutoSizeVertical;
	HIDESBASE MESSAGE void __fastcall CMHintShow(Winapi::Messages::TMessage &Message);
	HIDESBASE MESSAGE void __fastcall CMMouseEnter(Winapi::Messages::TMessage &Message);
	HIDESBASE MESSAGE void __fastcall CMMouseLeave(Winapi::Messages::TMessage &Message);
	System::Uitypes::TColor __fastcall GetColors(int Index);
	void __fastcall SetColors(int Index, System::Uitypes::TColor Value);
	void __fastcall SetIndentHorizontal(int AIndent);
	void __fastcall SetIndentVertical(int AIndent);
	void __fastcall SetUnixPath(bool AUnixPath);
	void __fastcall SetMask(System::UnicodeString Value);
	void __fastcall SetAutoSizeVertical(bool Value);
	
protected:
	DYNAMIC void __fastcall AdjustBounds();
	DYNAMIC void __fastcall Click();
	DYNAMIC void __fastcall DoDrawText(Winapi::Windows::TRect &Rect, System::LongInt Flags);
	virtual void __fastcall Notification(System::Classes::TComponent* AComponent, System::Classes::TOperation Operation);
	virtual void __fastcall Paint();
	bool __fastcall IsActive();
	System::UnicodeString __fastcall HotTrackPath(System::UnicodeString Path);
	DYNAMIC void __fastcall MouseMove(System::Classes::TShiftState Shift, int X, int Y);
	virtual void __fastcall DoPathClick(System::UnicodeString Path);
	
public:
	__fastcall virtual TCustomPathLabel(System::Classes::TComponent* AnOwner);
	void __fastcall UpdateStatus();
	__property System::Uitypes::TColor ActiveColor = {read=GetColors, write=SetColors, index=1, default=-16777214};
	__property System::Uitypes::TColor ActiveTextColor = {read=GetColors, write=SetColors, index=3, default=-16777207};
	__property System::Uitypes::TColor ActiveHotTrackColor = {read=GetColors, write=SetColors, index=5, default=-16777189};
	__property bool UnixPath = {read=FUnixPath, write=SetUnixPath, default=0};
	__property int IndentHorizontal = {read=FIndentHorizontal, write=SetIndentHorizontal, default=5};
	__property int IndentVertical = {read=FIndentVertical, write=SetIndentVertical, default=1};
	__property System::Uitypes::TColor InactiveColor = {read=GetColors, write=SetColors, index=0, default=-16777213};
	__property System::Uitypes::TColor InactiveTextColor = {read=GetColors, write=SetColors, index=2, default=-16777197};
	__property System::Uitypes::TColor InactiveHotTrackColor = {read=GetColors, write=SetColors, index=4, default=-16777188};
	__property TPathLabelGetStatusEvent OnGetStatus = {read=FOnGetStatus, write=FOnGetStatus};
	__property TPathLabelPathClickEvent OnPathClick = {read=FOnPathClick, write=FOnPathClick};
	__property bool HotTrack = {read=FHotTrack, write=FHotTrack, default=0};
	__property System::UnicodeString Mask = {read=FMask, write=SetMask};
	__property bool AutoSizeVertical = {read=FAutoSizeVertical, write=SetAutoSizeVertical, nodefault};
	__property FocusControl;
	__property Caption = {default=0};
	__property Hint = {stored=false, default=0};
	__property Align = {default=1};
public:
	/* TGraphicControl.Destroy */ inline __fastcall virtual ~TCustomPathLabel() { }
	
};


class PASCALIMPLEMENTATION TPathLabel : public TCustomPathLabel
{
	typedef TCustomPathLabel inherited;
	
__published:
	__property ActiveColor = {index=1, default=-16777214};
	__property ActiveTextColor = {index=3, default=-16777207};
	__property ActiveHotTrackColor = {index=5, default=-16777189};
	__property UnixPath = {default=0};
	__property IndentHorizontal = {default=5};
	__property IndentVertical = {default=1};
	__property InactiveColor = {index=0, default=-16777213};
	__property InactiveTextColor = {index=2, default=-16777197};
	__property InactiveHotTrackColor = {index=4, default=-16777188};
	__property AutoSizeVertical;
	__property HotTrack = {default=0};
	__property OnGetStatus;
	__property OnPathClick;
	__property Align = {default=1};
	__property Alignment = {default=0};
	__property Anchors = {default=3};
	__property AutoSize = {default=1};
	__property BiDiMode;
	__property Constraints;
	__property DragCursor = {default=-12};
	__property DragKind = {default=0};
	__property DragMode = {default=0};
	__property Enabled = {default=1};
	__property Font;
	__property ParentBiDiMode = {default=1};
	__property ParentFont = {default=1};
	__property PopupMenu;
	__property Transparent;
	__property Visible = {default=1};
	__property OnClick;
	__property OnDblClick;
	__property OnDragDrop;
	__property OnDragOver;
	__property OnEndDock;
	__property OnEndDrag;
	__property OnMouseDown;
	__property OnMouseMove;
	__property OnMouseUp;
	__property OnStartDock;
	__property OnStartDrag;
public:
	/* TCustomPathLabel.Create */ inline __fastcall virtual TPathLabel(System::Classes::TComponent* AnOwner) : TCustomPathLabel(AnOwner) { }
	
public:
	/* TGraphicControl.Destroy */ inline __fastcall virtual ~TPathLabel() { }
	
};


//-- var, const, procedure ---------------------------------------------------
extern DELPHI_PACKAGE void __fastcall Register();
}	/* namespace Pathlabel */
#if !defined(DELPHIHEADER_NO_IMPLICIT_NAMESPACE_USE) && !defined(NO_USING_NAMESPACE_PATHLABEL)
using namespace Pathlabel;
#endif
#pragma pack(pop)
#pragma option pop

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// PathLabelHPP
