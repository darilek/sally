//---------------------------------------------------------------------------
#include <vcl.h>
#include <Finally.h>
#pragma hdrstop

#include <Common.h>
#include <TextsWin.h>

#include <VCLCommon.h>
#include "WinInterface.h"
#include "License.h"
#include "Tools.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#ifndef NO_RESOURCES
#pragma resource "*.dfm"
#endif
//---------------------------------------------------------------------------
AnsiString LicenseStr[2] = { "LICENSE", "LICENSE_PUTTY" };
//---------------------------------------------------------------------------
void __fastcall DoLicenseDialog(TLicense License)
{
  TLicenseDialog * LicenseDialog = NULL;
  { // try/__finally -> scope guard (bcc64x)
    FINALLY_GUARD([&]() {
    delete LicenseDialog;
  });
    LicenseDialog = new TLicenseDialog(Application, License);
    LicenseDialog->ShowModal();
  }
}
//---------------------------------------------------------------------------
__fastcall TLicenseDialog::TLicenseDialog(TComponent * Owner, TLicense License)
  : TForm(Owner)
{
  UseSystemSettings(this);

  TStrings * LicenseList = new TStringList();
  { // try/__finally -> scope guard (bcc64x)
    FINALLY_GUARD([&]() {
    delete LicenseList;
  });
    LicenseList->Text = ReadResource(LicenseStr[License]);
    assert(LicenseList->Count > 0);
    Caption = FMTLOAD(LICENSE_CAPTION, (LicenseList->Strings[0]));
    LicenseList->Delete(0);
    LicenseMemo->Lines->Text = LicenseList->Text;
  }
}
