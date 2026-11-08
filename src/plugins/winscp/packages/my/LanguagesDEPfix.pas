unit LanguagesDEPfix;

{ Originally a patch for a DEP crash in SysUtils.TLanguages of Delphi 5-7.     }
{ That bug was fixed in the Delphi 2005 RTL, and the patch relied on private   }
{ TLanguages internals, so for modern RAD Studio (incl. Win64) it is reduced   }
{ to a thin wrapper around the standard SysUtils.Languages function.           }

interface

uses
  SysUtils;

function LanguagesDEPF: TLanguages;

implementation

function LanguagesDEPF: TLanguages;
begin
  Result := Languages;
end;

end.
