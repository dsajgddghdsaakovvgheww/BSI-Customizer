#pragma once
#include "ByteUtils.h"
#include "settings_fixture.h"
#include <QMap>
#include <algorithm>

namespace ShaderSettingsImageFixture {
using namespace ByteUtils;
inline quint32 hash(const QString &name)
{
    quint32 value = 0;
    for (auto c : name) { value = (value << 4) + c.unicode(); value ^= (value & 0xf0000000) >> 23; value &= 0x0fffffff; }
    return value;
}
inline QStringList nativeControlNames()
{
    return ShaderSettingsFixture::nativeControls;
}
// `controlLeafSuffix` appends text to each recorded stock-control leaf name
// (for example a player that registers "UiThemeObject.qml" only under a
// different name). The default records the plain leaf.
inline QByteArray fixture(bool dense = false, bool withNativeControls = true, const QString &omittedNativeControl = {},
                          const QString &controlLeafSuffix = {})
{
    QByteArray image(0x20000, '\0');
    image.replace(0, 2, "MZ"); putLe32(image, 0x3c, 0x80);
    image.replace(0x80, 4, QByteArray("PE\0\0", 4));
    putLe16(image, 0x84, 0x8664); putLe16(image, 0x86, 2);
    putLe16(image, 0x94, 0xf0); putLe16(image, 0x98, 0x20b);
    putLe32(image, 0x98 + 32, 0x1000); putLe32(image, 0x98 + 36, 0x200);
    putLe32(image, 0x98 + 56, 0x20000); putLe32(image, 0x98 + 60, 0x1000); putLe32(image, 0x98 + 108, 16);
    image.replace(0x188, 8, QByteArray(".rdata\0\0", 8));
    putLe32(image, 0x190, 0x17000); putLe32(image, 0x194, 0x1000);
    putLe32(image, 0x198, 0x17000); putLe32(image, 0x19c, 0x1000); putLe32(image, 0x1ac, 0x40000040);
    image.replace(0x1b0, 8, QByteArray(".text\0\0\0", 8));
    putLe32(image, 0x1b8, 0x8000); putLe32(image, 0x1bc, 0x18000);
    putLe32(image, 0x1c0, 0x8000); putLe32(image, 0x1c4, 0x18000); putLe32(image, 0x1d4, 0x60000020);
    if (dense) image.replace(0x1000, 0x17000, QByteArray(0x17000, char(0xa5)));
    image.replace(0x18000, 0x8000, QByteArray(0x8000, char(0x90)));
    // A real qRegisterResourceData import and x64 call site, never executed.
    image.replace(0x1000, 0x300, QByteArray(0x300, '\0'));
    putLe32(image, 0x98 + 120, 0x1000); putLe32(image, 0x98 + 124, 40);
    putLe32(image, 0x1000, 0x1100); putLe32(image, 0x100c, 0x1200); putLe32(image, 0x1010, 0x1120);
    putLe32(image, 0x1100, 0x1220); putLe32(image, 0x1120, 0x1220);
    const auto dll = QByteArray("Qt6Core.dll\0", 12);
    const auto symbol = QByteArray("?qRegisterResourceData@@YA_NHPEBE00@Z\0", 36);
    image.replace(0x1200, dll.size(), dll); image.replace(0x1222, symbol.size(), symbol);
    QByteArray code;
    for (const auto &arg : {qMakePair(QByteArray::fromHex("4c8d0d"), 0x2000), qMakePair(QByteArray::fromHex("4c8d05"), 0x6000), qMakePair(QByteArray::fromHex("488d15"), 0x7000)}) {
        const int begin = code.size(); code += arg.first + QByteArray(4, '\0');
        putLe32(code, begin + 3, quint32(arg.second - (0x18000 + code.size())));
    }
    code += QByteArray::fromHex("b902000000ff1500000000c3");
    putLe32(code, 28, quint32(0x1120 - (0x18000 + 32)));
    image.replace(0x18000, code.size(), code);
    QMap<QString, QByteArray> sources;
    sources.insert(QStringLiteral("plr.qml"), ShaderSettingsFixture::player().toUtf8());
    auto window = ShaderSettingsFixture::window();
    window.replace(QStringLiteral("//BSI v17"), QStringLiteral("//BSI v21"));
    sources.insert(QStringLiteral("UiSettingsWindow.qml"), window.toUtf8());
    // Resource-namespace doubles only. Real component loading is separately
    // tested against read-only resources from the four pinned player samples.
    if (withNativeControls)
        for (const auto &name : nativeControlNames())
            if (name != omittedNativeControl) sources.insert(controlLeafSuffix + name, QByteArray("import QtQuick 2.15\nItem{}\n"));
    for (const auto &page : ShaderSettingsFixture::pages) sources.insert(page, QByteArray("import QtQuick 2.15\nItem{}\n"));
    sources.insert(QStringLiteral("Unrelated.qml"), QByteArray("import QtQuick 2.15\nItem{property string untouched:'user edit'}\n"));
    QStringList ordered = sources.keys();
    std::sort(ordered.begin(), ordered.end(), [](const QString &a, const QString &b) { return hash(a) < hash(b); });
    image.replace(0x7000, (ordered.size() + 1) * 22, QByteArray((ordered.size() + 1) * 22, '\0'));
    putBe16(image, 0x7004, 2); putBe32(image, 0x7006, ordered.size()); putBe32(image, 0x700a, 1);
    qint64 data = 0x2000, name = 0x6000;
    for (int i = 0; i < ordered.size(); ++i) {
        const auto &leaf = ordered.at(i);
        const auto payload = qCompress(sources.value(leaf), 9);
        QByteArray record(4, '\0'); putBe32(record, 0, payload.size()); record += payload;
        image.replace(data, record.size(), record);
        const qint64 node = 0x7000 + (i + 1) * 22;
        putBe32(image, node, name - 0x6000); putBe16(image, node + 4, 1);
        putBe16(image, node + 8, 1); putBe32(image, node + 10, data - 0x2000); putBe64(image, node + 14, 1600000000000ULL);
        putBe16(image, name, leaf.size()); putBe32(image, name + 2, hash(leaf));
        for (int c = 0; c < leaf.size(); ++c) putBe16(image, name + 6 + c * 2, leaf.at(c).unicode());
        name += 6 + leaf.size() * 2; data += record.size();
    }
    return image;
}

// The shape of a shipped player whose *second* resource registration carries the
// stock controls: the window/panel QML lives in one pool (all of it valid) and
// the Ui* control library in another, while the control pool's name table was
// partly destroyed after linking. One name record is turned into a length/hash
// word followed by zeroes - exactly what the MSI 5.12 build carries - so the
// strict filesInPool() walk refuses the whole pool and enumeration alone has to
// still find the controls. Both pools are explicit qRegisterResourceData
// registrations, as in the real binary.
inline QByteArray splitFixture(bool corruptControlName = true, bool omitThemeControl = false)
{
    QByteArray image(0x20000, '\0');
    image.replace(0, 2, "MZ"); putLe32(image, 0x3c, 0x80);
    image.replace(0x80, 4, QByteArray("PE\0\0", 4));
    putLe16(image, 0x84, 0x8664); putLe16(image, 0x86, 3);
    putLe16(image, 0x94, 0xf0); putLe16(image, 0x98, 0x20b);
    putLe32(image, 0x98 + 32, 0x1000); putLe32(image, 0x98 + 36, 0x200);
    putLe32(image, 0x98 + 56, 0x20000); putLe32(image, 0x98 + 60, 0x1000); putLe32(image, 0x98 + 108, 16);
    // .rdata holds pools, names and payloads; .text holds the two call sites.
    // RVA == file offset in both, so a displacement written while building the
    // code at 0x18000 stays correct whatever the section headers say.
    image.replace(0x188, 8, QByteArray(".rdata\0\0", 8));
    putLe32(image, 0x190, 0x17000); putLe32(image, 0x194, 0x1000);
    putLe32(image, 0x198, 0x17000); putLe32(image, 0x19c, 0x1000); putLe32(image, 0x1ac, 0x40000040);
    image.replace(0x1b0, 8, QByteArray(".text\0\0\0", 8));
    putLe32(image, 0x1b8, 0x8000); putLe32(image, 0x1bc, 0x18000);
    putLe32(image, 0x1c0, 0x8000); putLe32(image, 0x1c4, 0x18000); putLe32(image, 0x1d4, 0x60000020);
    image.replace(0x18000, 0x8000, QByteArray(0x8000, char(0x90)));
    image.replace(0x1000, 0x300, QByteArray(0x300, '\0'));
    putLe32(image, 0x98 + 120, 0x1000); putLe32(image, 0x98 + 124, 40);
    putLe32(image, 0x1000, 0x1100); putLe32(image, 0x100c, 0x1200); putLe32(image, 0x1010, 0x1120);
    putLe32(image, 0x1100, 0x1220); putLe32(image, 0x1120, 0x1220);
    image.replace(0x1200, 12, QByteArray("Qt6Core.dll\0", 12));
    image.replace(0x1222, 36, QByteArray("?qRegisterResourceData@@YA_NHPEBE00@Z\0", 36));
    // Two qRegisterResourceData(3, tree, names, data) call sites, in argument order.
    struct Pool { quint32 tree, names, data; };
    // Panel pool: payloads 0x2000, names 0x9000, tree 0xa000.
    // Control pool: payloads 0x5000, names 0xb000, tree 0xc000.
    // The regions never overlap, so a longer rewritten panel payload cannot
    // land inside the control pool's records.
    const Pool pools[2] { { 0xa000, 0x9000, 0x2000 }, { 0xc000, 0xb000, 0x5000 } };
    QByteArray code;
    for (const auto &pool : pools) {
        for (const auto &arg : {qMakePair(QByteArray::fromHex("4c8d0d"), pool.data),
                                 qMakePair(QByteArray::fromHex("4c8d05"), pool.names),
                                 qMakePair(QByteArray::fromHex("488d15"), pool.tree)}) {
            const int begin = code.size(); code += arg.first + QByteArray(4, '\0');
            putLe32(code, begin + 3, quint32(arg.second - (0x18000 + code.size())));
        }
        // ECX=version, then call qword ptr [rip+disp] and return.
        code += QByteArray::fromHex("b903000000ff1500000000c3");
        const int call = code.size() - 7;            // the FF 15 instruction
        const int next = call + 6;                   // RIP-relative base
        putLe32(code, call + 2, quint32(0x1120 - (0x18000 + next)));
    }
    image.replace(0x18000, code.size(), code);
    QMap<QString, QByteArray> panel, controls;
    panel.insert(QStringLiteral("plr.qml"), ShaderSettingsFixture::player().toUtf8());
    auto window = ShaderSettingsFixture::window();
    window.replace(QStringLiteral("//BSI v17"), QStringLiteral("//BSI v21"));
    panel.insert(QStringLiteral("UiSettingsWindow.qml"), window.toUtf8());
    for (const auto &page : ShaderSettingsFixture::pages) panel.insert(page, QByteArray("import QtQuick 2.15\nItem{}\n"));
    for (const auto &name : nativeControlNames())
        if (!(omitThemeControl && name == QLatin1String("UiThemeObject.qml")))
            controls.insert(name, QByteArray("import QtQuick 2.15\nItem{}\n"));
    controls.insert(QStringLiteral("UiSliderHandle.qml"), QByteArray("import QtQuick 2.15\nItem{}\n"));
    // One record that is genuinely unreadable must stay skipped, and it sorts
    // last so the damaged entry is a resource nobody imports.
    controls.insert(QStringLiteral("ZzUnreadable.svg"), QByteArray("<svg/>"));
    const auto write = [&](const QMap<QString, QByteArray> &sources, qint64 tree, qint64 names, qint64 data,
                           bool corrupt) {
        QStringList ordered = sources.keys();
        std::sort(ordered.begin(), ordered.end(), [](const QString &a, const QString &b) { return hash(a) < hash(b); });
        putBe16(image, tree + 4, 2); putBe32(image, tree + 6, ordered.size()); putBe32(image, tree + 10, 1);
        qint64 at = data, name = names;
        for (int i = 0; i < ordered.size(); ++i) {
            const auto &leaf = ordered.at(i);
            const auto payload = qCompress(sources.value(leaf), 9);
            QByteArray record(4, '\0'); putBe32(record, 0, payload.size()); record += payload;
            image.replace(at, record.size(), record);
            const qint64 node = tree + (i + 1) * 22;
            putBe32(image, node, quint32(name - names)); putBe16(image, node + 4, 1);
            putBe16(image, node + 8, 1); putBe32(image, node + 10, quint32(at - data));
            putBe64(image, node + 14, 1600000000000ULL);
            putBe16(image, name, quint16(leaf.size())); putBe32(image, name + 2, hash(leaf));
            for (int c = 0; c < leaf.size(); ++c) putBe16(image, name + 6 + c * 2, leaf.at(c).unicode());
            name += 6 + leaf.size() * 2; at += record.size();
        }
        // One name record only: length/hash word kept, every character zeroed.
        // The node, its payload and the rest of the table stay intact - the
        // exact damage shape found in the shipped build.
        if (corrupt) {
            for (int i = 0; i < ordered.size(); ++i) {
                if (ordered.at(i) != QLatin1String("ZzUnreadable.svg")) continue;
                const qint64 record = names + be32(image, tree + (i + 1) * 22);
                const quint16 length = be16(image, record);
                image.replace(record + 6, length * 2, QByteArray(length * 2, '\0'));
            }
        }
    };
    write(panel, 0xa000, 0x9000, 0x2000, false);
    write(controls, 0xc000, 0xb000, 0x5000, corruptControlName);
    return image;
}
} // namespace ShaderSettingsImageFixture
