#include "core/SecretStore.h"

#include <QPointer>

#if __has_include(<qt6keychain/keychain.h>)
#include <qt6keychain/keychain.h>
#else
#include <keychain.h>
#endif

namespace {
const QString kService = QStringLiteral("RetroMail");
}

namespace SecretStore {

void read(const QString &key, QObject *context, ReadCallback callback)
{
    auto *job = new QKeychain::ReadPasswordJob(kService);
    job->setAutoDelete(true);
    job->setKey(key);
    QPointer<QObject> guard(context);
    QObject::connect(job, &QKeychain::Job::finished, context, [job, guard, callback] {
        if (!guard)
            return;
        if (job->error() != QKeychain::NoError)
            callback(false, {}, job->errorString());
        else
            callback(true, job->textData(), {});
    });
    job->start();
}

void write(const QString &key, const QString &secret, QObject *context, WriteCallback callback)
{
    auto *job = new QKeychain::WritePasswordJob(kService);
    job->setAutoDelete(true);
    job->setKey(key);
    job->setTextData(secret);
    QPointer<QObject> guard(context);
    QObject::connect(job, &QKeychain::Job::finished, context, [job, guard, callback] {
        if (!guard)
            return;
        if (job->error() != QKeychain::NoError)
            callback(false, job->errorString());
        else
            callback(true, {});
    });
    job->start();
}

void remove(const QString &key)
{
    auto *job = new QKeychain::DeletePasswordJob(kService);
    job->setAutoDelete(true);
    job->setKey(key);
    job->start();
}

} // namespace SecretStore
