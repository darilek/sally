//----------------------------------------------------------------------------
#ifndef FileSystemInfoH
#define FileSystemInfoH
//----------------------------------------------------------------------------
#include <System.hpp>
#include <Windows.hpp>
#include <SysUtils.hpp>
#include <Classes.hpp>
#include <Graphics.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Controls.hpp>
#include <Buttons.hpp>
#include <ExtCtrls.hpp>
#include <ComCtrls.hpp>
#include <Menus.hpp>
//----------------------------------------------------------------------------
typedef void __fastcall (__closure *TFeedFileSystemData)
  (TControl * Control, int Label, AnsiString Value);
//----------------------------------------------------------------------------
class TFileSystemInfoDialog : public TForm
{
__published:
  TButton *CloseButton;
  TButton *HelpButton;
  TPageControl *PageControl;
  TTabSheet *ProtocolSheet;
  TGroupBox *HostKeyGroup;
  TEdit *HostKeyFingerprintEdit;
  TTabSheet *CapabilitiesSheet;
  TGroupBox *InfoGroup;
  TMemo *InfoMemo;
  TListView *ServerView;
  TListView *ProtocolView;
  TButton *ClipboardButton;
  TPopupMenu *ListViewMenu;
  TMenuItem *Copy;
  TTabSheet *SpaceAvailableSheet;
  TListView *SpaceAvailableView;
  TLabel *Label1;
  TEdit *SpaceAvailablePathEdit;
  TButton *SpaceAvailableButton;
  TGroupBox *CertificateGroup;
  TEdit *CertificateFingerprintEdit;
  TButton *CertificateViewButton;
  void __fastcall HelpButtonClick(TObject *Sender);
  void __fastcall ClipboardButtonClick(TObject *Sender);
  void __fastcall CopyClick(TObject *Sender);
  void __fastcall FormShow(TObject *Sender);
  void __fastcall SpaceAvailableButtonClick(TObject *Sender);
  void __fastcall PageControlChange(TObject *Sender);
  void __fastcall ControlChange(TObject *Sender);
  void __fastcall SpaceAvailablePathEditEnter(TObject *Sender);
  void __fastcall SpaceAvailablePathEditExit(TObject *Sender);
  void __fastcall ControlContextPopup(TObject *Sender, TPoint &MousePos,
          bool &Handled);
  void __fastcall CertificateViewButtonClick(TObject *Sender);
public:
  virtual __fastcall TFileSystemInfoDialog(TComponent * AOwner,
    TGetSpaceAvailable OnGetSpaceAvailable);

  void __fastcall Execute(const TSessionInfo & SessionInfo,
    const TFileSystemInfo & FileSystemInfo, AnsiString SpaceAvailablePath);

private:
  TControl * FLastFeededControl;
  AnsiString FClipboard;
  TGetSpaceAvailable FOnGetSpaceAvailable;
  bool FSpaceAvailableLoaded;
  TSpaceAvailable FSpaceAvailable;
  int FLastListItem;
  TSessionInfo FSessionInfo;
  TFileSystemInfo FFileSystemInfo;

  void __fastcall Feed(TFeedFileSystemData AddItem);
  void __fastcall UpdateControls();
  AnsiString __fastcall CapabilityStr(TFSCapability Capability);
  AnsiString __fastcall CapabilityStr(TFSCapability Capability1,
    TFSCapability Capability2);
  AnsiString __fastcall SpaceStr(__int64 Bytes);
  void __fastcall ControlsAddItem(TControl * Control, int Label, AnsiString Value);
  void __fastcall ClipboardAddItem(TControl * Control, int Label, AnsiString Value);
  void __fastcall CheckSpaceAvailable();
  void __fastcall NeedSpaceAvailable();
  bool __fastcall SpaceAvailableSupported();
  void __fastcall FeedControls();
};
//----------------------------------------------------------------------------
#endif
