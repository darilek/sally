// Reading a private key file into memory (secret-safe) and recognising its format.
#pragma once

#include <string>

#include "secret.h"

// Reads the whole file (UTF-8 path, any Unicode name) into out. The bytes live in a CSecret, so they are wiped when it
// is cleared or destroyed. Returns false with a message in why; out is empty then. Refuses files over 1 MiB.
bool ReadKeyFile(const std::string& utf8Path, CSecret& out, std::string& why);

// True for a PuTTY private key file ("PuTTY-User-Key-File-2:" / "-3:" header).
bool IsPuttyKey(const CSecret& key);
