// CodeGear C++Builder
// Copyright (c) 1995, 2025 by Embarcadero Technologies, Inc.
// All rights reserved

// (DO NOT EDIT: machine generated header) 'IEListView.pas' rev: 37.00 (Windows)

#ifndef IEListViewHPP
#define IEListViewHPP

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
#include <Winapi.ActiveX.hpp>
#include <Winapi.CommCtrl.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.ImgList.hpp>
#include <Vcl.ComCtrls.hpp>
#include <NortonLikeListView.hpp>
#include <ListViewColProperties.hpp>
#include <System.UITypes.hpp>
#include <Vcl.Menus.hpp>

//-- user supplied -----------------------------------------------------------

namespace Ielistview
{
//-- forward type declarations -----------------------------------------------
class DELPHICLASS TIEListViewColProperties;
class DELPHICLASS TCustomIEListView;
class DELPHICLASS TIEListView;
//-- type declarations -------------------------------------------------------
class PASCALIMPLEMENTATION TIEListViewColProperties : public Listviewcolproperties::TCustomListViewColProperties
{
	typedef Listviewcolproperties::TCustomListViewColProperties inherited;
	
protected:
	bool __fastcall GetSortAscending();
	void __fastcall SetSortColumn(int Value);
	int __fastcall GetSortColumn();
	virtual System::UnicodeString __fastcall GetSortStr();
	void __fastcall SetSortAscending(bool Value);
	virtual void __fastcall SetSortStr(System::UnicodeString Value);
	virtual System::UnicodeString __fastcall GetParamsStr();
	virtual void __fastcall SetParamsStr(System::UnicodeString Value);
	
public:
	__fastcall TIEListViewColProperties(Vcl::Comctrls::TCustomListView* ListView, int ColCount);
	__property bool SortAscending = {read=GetSortAscending, write=SetSortAscending, default=1};
	__property int SortColumn = {read=GetSortColumn, write=SetSortColumn, nodefault};
	__property System::UnicodeString SortStr = {read=GetSortStr, write=SetSortStr, stored=false};
public:
	/* TPersistent.Destroy */ inline __fastcall virtual ~TIEListViewColProperties() { }
	
};


enum DECLSPEC_DENUM TDateTimeDisplay : unsigned char { dtdDateTimeSec, dtdDateTime, dtdDate };

typedef void __fastcall (__closure *TListViewSecondaryColumnHeaderEvent)(TCustomIEListView* Sender, int Index, int &SecondaryColumn);

class PASCALIMPLEMENTATION TCustomIEListView : public Nortonlikelistview::TCustomNortonLikeListView
{
	typedef Nortonlikelistview::TCustomNortonLikeListView inherited;
	
private:
	int FSortColumn;
	bool FSortAscending;
	bool FColumnIconPainted;
	bool FShowColumnIcon;
	HWND FHeaderHandle;
	Vcl::Forms::TCustomForm* FParentForm;
	Vcl::Graphics::TCanvas* FHeaderCanvas;
	System::Classes::TNotifyEvent FOnHeaderEndDrag;
	System::Classes::TNotifyEvent FOnHeaderEndTrack;
	TListViewSecondaryColumnHeaderEvent FOnSecondaryColumnHeader;
	System::UnicodeString FDateTimeFormatStr;
	System::UnicodeString FDateFormatStr;
	TDateTimeDisplay FDateTimeDisplay;
	Vcl::Controls::TDragImageList* FDragImageList;
	Vcl::Controls::TImageList* FHeaderImages;
	int __fastcall SecondaryColumnHeaderOffset(Vcl::Graphics::TCanvas* Canvas, int Index);
	int __fastcall ColumnHeaderIconWidth();
	
protected:
	virtual void __fastcall ColPropertiesChange(System::TObject* Sender);
	virtual void __fastcall SetShowColumnIcon(bool Value);
	virtual void __fastcall SetSortColumn(int Value);
	virtual void __fastcall SetSortAscending(bool Value);
	virtual void __fastcall SortItems();
	virtual void __fastcall SetViewStyle(Vcl::Comctrls::TViewStyle Value);
	virtual void __fastcall SetDateTimeDisplay(TDateTimeDisplay Value);
	virtual void __fastcall SetDateTimeFormatString();
	virtual void __fastcall HeaderEndDrag(System::TObject* Sender);
	virtual void __fastcall SetHeaderImages(Vcl::Controls::TImageList* Value);
	virtual int __fastcall SecondaryColumnHeader(int Index, bool &AliasOnly);
	virtual Listviewcolproperties::TCustomListViewColProperties* __fastcall NewColProperties();
	virtual bool __fastcall SortAscendingByDefault(int Index);
	virtual void __fastcall CreateWnd();
	DYNAMIC void __fastcall ColClick(Vcl::Comctrls::TListColumn* Column);
	virtual void __fastcall Loaded();
	HIDESBASE MESSAGE void __fastcall WMPaint(Winapi::Messages::TWMPaint &Msg);
	HIDESBASE MESSAGE void __fastcall WMNotify(Winapi::Messages::TWMNotify &Msg);
	
public:
	__fastcall virtual TCustomIEListView(System::Classes::TComponent* AOwner);
	__fastcall virtual ~TCustomIEListView();
	virtual void __fastcall SetColumnImages();
	__property Vcl::Controls::TImageList* HeaderImages = {read=FHeaderImages, write=SetHeaderImages};
	__property Vcl::Controls::TDragImageList* DragImageList = {read=FDragImageList};
	__property HWND HeaderHandle = {read=FHeaderHandle};
	__property Vcl::Forms::TCustomForm* ParentForm = {read=FParentForm};
	__property System::UnicodeString DateTimeFormatStr = {read=FDateTimeFormatStr, write=FDateTimeFormatStr, stored=false};
	__property System::UnicodeString DateFormatStr = {read=FDateFormatStr};
	__property int SortColumn = {read=FSortColumn, write=SetSortColumn, nodefault};
	__property bool ShowColumnIcon = {read=FShowColumnIcon, write=SetShowColumnIcon, default=1};
	__property bool SortAscending = {read=FSortAscending, write=SetSortAscending, default=1};
	__property TListViewSecondaryColumnHeaderEvent OnSecondaryColumnHeader = {read=FOnSecondaryColumnHeader, write=FOnSecondaryColumnHeader};
	
__published:
	__property TDateTimeDisplay DateTimeDisplay = {read=FDateTimeDisplay, write=SetDateTimeDisplay, default=0};
	__property System::Classes::TNotifyEvent OnHeaderEndDrag = {read=FOnHeaderEndDrag, write=FOnHeaderEndDrag};
	__property System::Classes::TNotifyEvent OnHeaderEndTrack = {read=FOnHeaderEndTrack, write=FOnHeaderEndTrack};
	__property Align = {default=0};
	__property AllocBy = {default=0};
	__property Anchors = {default=3};
	__property BiDiMode;
	__property BorderStyle = {default=1};
	__property BorderWidth = {default=0};
	__property Checkboxes = {default=0};
	__property Color = {default=-16777211};
	__property ColumnClick = {default=1};
	__property Constraints;
	__property Ctl3D;
	__property Enabled = {default=1};
	__property Font;
	__property FlatScrollBars = {default=0};
	__property FullDrag = {default=0};
	__property GridLines = {default=0};
	__property HideSelection = {default=1};
	__property HotTrack = {default=0};
	__property HotTrackStyles = {default=0};
	__property IconOptions;
	__property ReadOnly = {default=0};
	__property RowSelect = {default=0};
	__property ParentBiDiMode = {default=1};
	__property ParentColor = {default=0};
	__property ParentFont = {default=1};
	__property ParentShowHint = {default=1};
	__property PopupMenu;
	__property ShowColumnHeaders = {default=1};
	__property ShowHint;
	__property TabOrder = {default=-1};
	__property TabStop = {default=1};
	__property ViewStyle = {default=0};
	__property Visible = {default=1};
	__property OnChange;
	__property OnChanging;
	__property OnClick;
	__property OnColumnClick;
	__property OnColumnRightClick;
	__property OnCustomDraw;
	__property OwnerDraw = {default=0};
	__property OnCustomDrawItem;
	__property OnCustomDrawSubItem;
	__property OnDblClick;
	__property OnDeletion;
	__property OnDrawItem;
	__property OnEdited;
	__property OnEditing;
	__property OnEndDock;
	__property OnEnter;
	__property OnExit;
	__property OnInsert;
	__property OnKeyDown;
	__property OnKeyPress;
	__property OnKeyUp;
	__property OnMouseDown;
	__property OnMouseMove;
	__property OnMouseUp;
	__property OnResize;
	__property OnStartDock;
	__property NortonLike = {default=0};
	__property OnSelectByMask;
public:
	/* TWinControl.CreateParented */ inline __fastcall TCustomIEListView(HWND ParentWindow) : Nortonlikelistview::TCustomNortonLikeListView(ParentWindow) { }
	
};


class PASCALIMPLEMENTATION TIEListView : public TCustomIEListView
{
	typedef TCustomIEListView inherited;
	
__published:
	__property Action;
	__property Align = {default=0};
	__property AllocBy = {default=0};
	__property Anchors = {default=3};
	__property BevelEdges = {default=15};
	__property BevelInner = {index=0, default=2};
	__property BevelOuter = {index=1, default=1};
	__property BevelKind = {default=0};
	__property BevelWidth = {default=1};
	__property BiDiMode;
	__property BorderStyle = {default=1};
	__property BorderWidth = {default=0};
	__property Checkboxes = {default=0};
	__property Color = {default=-16777211};
	__property Columns;
	__property ColumnClick = {default=1};
	__property Constraints;
	__property Ctl3D;
	__property DragCursor = {default=-12};
	__property DragKind = {default=0};
	__property DragMode = {default=0};
	__property Enabled = {default=1};
	__property Font;
	__property FlatScrollBars = {default=0};
	__property FullDrag = {default=0};
	__property GridLines = {default=0};
	__property HeaderImages;
	__property HideSelection = {default=1};
	__property HotTrack = {default=0};
	__property HotTrackStyles = {default=0};
	__property HoverTime = {default=-1};
	__property IconOptions;
	__property Items;
	__property LargeImages;
	__property MultiSelect = {default=1};
	__property OwnerData = {default=0};
	__property OwnerDraw = {default=0};
	__property ReadOnly = {default=0};
	__property RowSelect = {default=0};
	__property ParentBiDiMode = {default=1};
	__property ParentColor = {default=0};
	__property ParentFont = {default=1};
	__property ParentShowHint = {default=1};
	__property PopupMenu;
	__property ShowColumnHeaders = {default=1};
	__property ShowWorkAreas = {default=0};
	__property ShowHint;
	__property SmallImages;
	__property SortType = {default=0};
	__property StateImages;
	__property TabOrder = {default=-1};
	__property TabStop = {default=1};
	__property ViewStyle = {default=0};
	__property Visible = {default=1};
	__property OnAdvancedCustomDraw;
	__property OnAdvancedCustomDrawItem;
	__property OnAdvancedCustomDrawSubItem;
	__property OnChange;
	__property OnChanging;
	__property OnClick;
	__property OnColumnClick;
	__property OnColumnDragged;
	__property OnColumnRightClick;
	__property OnCompare;
	__property OnContextPopup;
	__property OnCustomDraw;
	__property OnCustomDrawItem;
	__property OnCustomDrawSubItem;
	__property OnData;
	__property OnDataFind;
	__property OnDataHint;
	__property OnDataStateChange;
	__property OnDblClick;
	__property OnDeletion;
	__property OnDrawItem;
	__property OnEdited;
	__property OnEditing;
	__property OnEndDock;
	__property OnEndDrag;
	__property OnEnter;
	__property OnExit;
	__property OnGetImageIndex;
	__property OnGetSubItemImage;
	__property OnDragDrop;
	__property OnDragOver;
	__property OnInfoTip;
	__property OnInsert;
	__property OnKeyDown;
	__property OnKeyPress;
	__property OnKeyUp;
	__property OnMouseDown;
	__property OnMouseMove;
	__property OnMouseUp;
	__property OnResize;
	__property OnSelectItem;
	__property OnStartDock;
	__property OnStartDrag;
	__property OnSecondaryColumnHeader;
public:
	/* TCustomIEListView.Create */ inline __fastcall virtual TIEListView(System::Classes::TComponent* AOwner) : TCustomIEListView(AOwner) { }
	/* TCustomIEListView.Destroy */ inline __fastcall virtual ~TIEListView() { }
	
public:
	/* TWinControl.CreateParented */ inline __fastcall TIEListView(HWND ParentWindow) : TCustomIEListView(ParentWindow) { }
	
};


//-- var, const, procedure ---------------------------------------------------
extern DELPHI_PACKAGE Vcl::Controls::TDragImageList* GlobalDragImageList;
extern DELPHI_PACKAGE void __fastcall Register();
}	/* namespace Ielistview */
#if !defined(DELPHIHEADER_NO_IMPLICIT_NAMESPACE_USE) && !defined(NO_USING_NAMESPACE_IELISTVIEW)
using namespace Ielistview;
#endif
#pragma pack(pop)
#pragma option pop

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// IEListViewHPP
