//----------------------------------------------------------------------------
#ifndef ConsoleH
#define ConsoleH
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
#include <HistoryComboBox.hpp>
#include <PathLabel.hpp>
#include <ImgList.hpp>
#include <Menus.hpp>
#include <ActnList.hpp>
#include <StdActns.hpp>
//----------------------------------------------------------------------------
#include "WinInterface.h"
#include <Terminal.h>
//----------------------------------------------------------------------------
class TConsoleDialog : public TForm
{
__published:
  TMemo *OutputMemo;
  TBevel *Bevel1;
  TLabel *Label1;
  TLabel *Label2;
  TLabel *Label4;
  TButton *CancelBtn;
  THistoryComboBox *CommandEdit;
  TButton *ExecuteButton;
  TPathLabel *DirectoryLabel;
  TButton *HelpButton;
  TImageList *Images;
  TPopupMenu *PopupMenu;
  TMenuItem *SelectAllItem;
  TMenuItem *CopyItem;
  TMenuItem *N1;
  TMenuItem *AdjustWindowItem;
  TActionList *ActionList;
  TEditCopy *EditCopy;
  TEditSelectAll *EditSelectAll;
  TAction *AdjustWindow;
  void __fastcall ExecuteButtonClick(TObject *Sender);
  void __fastcall CommandEditChange(TObject *Sender);
  void __fastcall HelpButtonClick(TObject *Sender);
  void __fastcall ActionListExecute(TBasicAction *Action, bool &Handled);
  void __fastcall ActionListUpdate(TBasicAction *Action, bool &Handled);
  void __fastcall FormShow(TObject *Sender);
  void __fastcall OutputMemoContextPopup(TObject *Sender, TPoint &MousePos,
          bool &Handled);

private:
  TTerminal * FTerminal;
  TTerminal * FLastTerminal;
  TNotifyEvent FOldChangeDirectory;
  TNotifyEvent FPrevTerminalClose;
  TRect FAutoBounds;
  bool FClearExceptionOnFail;
  bool FAnyCommandExecuted;

  void __fastcall DoExecuteCommand();
  void __fastcall ExecuteCommand();
  void __fastcall SetTerminal(TTerminal * value);
  void __fastcall TerminalClose(TObject * Sender);
  void __fastcall AddLine(const AnsiString & Line, bool StdError);

protected:
  void __fastcall DoChangeDirectory(TObject * Sender);
  void __fastcall UpdateControls();
  virtual void __fastcall CreateParams(TCreateParams & Params);
  void __fastcall DoAdjustWindow();

public:
  virtual __fastcall ~TConsoleDialog();
    virtual __fastcall TConsoleDialog(TComponent* AOwner);
  bool __fastcall Execute(const AnsiString Command = "",
    const TStrings * Log = NULL);
  __property TTerminal * Terminal = { read = FTerminal, write = SetTerminal };
};
//----------------------------------------------------------------------------
#endif
