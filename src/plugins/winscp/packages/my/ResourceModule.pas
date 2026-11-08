unit ResourceModule;

{ Originally a copy of the Delphi 5 RTL resource-DLL lookup with hand-written  }
{ 32-bit API declarations. Not used by the WinSCP plugin; kept only as a thin  }
{ wrapper around the modern RTL so that old project files still compile.      }

interface

function GetResourceModule(ModuleName: PChar): string;

implementation

function GetResourceModule(ModuleName: PChar): string;
begin
  // unused: no satellite resource DLL lookup needed
  Result := '';
end;

end.
