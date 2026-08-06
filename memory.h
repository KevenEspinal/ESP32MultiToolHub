#pragma once
#include <Preferences.h>
Preferences nvsVault;

void saveCredential(String siteName, String encryptedPassword) {
  // Open the "Vault" namespace in Read/Write mode (false)
  nvsVault.begin("Vault", false);

  // Save the string under the key of the site name
  nvsVault.putString(siteName.c_str(), encryptedPassword);

  // Close the vault to free up memory
  nvsVault.end();
}

String loadCredential(String siteName) {
  // Open the "Vault" namespace in Read-Only mode (true)
  nvsVault.begin("Vault", true);

  // Retrieve the string. The second argument ("") is the default fallback
  // just in case the key doesn't actually exist in memory yet.
  String retrievedData = nvsVault.getString(siteName.c_str(), "");  // Fixed capital 'C'

  nvsVault.end();

  return retrievedData;
}