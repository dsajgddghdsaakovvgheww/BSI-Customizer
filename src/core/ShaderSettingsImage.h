#pragma once
#include "QtResources.h"
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// Read-only, bounded preparation for the native shader category. No PE import
// is added: QQmlEngine loads a regular matching-Qt plugin outside DllMain.
// The EXE's size never changes. A caller must separately transact and verify
// the executable + owned plugin package before claiming an installation.
namespace ShaderSettingsImage {
struct Change {
    QString path;
    QString leaf;
    QByteArray before;
    QByteArray after;
};
struct Plan {
    bool ok = false;
    bool changed = false;
    bool installed = false;
    int qtMajor = 0;
    int tabIndex = -1;
    QByteArray output;
    QList<Change> sources;
    QString error;
};
// Case-insensitive logical path of a registered record ("qml/UiButton.qml").
QString registeredPath(const QtResources::File &file);
// The paths a stock control may legitimately be registered under. A prefix is
// not a rename: "plr.qml" matches "qrc:/plr.qml" and "qml/plr.qml", never
// "Old-plr.qml". Exposed so the rule is unit-tested without a full EXE.
bool controlPathMatches(const QString &control, const QString &registeredPath);
// Resolve the stock controls the Shaders page imports against a registered file
// list. Returns an empty error on success; otherwise the exact refusal text.
// `image` enables the payload checks (readable, and identical when a control is
// registered more than once); pass null to test the matching rule alone.
// `pools` optionally carries QtResources::enumerationReport() lines, appended
// to a refusal so a player whose resource pool is damaged can be diagnosed.
QString stockControlProblem(const QList<QtResources::File> &entries, const QByteArray *image = nullptr,
                            const QStringList *pools = nullptr);
Plan prepare(const QByteArray &image, bool enabled);
// Also used by tests/other planners to stage a known resource. The input is
// untouched on failure; registered aliases and source-specific caches are
// verified by the shared resource writer. Never appends storage/PE sections.
bool replaceSource(QByteArray &image, const QString &leaf, const QByteArray &source,
                    QString *error);
} // namespace ShaderSettingsImage
