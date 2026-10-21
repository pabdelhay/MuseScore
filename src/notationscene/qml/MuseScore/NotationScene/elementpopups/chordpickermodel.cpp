/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "chordpickermodel.h"

#include <algorithm>

#include <QHash>

#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/note.h"
#include "engraving/dom/noteval.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/segment.h"

#include "notation/imasternotation.h"
#include "notation/inotationinteraction.h"
#include "notation/inotationplayback.h"

#include "settings.h"

#include "log.h"

using namespace mu::notation;
using namespace mu::engraving;

namespace {
constexpr int PREVIEW_DURATION_US = 650000;
constexpr int RIGHT_HAND_PITCH = 60;       // C4
constexpr int DEFAULT_PIANO_LOW = 48;      // C3
constexpr int SIMPLE_PIANO_SPAN = 36;      // three octaves, C to C
constexpr int ADVANCED_PIANO_SPAN = 48;    // four octaves, C to C

const QStringList ROOTS = {
    QStringLiteral("C"),
    QStringLiteral("C#"),
    QStringLiteral("Db"),
    QStringLiteral("D"),
    QStringLiteral("D#"),
    QStringLiteral("Eb"),
    QStringLiteral("E"),
    QStringLiteral("F"),
    QStringLiteral("F#"),
    QStringLiteral("Gb"),
    QStringLiteral("G"),
    QStringLiteral("G#"),
    QStringLiteral("Ab"),
    QStringLiteral("A"),
    QStringLiteral("A#"),
    QStringLiteral("Bb"),
    QStringLiteral("B"),
};

struct QualityDef {
    const char* suffix;
    const char* label;
    int group;
    bool advanced;
};

// Groups: 0 major, 1 minor, 2 dominant, 3 diminished/augmented, 4 suspended.
// Listed column-major, three buttons per column. The first column is the simple set.
const QualityDef QUALITIES[] = {
    { "",        "maj",       0, false },
    { "6",       "6",         0, false },
    { "maj7",    "maj7",      0, false },
    { "add9",    "add9",      0, true },
    { "maj9",    "maj9",      0, true },
    { "69",      "6/9",       0, true },
    { "maj7#11", "maj7#11",   0, true },
    { "maj13",   "maj13",     0, true },

    { "m",       "m",         1, false },
    { "m6",      "m6",        1, false },
    { "m7",      "m7",        1, false },
    { "m9",      "m9",        1, true },
    { "madd9",   "madd9",     1, true },
    { "m11",     "m11",       1, true },
    { "m13",     "m13",       1, true },
    { "mMaj7",   "m(maj7)",   1, true },
    { "m69",     "m6/9",      1, true },

    { "7",       "7",         2, false },
    { "9",       "9",         2, false },
    { "13",      "13",        2, false },
    { "11",      "11",        2, true },
    { "7b9",     "7b9",       2, true },
    { "7#9",     "7#9",       2, true },
    { "7#11",    "7#11",      2, true },
    { "7b13",    "7b13",      2, true },
    { "7b5",     "7b5",       2, true },
    { "7alt",    "7alt",      2, true },

    { "dim",     "dim",       3, false },
    { "dim7",    "dim7",      3, false },
    { "m7b5",    "m7(b5)",    3, false },
    { "aug",     "aug",       3, true },
    { "aug7",    "aug7",      3, true },
    { "maj7#5",  "aug(maj7)", 3, true },

    { "sus2",    "sus2",      4, false },
    { "sus4",    "sus4",      4, false },
    { "7sus4",   "7sus4",     4, false },
    { "5",       "5",         4, true },
    { "9sus4",   "9sus4",     4, true },
    { "13sus4",  "13sus4",    4, true },
};

const char* const GROUP_KEYS[] = { "major", "minor", "dominant", "dimaug", "sus" };

const char* const SHARP_ROOTS[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
const char* const FLAT_ROOTS[] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

const char* const SETTINGS_MODULE = "notation";

muse::Settings::Key settingsKey(const char* key)
{
    return muse::Settings::Key(SETTINGS_MODULE, key);
}

int pitchClassForRoot(const QString& name)
{
    if (name.isEmpty()) {
        return -1;
    }

    int pc = -1;
    switch (name.at(0).toUpper().unicode()) {
    case u'C': pc = 0;
        break;
    case u'D': pc = 2;
        break;
    case u'E': pc = 4;
        break;
    case u'F': pc = 5;
        break;
    case u'G': pc = 7;
        break;
    case u'A': pc = 9;
        break;
    case u'B': pc = 11;
        break;
    default: return -1;
    }

    if (name.size() > 1) {
        const QChar accidental = name.at(1);
        if (accidental == u'#' || accidental == QChar(0x266F)) {
            ++pc;
        } else if (accidental == u'b' || accidental == QChar(0x266D)) {
            --pc;
        }
    }

    return (pc % 12 + 12) % 12;
}

int normalizePitchClass(int pitch)
{
    return (pitch % 12 + 12) % 12;
}

QString normalizeAccidentals(QString text)
{
    text.replace(QChar(0x266D), u'b');
    text.replace(QChar(0x266F), u'#');
    return text.trimmed();
}

QString degreeName(int interval, bool hasSeventh, bool hasThird, bool hasFifth)
{
    switch (interval) {
    case 0: return QStringLiteral("1");
    case 1: return hasSeventh ? QStringLiteral("b9") : QStringLiteral("b2");
    case 2: return (hasSeventh || hasThird) ? QStringLiteral("9") : QStringLiteral("2");
    case 3: return QStringLiteral("b3");
    case 4: return QStringLiteral("3");
    case 5: return (hasSeventh && hasThird) ? QStringLiteral("11") : QStringLiteral("4");
    case 6: return (hasSeventh && hasFifth) ? QStringLiteral("#11") : QStringLiteral("b5");
    case 7: return QStringLiteral("5");
    case 8: return (hasSeventh && hasFifth) ? QStringLiteral("b13") : QStringLiteral("#5");
    case 9: return hasSeventh ? QStringLiteral("13") : QStringLiteral("6");
    case 10: return QStringLiteral("b7");
    case 11: return QStringLiteral("7");
    default: return QString();
    }
}
}

ChordPickerModel* ChordPickerModel::s_active = nullptr;

ChordPickerModel::ChordPickerModel(QObject* parent)
    : AbstractElementPopupModel(PopupModelType::TYPE_HARMONY, parent)
{
    loadSettings();

    connect(this, &AbstractElementPopupModel::dataChanged, this, [this]() {
        if (m_userAdjusted) {
            return;
        }
        readFromItem();
    });
}

void ChordPickerModel::loadSettings()
{
    muse::settings()->setDefaultValue(settingsKey("ui/chordPicker/expanded"), muse::Val(true));
    muse::settings()->setDefaultValue(settingsKey("ui/chordPicker/advanced"), muse::Val(false));
    muse::settings()->setDefaultValue(settingsKey("ui/chordPicker/preferSharps"), muse::Val(true));
    muse::settings()->setDefaultValue(settingsKey("ui/chordPicker/literal"), muse::Val(true));
    muse::settings()->setDefaultValue(settingsKey("ui/chordPicker/inversion"), muse::Val(0));

    m_expanded = muse::settings()->value(settingsKey("ui/chordPicker/expanded")).toBool();
    m_advanced = muse::settings()->value(settingsKey("ui/chordPicker/advanced")).toBool();
    m_preferSharps = muse::settings()->value(settingsKey("ui/chordPicker/preferSharps")).toBool();
    m_literal = muse::settings()->value(settingsKey("ui/chordPicker/literal")).toBool();
    m_inversion = muse::settings()->value(settingsKey("ui/chordPicker/inversion")).toInt();
    if (m_inversion < 0) {
        m_inversion = 0;
    }
}

void ChordPickerModel::writeBoolSetting(const char* key, bool value)
{
    muse::settings()->setSharedValue(settingsKey(key), muse::Val(value));
}

void ChordPickerModel::writeIntSetting(const char* key, int value)
{
    muse::settings()->setSharedValue(settingsKey(key), muse::Val(value));
}

QStringList ChordPickerModel::pitchNames() const
{
    static const int order[] = { 9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    QStringList names;
    names.reserve(12);
    for (int pitchClass : order) {
        names.append(spellPitchClass(pitchClass));
    }
    return names;
}

QVariantList ChordPickerModel::qualityGroups() const
{
    QVariantList groups;
    for (int group = 0; group < 5; ++group) {
        QVariantList flat;
        for (const QualityDef& quality : QUALITIES) {
            if (quality.group != group) {
                continue;
            }
            if (quality.advanced && !m_advanced) {
                continue;
            }
            flat.append(QVariantMap {
                { QStringLiteral("suffix"), QString::fromLatin1(quality.suffix) },
                { QStringLiteral("label"), QString::fromLatin1(quality.label) },
            });
        }

        QVariantList columns;
        QVariantList column;
        for (const QVariant& item : flat) {
            column.append(item);
            if (column.size() == 3) {
                columns.append(QVariant(column));
                column.clear();
            }
        }
        if (!column.isEmpty()) {
            columns.append(QVariant(column));
        }

        groups.append(QVariantMap {
            { QStringLiteral("key"), QString::fromLatin1(GROUP_KEYS[group]) },
            { QStringLiteral("columns"), columns },
        });
    }
    return groups;
}

QString ChordPickerModel::root() const
{
    return m_root;
}

QString ChordPickerModel::quality() const
{
    return m_quality;
}

QString ChordPickerModel::bass() const
{
    return m_bass;
}

QString ChordPickerModel::preview() const
{
    if (!m_customSymbol.isEmpty()) {
        return m_customSymbol;
    }
    return buildSymbol();
}

bool ChordPickerModel::hasCustomSymbol() const
{
    return !m_customSymbol.isEmpty();
}

bool ChordPickerModel::advanced() const
{
    return m_advanced;
}

bool ChordPickerModel::preferSharps() const
{
    return m_preferSharps;
}

bool ChordPickerModel::literal() const
{
    return m_literal;
}

int ChordPickerModel::inversion() const
{
    const int count = rightHandCount();
    if (count <= 1) {
        return 0;
    }
    return (m_inversion % count + count) % count;
}

bool ChordPickerModel::expanded() const
{
    return m_expanded;
}

QString ChordPickerModel::noteNames() const
{
    return m_noteNames;
}

QString ChordPickerModel::formula() const
{
    return m_formula;
}

QVariantList ChordPickerModel::soundingNotes() const
{
    QVariantList notes;
    notes.reserve(int(m_sounding.size()));
    for (const SoundingNote& note : m_sounding) {
        notes.append(QVariantMap {
            { QStringLiteral("pitch"), note.pitch },
            { QStringLiteral("bass"), note.bass },
            { QStringLiteral("name"), note.name },
        });
    }
    return notes;
}

int ChordPickerModel::pianoLow() const
{
    return m_pianoLow;
}

int ChordPickerModel::pianoHigh() const
{
    return m_pianoHigh;
}

ChordPickerModel::~ChordPickerModel()
{
    if (s_active == this) {
        s_active = nullptr;
    }
}

void ChordPickerModel::init()
{
    AbstractElementPopupModel::init();
    s_active = this;
    m_userAdjusted = false;
    readFromItem();
}

bool ChordPickerModel::applyActive()
{
    ChordPickerModel* model = s_active;
    if (!model || !model->m_expanded || model->preview().isEmpty()) {
        return false;
    }
    if (!model->commitSymbol()) {
        return false;
    }

    INotationInteractionPtr notationInteraction = model->interaction();
    if (!notationInteraction) {
        return true;
    }

    Harmony* harmony = model->harmonyItem();
    const bool editingThis = harmony
                             && notationInteraction->isTextEditingStarted()
                             && notationInteraction->editedText() == harmony;
    if (!editingThis && harmony) {
        notationInteraction->startEditText(harmony);
    }

    // Moves to the next chord and can destroy this model. Do not touch model after this.
    notationInteraction->navigateToNearHarmony(MoveDirection::Right, true);
    return true;
}

void ChordPickerModel::setRoot(const QString& root)
{
    const int pitchClass = pitchClassForRoot(root);
    if (pitchClass < 0) {
        return;
    }

    const QString spelled = spellPitchClass(pitchClass);
    const bool bassCleared = !m_bass.isEmpty() && pitchClassForRoot(m_bass) == pitchClass;
    if (m_root != spelled || !m_customSymbol.isEmpty() || bassCleared) {
        const bool customCleared = !m_customSymbol.isEmpty();
        m_root = spelled;
        m_customSymbol.clear();
        if (bassCleared) {
            m_bass.clear();
        }
        m_userAdjusted = true;
        emit rootChanged();
        if (bassCleared) {
            emit bassChanged();
        }
        if (customCleared) {
            emit hasCustomSymbolChanged();
        }
        emit previewChanged();
        refreshRealization();
    }

    playPreview();
}

void ChordPickerModel::setQuality(const QString& quality)
{
    if (m_quality != quality || !m_customSymbol.isEmpty()) {
        m_quality = quality;
        m_customSymbol.clear();
        m_userAdjusted = true;
        emit qualityChanged();
        emit hasCustomSymbolChanged();
        emit previewChanged();
        refreshRealization();
    }

    playPreview();
}

void ChordPickerModel::setBass(const QString& bass)
{
    QString next = bass;
    const int nextPitchClass = pitchClassForRoot(next);
    const int rootPitchClass = pitchClassForRoot(m_root);
    if (nextPitchClass >= 0 && nextPitchClass == rootPitchClass) {
        next.clear();
    } else if (!next.isEmpty() && next == m_bass) {
        next.clear();
    } else if (nextPitchClass >= 0) {
        next = spellPitchClass(nextPitchClass);
    }

    if (m_bass != next || !m_customSymbol.isEmpty()) {
        const bool customCleared = !m_customSymbol.isEmpty();
        m_bass = next;
        m_customSymbol.clear();
        m_userAdjusted = true;
        emit bassChanged();
        if (customCleared) {
            emit hasCustomSymbolChanged();
        }
        emit previewChanged();
        refreshRealization();
    }

    playPreview();
}

void ChordPickerModel::setAdvanced(bool advanced)
{
    if (m_advanced == advanced) {
        return;
    }
    m_advanced = advanced;
    writeBoolSetting("ui/chordPicker/advanced", m_advanced);
    emit advancedChanged();
    emit qualityGroupsChanged();
    rebuildVoicing();
}

void ChordPickerModel::setPreferSharps(bool preferSharps)
{
    if (m_preferSharps == preferSharps) {
        return;
    }
    m_preferSharps = preferSharps;
    writeBoolSetting("ui/chordPicker/preferSharps", m_preferSharps);

    const int rootPitchClass = pitchClassForRoot(m_root);
    if (rootPitchClass >= 0) {
        m_root = spellPitchClass(rootPitchClass);
    }
    if (!m_bass.isEmpty()) {
        const int bassPitchClass = pitchClassForRoot(m_bass);
        if (bassPitchClass >= 0) {
            m_bass = spellPitchClass(bassPitchClass);
        }
    }

    emit preferSharpsChanged();
    emit pitchNamesChanged();
    emit rootChanged();
    emit bassChanged();
    emit previewChanged();
    refreshRealization();
}

void ChordPickerModel::setLiteral(bool literal)
{
    if (m_literal == literal) {
        return;
    }
    m_literal = literal;
    writeBoolSetting("ui/chordPicker/literal", m_literal);
    emit literalChanged();
    refreshRealization();
    storePlaybackVoicingIfCurrent();
    playPreview();
}

void ChordPickerModel::setExpanded(bool expanded)
{
    if (m_expanded == expanded) {
        return;
    }
    m_expanded = expanded;
    writeBoolSetting("ui/chordPicker/expanded", m_expanded);
    emit expandedChanged();
}

void ChordPickerModel::cycleInversion()
{
    const int count = rightHandCount();
    if (count <= 1) {
        playPreview();
        return;
    }
    m_inversion = (m_inversion + 1) % count;
    writeIntSetting("ui/chordPicker/inversion", m_inversion);
    emit inversionChanged();
    rebuildVoicing();
    storePlaybackVoicingIfCurrent();
    playPreview();
}

void ChordPickerModel::playPreview()
{
    Harmony* harmony = harmonyItem();
    if (!harmony || m_sounding.empty() || !globalContext()) {
        return;
    }

    IMasterNotationPtr master = globalContext()->currentMasterNotation();
    if (!master || !master->playback()) {
        return;
    }

    Segment* segment = harmony->getParentSeg();
    if (!segment) {
        return;
    }

    Chord* chord = Factory::createChord(segment);
    chord->setScore(harmony->score());
    chord->setTrack(harmony->track());

    std::vector<Note*> notes;
    notes.reserve(m_sounding.size());
    for (const SoundingNote& sounding : m_sounding) {
        Note* note = Factory::createNote(chord);
        note->setScore(harmony->score());
        note->setTrack(harmony->track());
        note->setStaffIdx(harmony->staffIdx());

        NoteVal value;
        value.pitch = sounding.pitch;
        value.tpc1 = sounding.tpc;
        note->setNval(value);
        notes.push_back(note);
    }

    if (!notes.empty()) {
        std::vector<const EngravingItem*> items;
        items.reserve(notes.size());
        for (Note* note : notes) {
            items.push_back(note);
        }
        master->playback()->triggerEventsForItems(items, PREVIEW_DURATION_US, true);
    }

    delete chord;
    muse::DeleteAll(notes);
}

bool ChordPickerModel::commitSymbol()
{
    Harmony* harmony = harmonyItem();
    if (!harmony) {
        return false;
    }

    const QString symbol = preview();
    if (symbol.isEmpty()) {
        return false;
    }

    const String chordText = muse::String::fromQString(symbol);
    INotationInteractionPtr notationInteraction = interaction();
    const bool editingThis = notationInteraction
                             && notationInteraction->isTextEditingStarted()
                             && notationInteraction->editedText() == harmony;
    TextCursor* cursor = editingThis ? harmony->cursor() : nullptr;
    if (editingThis && !cursor) {
        return false;
    }

    beginCommand(muse::TranslatableString("undoableAction", "Set chord symbol"));
    if (editingThis) {
        cursor->movePosition(TextCursor::MoveOperation::Start, TextCursor::MoveMode::MoveAnchor);
        cursor->movePosition(TextCursor::MoveOperation::End, TextCursor::MoveMode::KeepAnchor);
        if (cursor->hasSelection()) {
            harmony->deleteSelectedText();
        }
        harmony->insertText(chordText);
    } else {
        harmony->undoChangeProperty(Pid::TEXT, PropertyValue(chordText), harmony->propertyFlags(Pid::TEXT));
    }
    storePlaybackVoicing();
    endCommand();
    if (!editingThis) {
        updateNotation();
    }
    return true;
}

void ChordPickerModel::storePlaybackVoicing()
{
    Harmony* harmony = harmonyItem();
    if (!harmony) {
        return;
    }

    const auto unstyled = [harmony](Pid id) {
        PropertyFlags flags = harmony->propertyFlags(id);
        if (flags == PropertyFlags::STYLED) {
            flags = PropertyFlags::UNSTYLED;
        }
        return flags;
    };

    harmony->undoChangeProperty(Pid::HARMONY_VOICE_LITERAL, PropertyValue(m_literal), unstyled(Pid::HARMONY_VOICE_LITERAL));
    harmony->undoChangeProperty(Pid::HARMONY_VOICING, PropertyValue(int(Voicing::KEYBOARD)), unstyled(Pid::HARMONY_VOICING));
    harmony->undoChangeProperty(Pid::HARMONY_INVERSION, PropertyValue(inversion()), PropertyFlags::NOSTYLE);
}

void ChordPickerModel::storePlaybackVoicingIfCurrent()
{
    if (!previewMatchesItem()) {
        return;
    }

    beginCommand(muse::TranslatableString("undoableAction", "Set chord symbol voicing"));
    storePlaybackVoicing();
    endCommand();
    updateNotation();
}

bool ChordPickerModel::previewMatchesItem() const
{
    const Harmony* harmony = harmonyItem();
    if (!harmony) {
        return false;
    }
    const QString symbol = preview();
    if (symbol.isEmpty()) {
        return false;
    }

    const auto same = [&symbol](QString text) {
        text = normalizeAccidentals(text);
        if (text.startsWith(u'(') && text.endsWith(u')') && text.size() > 2) {
            text = text.mid(1, text.size() - 2).trimmed();
        }
        return text == symbol;
    };
    return same(harmony->plainText().toQString()) || same(harmony->harmonyName().toQString());
}

void ChordPickerModel::syncVoicingFromItem()
{
    Harmony* harmony = harmonyItem();
    const bool stored = harmony && harmony->realizedHarmony().voicing() == Voicing::KEYBOARD;
    if (stored) {
        const int storedInversion = std::max(0, harmony->realizedHarmony().inversion());
        const bool storedLiteral = harmony->realizedHarmony().literal();
        if (m_inversion != storedInversion) {
            m_inversion = storedInversion;
            emit inversionChanged();
        }
        if (m_literal != storedLiteral) {
            m_literal = storedLiteral;
            emit literalChanged();
        }
        m_usingStoredVoicing = true;
        return;
    }

    if (!m_usingStoredVoicing) {
        return;
    }

    m_usingStoredVoicing = false;
    const int preferredInversion = std::max(0, muse::settings()->value(settingsKey("ui/chordPicker/inversion")).toInt());
    const bool preferredLiteral = muse::settings()->value(settingsKey("ui/chordPicker/literal")).toBool();
    if (m_inversion != preferredInversion) {
        m_inversion = preferredInversion;
        emit inversionChanged();
    }
    if (m_literal != preferredLiteral) {
        m_literal = preferredLiteral;
        emit literalChanged();
    }
}

void ChordPickerModel::apply()
{
    if (!commitSymbol()) {
        return;
    }

    setExpanded(false);

    INotationInteractionPtr notationInteraction = interaction();
    Harmony* harmony = harmonyItem();
    const bool editingThis = notationInteraction && harmony
                             && notationInteraction->isTextEditingStarted()
                             && notationInteraction->editedText() == harmony;
    if (editingThis) {
        // Ending text edit can close this popup and destroy the model.
        notationInteraction->endEditText();
    }
}

Harmony* ChordPickerModel::harmonyItem() const
{
    if (!m_item || !m_item->isHarmony()) {
        return nullptr;
    }
    return toHarmony(m_item);
}

void ChordPickerModel::readFromItem()
{
    QString root = QStringLiteral("C");
    QString quality;
    QString bass;
    QString custom;

    if (const Harmony* harmony = harmonyItem()) {
        QString text = normalizeAccidentals(harmony->plainText().toQString());
        if (text.isEmpty()) {
            text = normalizeAccidentals(harmony->harmonyName().toQString());
        }
        if (text.startsWith(u'(') && text.endsWith(u')') && text.size() > 2) {
            text = text.mid(1, text.size() - 2).trimmed();
        }

        if (!text.isEmpty() && !parseSymbol(text, root, quality, bass)) {
            custom = text;
            root = QStringLiteral("C");
            quality.clear();
            bass.clear();
        }
    }

    syncVoicingFromItem();
    assignSelection(root, quality, bass, custom);
    refreshRealization();
}

void ChordPickerModel::assignSelection(const QString& root, const QString& quality, const QString& bass, const QString& customSymbol)
{
    QString spelledRoot = root;
    const int rootPitchClass = pitchClassForRoot(root);
    if (rootPitchClass >= 0) {
        spelledRoot = spellPitchClass(rootPitchClass);
    }

    QString spelledBass = bass;
    if (!bass.isEmpty()) {
        const int bassPitchClass = pitchClassForRoot(bass);
        if (bassPitchClass >= 0) {
            spelledBass = bassPitchClass == rootPitchClass ? QString() : spellPitchClass(bassPitchClass);
        }
    }

    const bool rootDiffers = m_root != spelledRoot;
    const bool qualityDiffers = m_quality != quality;
    const bool bassDiffers = m_bass != spelledBass;
    const bool customDiffers = m_customSymbol != customSymbol;

    m_root = spelledRoot;
    m_quality = quality;
    m_bass = spelledBass;
    m_customSymbol = customSymbol;

    if (rootDiffers) {
        emit rootChanged();
    }
    if (qualityDiffers) {
        emit qualityChanged();
    }
    if (bassDiffers) {
        emit bassChanged();
    }
    if (customDiffers) {
        emit hasCustomSymbolChanged();
    }
    if (rootDiffers || qualityDiffers || bassDiffers || customDiffers) {
        emit previewChanged();
    }
}

bool ChordPickerModel::parseSymbol(const QString& text, QString& root, QString& quality, QString& bass) const
{
    if (text.contains(u'|')) {
        return false;
    }

    QString body = text;
    const int slash = body.lastIndexOf(u'/');
    if (slash > 0) {
        const QString bassText = body.mid(slash + 1).trimmed();
        body = body.left(slash).trimmed();
        if (bassText.isEmpty()) {
            return false;
        }
        bass = matchRoot(bassText);
        if (bass.isEmpty() || bass.size() != bassText.size()) {
            bass = bassText;
        }
    } else {
        bass.clear();
    }

    const QString matchedRoot = matchRoot(body);
    if (matchedRoot.isEmpty()) {
        return false;
    }

    root = matchedRoot;
    QString qualityText = body.mid(matchedRoot.size());
    if (qualityText.startsWith(u'(') && qualityText.endsWith(u')') && qualityText.size() > 2) {
        qualityText = qualityText.mid(1, qualityText.size() - 2);
    }
    quality = canonicalQuality(qualityText);
    return true;
}

QString ChordPickerModel::matchRoot(const QString& text) const
{
    QString best;
    for (const QString& root : ROOTS) {
        if (text.startsWith(root, Qt::CaseInsensitive) && root.size() > best.size()) {
            best = root;
        }
    }

    if (best.size() == 1 && text.size() > 1) {
        const QChar accidental = text.at(1);
        if (accidental == u'#' || accidental == u'b') {
            const QString extended = text.left(2);
            bool listed = false;
            for (const QString& knownRoot : ROOTS) {
                if (knownRoot.compare(extended, Qt::CaseInsensitive) == 0) {
                    listed = true;
                    break;
                }
            }
            if (!listed) {
                return text.left(1).toUpper() + text.mid(1, 1);
            }
        }
    }

    return best;
}

QString ChordPickerModel::canonicalQuality(const QString& quality) const
{
    const QString normalized = normalizeAccidentals(quality);
    if (normalized.isEmpty()) {
        return QString();
    }

    for (const QualityDef& known : QUALITIES) {
        const QString suffix = QString::fromLatin1(known.suffix);
        if (suffix.isEmpty()) {
            continue;
        }
        if (normalized.compare(suffix, Qt::CaseSensitive) == 0) {
            return suffix;
        }
        if (normalized.size() > 1 && normalized.compare(suffix, Qt::CaseInsensitive) == 0) {
            return suffix;
        }
    }

    static const QHash<QString, QString> aliases = {
        { QStringLiteral("maj"), QString() },
        { QStringLiteral("Maj"), QString() },
        { QStringLiteral("ma"), QString() },
        { QStringLiteral("Ma"), QString() },
        { QStringLiteral("major"), QString() },
        { QStringLiteral("Major"), QString() },
        { QStringLiteral("M"), QString() },
        { QStringLiteral("min"), QStringLiteral("m") },
        { QStringLiteral("mi"), QStringLiteral("m") },
        { QStringLiteral("-"), QStringLiteral("m") },
        { QStringLiteral("min7"), QStringLiteral("m7") },
        { QStringLiteral("mi7"), QStringLiteral("m7") },
        { QStringLiteral("-7"), QStringLiteral("m7") },
        { QStringLiteral("ma7"), QStringLiteral("maj7") },
        { QStringLiteral("Ma7"), QStringLiteral("maj7") },
        { QStringLiteral("M7"), QStringLiteral("maj7") },
        { QStringLiteral("o"), QStringLiteral("dim") },
        { QStringLiteral("°"), QStringLiteral("dim") },
        { QStringLiteral("o7"), QStringLiteral("dim7") },
        { QStringLiteral("°7"), QStringLiteral("dim7") },
        { QStringLiteral("ø"), QStringLiteral("m7b5") },
        { QStringLiteral("ø7"), QStringLiteral("m7b5") },
        { QStringLiteral("+"), QStringLiteral("aug") },
        { QStringLiteral("sus"), QStringLiteral("sus4") },
        { QStringLiteral("7sus"), QStringLiteral("7sus4") },
        { QStringLiteral("m7(b9)"), QStringLiteral("m7b9") },
        { QStringLiteral("7+"), QStringLiteral("aug7") },
        { QStringLiteral("+7"), QStringLiteral("aug7") },
        { QStringLiteral("6/9"), QStringLiteral("69") },
        { QStringLiteral("m6/9"), QStringLiteral("m69") },
        { QStringLiteral("m(maj7)"), QStringLiteral("mMaj7") },
        { QStringLiteral("m(Maj7)"), QStringLiteral("mMaj7") },
        { QStringLiteral("aug(maj7)"), QStringLiteral("maj7#5") },
        { QStringLiteral("maj7+5"), QStringLiteral("maj7#5") },
        { QStringLiteral("+maj7"), QStringLiteral("maj7#5") },
        { QStringLiteral("9sus"), QStringLiteral("9sus4") },
        { QStringLiteral("13sus"), QStringLiteral("13sus4") },
        { QStringLiteral("m7(b5)"), QStringLiteral("m7b5") },
        { QStringLiteral("m7b5"), QStringLiteral("m7b5") },
    };

    const auto alias = aliases.constFind(normalized);
    if (alias != aliases.constEnd()) {
        return alias.value();
    }

    return normalized;
}

QString ChordPickerModel::buildSymbol() const
{
    QString symbol = m_root + m_quality;
    if (!m_bass.isEmpty()) {
        symbol += QStringLiteral("/") + m_bass;
    }
    return symbol;
}

QString ChordPickerModel::spellPitchClass(int pitchClass) const
{
    const int index = normalizePitchClass(pitchClass);
    return QString::fromLatin1(m_preferSharps ? SHARP_ROOTS[index] : FLAT_ROOTS[index]);
}

void ChordPickerModel::refreshRealization()
{
    m_tones.clear();
    m_rootPc = 0;
    m_rootTpc = Tpc::TPC_C;
    m_bassPc = 0;
    m_bassTpc = Tpc::TPC_INVALID;

    Harmony* harmony = harmonyItem();
    const QString symbol = preview();
    // Realized on a temporary harmony so typing into the symbol does not rewrite it.
    Harmony* probe = nullptr;
    EngravingObject* const owner = harmony ? harmony->ownershipParent() : nullptr;
    if (harmony && !symbol.isEmpty() && owner && owner->isEngravingItem()) {
        probe = Factory::createHarmony(toEngravingItem(owner));
        probe->setTrack(harmony->track());
        probe->setHarmonyType(harmony->harmonyType());
        probe->setHarmony(muse::String::fromQString(symbol));
        if (probe->isRealizable() && probe->parsedForm()) {
            const int rootTpc = probe->rootTpc();
            const int bassTpc = probe->bassTpc();
            const std::vector<RealizedHarmony::KeyboardTone> tones = probe->realizedHarmony().keyboardTones(rootTpc, m_literal);

            if (tpcIsValid(rootTpc)) {
                m_rootTpc = rootTpc;
                m_rootPc = normalizePitchClass(tpc2pitch(rootTpc));
            }

            if (tpcIsValid(bassTpc)) {
                m_bassTpc = bassTpc;
                m_bassPc = normalizePitchClass(tpc2pitch(bassTpc));
            } else {
                m_bassTpc = m_rootTpc;
                m_bassPc = m_rootPc;
            }

            m_tones.reserve(tones.size());
            for (const RealizedHarmony::KeyboardTone& tone : tones) {
                ChordTone chordTone;
                chordTone.pc = tone.pitchClass;
                chordTone.tpc = tone.tpc;
                chordTone.interval = tone.interval;
                m_tones.push_back(chordTone);
            }
        }
        probe->setOwnershipParent(nullptr);
        delete probe;
    }

    bool hasSeventh = false;
    bool hasThird = false;
    bool hasFifth = false;
    for (const ChordTone& tone : m_tones) {
        if (tone.interval == 10 || tone.interval == 11) {
            hasSeventh = true;
        } else if (tone.interval == 3 || tone.interval == 4) {
            hasThird = true;
        } else if (tone.interval == 7) {
            hasFifth = true;
        }
    }

    QStringList names;
    QStringList degrees;
    for (const ChordTone& tone : m_tones) {
        int tpc = tone.tpc;
        if (!tpcIsValid(tpc)) {
            tpc = pitch2tpc(RIGHT_HAND_PITCH + tone.pc, Key::C, m_preferSharps ? Prefer::SHARPS : Prefer::FLATS);
        }
        names.append(tpc2name(tpc, NoteSpellingType::STANDARD, NoteCaseType::CAPITAL).toQString());
        degrees.append(degreeName(tone.interval, hasSeventh, hasThird, hasFifth));
    }

    const QString noteNames = names.join(QLatin1Char(' '));
    const QString formula = degrees.join(QLatin1Char(' '));
    if (m_noteNames != noteNames) {
        m_noteNames = noteNames;
        emit noteNamesChanged();
    }
    if (m_formula != formula) {
        m_formula = formula;
        emit formulaChanged();
    }

    rebuildVoicing();
}

int ChordPickerModel::rightHandCount() const
{
    return int(m_tones.size());
}

void ChordPickerModel::rebuildVoicing()
{
    m_sounding.clear();

    const auto spell = [this](int pitch, int tpc) {
        if (!tpcIsValid(tpc)) {
            tpc = pitch2tpc(pitch, Key::C, m_preferSharps ? Prefer::SHARPS : Prefer::FLATS);
        }
        return tpc2name(tpc, NoteSpellingType::STANDARD, NoteCaseType::CAPITAL).toQString();
    };

    std::vector<RealizedHarmony::KeyboardTone> rightHand;
    rightHand.reserve(m_tones.size());
    for (const ChordTone& tone : m_tones) {
        rightHand.push_back(RealizedHarmony::KeyboardTone { tone.pc, tone.tpc, tone.interval });
    }

    const bool addBass = tpcIsValid(m_bassTpc) || !rightHand.empty();
    const int bassPitchClass = tpcIsValid(m_bassTpc) ? m_bassPc : m_rootPc;
    const int bassTpc = addBass ? (tpcIsValid(m_bassTpc) ? m_bassTpc : m_rootTpc) : Tpc::TPC_INVALID;
    const RealizedHarmony::PitchMap pitches = RealizedHarmony::voiceKeyboard(rightHand, bassPitchClass, bassTpc, inversion(), 0);

    bool first = true;
    for (const auto& pitch : pitches) {
        int tpc = pitch.second;
        if (!tpcIsValid(tpc)) {
            tpc = pitch2tpc(pitch.first, Key::C, m_preferSharps ? Prefer::SHARPS : Prefer::FLATS);
        }
        m_sounding.push_back(SoundingNote { pitch.first, tpc, first && addBass, spell(pitch.first, tpc) });
        first = false;
    }

    const int span = m_advanced ? ADVANCED_PIANO_SPAN : SIMPLE_PIANO_SPAN;
    int low = DEFAULT_PIANO_LOW;
    int high = low + span;
    if (!m_sounding.empty()) {
        int noteLow = m_sounding.front().pitch;
        int noteHigh = m_sounding.front().pitch;
        for (const SoundingNote& note : m_sounding) {
            noteLow = std::min(noteLow, note.pitch);
            noteHigh = std::max(noteHigh, note.pitch);
        }
        int start = (noteLow / 12) * 12;
        if (start + span < noteHigh) {
            const int shifted = ((noteHigh - span + 11) / 12) * 12;
            if (shifted <= noteLow) {
                start = shifted;
            }
        }
        low = start;
        high = start + span;
    }

    const bool rangeChanged = m_pianoLow != low || m_pianoHigh != high;
    m_pianoLow = low;
    m_pianoHigh = high;

    emit soundingNotesChanged();
    emit inversionChanged();
    if (rangeChanged) {
        emit pianoRangeChanged();
    }
}
