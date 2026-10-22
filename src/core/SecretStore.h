#pragma once

#include <QObject>
#include <QString>

#include <functional>

// Thin async wrapper around QtKeychain: macOS Keychain, Windows Credential
// Manager, or the Secret Service (GNOME Keyring / KWallet) on Linux.
namespace SecretStore {

using ReadCallback = std::function<void(bool ok, const QString &secret, const QString &error)>;
using WriteCallback = std::function<void(bool ok, const QString &error)>;

void read(const QString &key, QObject *context, ReadCallback callback);
void write(const QString &key, const QString &secret, QObject *context, WriteCallback callback);
void remove(const QString &key);

} // namespace SecretStore
