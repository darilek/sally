//---------------------------------------------------------------------------
#include <vcl.h>
#include <Finally.h>
#include <type_traits>
#pragma hdrstop

#include <Common.h>
#include <TextsCore.h>
#include <SessionData.h>
#include <CoreMain.h>
#include <Interface.h>
#include "CustomWinConfiguration.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
class THistoryStrings : public TStringList
{
public:
  __fastcall THistoryStrings() : TStringList()
  {
    FModified = false;
  };

  __property bool Modified = { read = FModified, write = FModified };

private:
  bool FModified;
};
//---------------------------------------------------------------------------
__fastcall TCustomWinConfiguration::TCustomWinConfiguration():
  TGUIConfiguration()
{
  FHistory = new TStringList();
  FEmptyHistory = new TStringList();
  FDefaultInterface = ifCommander;
  FDefaultShowAdvancedLoginOptions = false;
}
//---------------------------------------------------------------------------
__fastcall TCustomWinConfiguration::~TCustomWinConfiguration()
{
  ClearHistory();
  delete FHistory;
  delete FEmptyHistory;
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::ClearHistory()
{
  assert(FHistory != NULL);

  THistoryStrings * HistoryStrings;
  for (int Index = 0; Index < FHistory->Count; Index++)
  {
    HistoryStrings = dynamic_cast<THistoryStrings *>(FHistory->Objects[Index]);
    FHistory->Objects[Index] = NULL;
    delete HistoryStrings;
  }
  FHistory->Clear();
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::DefaultHistory()
{
  ClearHistory();

  // defaults for speed limits
  THistoryStrings * Strings = new THistoryStrings();
  Strings->Add(LoadStr(SPEED_UNLIMITED));
  unsigned long Speed = 1024;
  while (Speed >= 8)
  {
    Strings->Add(IntToStr((__int64)Speed));
    Speed = Speed / 2;
  }
  FHistory->AddObject("SpeedLimit", Strings);
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::Default()
{
  TGUIConfiguration::Default();

  FShowAdvancedLoginOptions = FDefaultShowAdvancedLoginOptions;
  FInterface = FDefaultInterface;
  FLogView = lvNone;
  FSynchronizeChecklist.WindowParams = "0;-1;-1;600;450;0";
  FSynchronizeChecklist.ListParams = "1;1|150,1;100,1;80,1;130,1;25,1;100,1;80,1;130,1|0;1;2;3;4;5;6;7";
  FFindFile.WindowParams = "646,481";
  FFindFile.ListParams = "3;1|125,1;181,1;80,1;122,1|0;1;2;3";
  FConsoleWin.WindowSize = "570,430";
  FConfirmExitOnCompletion = true;
  FOperationProgressOnTop = true;

  DefaultHistory();
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::Saved()
{
  TGUIConfiguration::Saved();

  THistoryStrings * HistoryStrings;
  for (int Index = 0; Index < FHistory->Count; Index++)
  {
    HistoryStrings = dynamic_cast<THistoryStrings *>(FHistory->Objects[Index]);
    assert(HistoryStrings != NULL);
    HistoryStrings->Modified = false;
  }
}
//---------------------------------------------------------------------------
// duplicated from core\configuration.cpp
#define LASTELEM(ELEM) \
  ELEM.SubString(ELEM.LastDelimiter(".>")+1, ELEM.Length() - ELEM.LastDelimiter(".>"))
#define BLOCK(KEY, CANCREATE, BLOCK) \
  if (Storage->OpenSubKey(KEY, CANCREATE, true)) { FINALLY_GUARD([&]() { Storage->CloseSubKey(); }); BLOCK }
#define REGCONFIG(CANCREATE) \
  BLOCK("Interface", CANCREATE, \
    KEY(Integer,  Interface); \
    KEY(Bool,     ShowAdvancedLoginOptions); \
    KEY(Bool,     ConfirmExitOnCompletion); \
  ) \
  BLOCK("Logging", CANCREATE, \
    KEY(Integer, LogView); \
  ) \
  BLOCK("Interface\\SynchronizeChecklist", CANCREATE, \
    KEY(String,   SynchronizeChecklist.WindowParams); \
    KEY(String,   SynchronizeChecklist.ListParams); \
  ); \
  BLOCK("Interface\\FindFile", CANCREATE, \
    KEY(String,   FindFile.WindowParams); \
    KEY(String,   FindFile.ListParams); \
  ); \
  BLOCK("Interface\\ConsoleWin", CANCREATE, \
    KEY(String,   ConsoleWin.WindowSize); \
  ); \
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SaveData(
  THierarchicalStorage * Storage, bool All)
{
  TGUIConfiguration::SaveData(Storage, All);

  // duplicated from core\configuration.cpp
  #define KEY(TYPE, VAR) Storage->Write ## TYPE(LASTELEM(AnsiString(#VAR)), VAR)
  REGCONFIG(true);
  #undef KEY

  if (FHistory->Count > 0)
  {
    if (Storage->OpenSubKey("History", true))
    {
      { // try/__finally -> scope guard (bcc64x)
        FINALLY_GUARD([&]() {
        Storage->CloseSubKey();
      });
        THistoryStrings * HistoryStrings;
        for (int Index = 0; Index < FHistory->Count; Index++)
        {
          HistoryStrings = dynamic_cast<THistoryStrings *>(FHistory->Objects[Index]);
          assert(HistoryStrings != NULL);
          if (All || HistoryStrings->Modified)
          {
            if (Storage->OpenSubKey(FHistory->Strings[Index], true))
            {
              { // try/__finally -> scope guard (bcc64x)
                FINALLY_GUARD([&]() {
                Storage->CloseSubKey();
              });
                Storage->WriteValues(HistoryStrings);
              }
            }
          }
        }
      }
    }

    if (Storage->OpenSubKey("HistoryParams", true))
    {
      { // try/__finally -> scope guard (bcc64x)
        FINALLY_GUARD([&]() {
        Storage->CloseSubKey();
      });
        THistoryStrings * HistoryStrings;
        for (int Index = 0; Index < FHistory->Count; Index++)
        {
          HistoryStrings = dynamic_cast<THistoryStrings *>(FHistory->Objects[Index]);
          assert(HistoryStrings != NULL);
          if (All || HistoryStrings->Modified)
          {
            bool HasData = false;
            for (int VIndex = 0; !HasData && (VIndex < HistoryStrings->Count); VIndex++)
            {
              HasData = (HistoryStrings->Objects[VIndex] != NULL);
            }

            if (!HasData)
            {
              Storage->RecursiveDeleteSubKey(FHistory->Strings[Index]);
            }
            else if (Storage->OpenSubKey(FHistory->Strings[Index], true))
            {
              { // try/__finally -> scope guard (bcc64x)
                FINALLY_GUARD([&]() {
                Storage->CloseSubKey();
              });
                Storage->ClearValues();
                for (int VIndex = 0; VIndex < HistoryStrings->Count; VIndex++)
                {
                  void * Data = HistoryStrings->Objects[VIndex];
                  Storage->WriteBinaryData(IntToStr(VIndex), &Data, sizeof(Data));
                }
              }
            }
          }
        }
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::LoadData(
  THierarchicalStorage * Storage)
{
  TGUIConfiguration::LoadData(Storage);

  // duplicated from core\configuration.cpp
  #define KEY(TYPE, VAR) VAR = (std::decay_t<decltype(VAR)>)Storage->Read ## TYPE(LASTELEM(AnsiString(#VAR)), VAR)
  #pragma warn -eas
  REGCONFIG(false);
  #pragma warn +eas
  #undef KEY

  DefaultHistory();
  if (Storage->OpenSubKey("History", false))
  {
    TStrings * Names = NULL;
    { // try/__finally -> scope guard (bcc64x)
      FINALLY_GUARD([&]() {
      Storage->CloseSubKey();
      delete Names;
    });
      Names = new TStringList();
      Storage->GetSubKeyNames(Names);
      for (int Index = 0; Index < Names->Count; Index++)
      {
        if (Storage->OpenSubKey(Names->Strings[Index], false))
        {
          THistoryStrings * HistoryStrings = NULL;
          { // try/__finally -> scope guard (bcc64x)
            FINALLY_GUARD([&]() {
            Storage->CloseSubKey();
            delete HistoryStrings;
          });
            // remove defaults, if any
            int HIndex = FHistory->IndexOf(Names->Strings[Index]);
            if (HIndex >= 0)
            {
              THistoryStrings * DefaultStrings = dynamic_cast<THistoryStrings *>(FHistory->Objects[HIndex]);
              delete DefaultStrings;
              FHistory->Delete(HIndex);
            }

            HistoryStrings = new THistoryStrings();
            Storage->ReadValues(HistoryStrings);
            FHistory->AddObject(Names->Strings[Index], HistoryStrings);
            HistoryStrings = NULL;
          }
        }
      }
    }
  }

  if (Storage->OpenSubKey("HistoryParams", false))
  {
    { // try/__finally -> scope guard (bcc64x)
      FINALLY_GUARD([&]() {
      Storage->CloseSubKey();
    });
      THistoryStrings * HistoryStrings;
      for (int Index = 0; Index < FHistory->Count; Index++)
      {
        HistoryStrings = dynamic_cast<THistoryStrings *>(FHistory->Objects[Index]);
        if (Storage->OpenSubKey(FHistory->Strings[Index], false))
        {
          { // try/__finally -> scope guard (bcc64x)
            FINALLY_GUARD([&]() {
            Storage->CloseSubKey();
          });
            for (int VIndex = 0; VIndex < HistoryStrings->Count; VIndex++)
            {
              void * Data;
              if (Storage->ReadBinaryData(IntToStr(VIndex), &Data, sizeof(Data)) ==
                      sizeof(Data))
              {
                HistoryStrings->Objects[VIndex] = reinterpret_cast<TObject *>(Data);
              }
            }
          }
        }
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::LoadAdmin(THierarchicalStorage * Storage)
{
  TGUIConfiguration::LoadAdmin(Storage);
  FDefaultInterface = TInterface(Storage->ReadInteger("DefaultInterfaceInterface", FDefaultInterface));
  FDefaultShowAdvancedLoginOptions = Storage->ReadBool("DefaultInterfaceShowAdvancedLoginOptions", FDefaultShowAdvancedLoginOptions);
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::RecryptPasswords()
{
  Busy(true);
  { // try/__finally -> scope guard (bcc64x)
    FINALLY_GUARD([&]() {
    Busy(false);
  });
    StoredSessions->RecryptPasswords();
    if (OnMasterPasswordRecrypt != NULL)
    {
      OnMasterPasswordRecrypt(NULL);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SetShowAdvancedLoginOptions(bool value)
{
  SET_CONFIG_PROPERTY(ShowAdvancedLoginOptions);
}
//---------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SetLogView(TLogView value)
{
  SET_CONFIG_PROPERTY(LogView);
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SetInterface(TInterface value)
{
  SET_CONFIG_PROPERTY(Interface);
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SetHistory(const AnsiString Index,
  TStrings * value)
{
  int I = FHistory->IndexOf(Index);
  bool NonEmpty = (value != NULL) && (value->Count > 0);
  THistoryStrings * HistoryStrings = NULL;
  if (I >= 0)
  {
    HistoryStrings = dynamic_cast<THistoryStrings *>(FHistory->Objects[I]);
    if (HistoryStrings->Equals(value))
    {
      HistoryStrings = NULL;
    }
  }
  else if (NonEmpty)
  {
    HistoryStrings = new THistoryStrings();
    FHistory->AddObject(Index, HistoryStrings);
  }

  if (HistoryStrings != NULL)
  {
    if (NonEmpty)
    {
      HistoryStrings->Assign(value);
      while (HistoryStrings->Count > MaxHistoryCount)
      {
        HistoryStrings->Delete(HistoryStrings->Count - 1);
      }
    }
    else
    {
      HistoryStrings->Clear();
    }
    HistoryStrings->Modified = true;
  }
}
//---------------------------------------------------------------------------
TStrings * __fastcall TCustomWinConfiguration::GetHistory(const AnsiString Index)
{
  int I = FHistory->IndexOf(Index);
  return I >= 0 ? dynamic_cast<TStrings *>(FHistory->Objects[I]) : FEmptyHistory;
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SetSynchronizeChecklist(TSynchronizeChecklistConfiguration value)
{
  SET_CONFIG_PROPERTY(SynchronizeChecklist);
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SetFindFile(TFindFileConfiguration value)
{
  SET_CONFIG_PROPERTY(FindFile);
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SetConsoleWin(TConsoleWinConfiguration value)
{
  SET_CONFIG_PROPERTY(ConsoleWin);
}
//---------------------------------------------------------------------------
void __fastcall TCustomWinConfiguration::SetConfirmExitOnCompletion(bool value)
{
  SET_CONFIG_PROPERTY(ConfirmExitOnCompletion);
}
