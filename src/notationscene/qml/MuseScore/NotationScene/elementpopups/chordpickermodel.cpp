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

#include <QHash>

#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/note.h"
#include "engraving/dom/noteval.h"
#include "engraving/dom/segment.h"

#include "notation/imasternotation.h"
#include "notation/inotationinteraction.h"
#include "notation/inotationplayback.h"

#include "log.h"

using namespace mu::notation;
using namespace mu::engraving;

namespace {
constexpr int PREVIEW_DURATION_US = 650000;
}

ChordPickerModel* ChordPickerModel::s_active = nullptr;

namespace {
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
};

const QualityDef QUALITIES[] = {
    { "",      "maj" },
    { "m",     "m" },
    { "dim",   "dim" },
    { "aug",   "aug" },
    { "sus2",  "sus2" },
    { "sus4",  "sus4" },
    { "7",     "7" },
    { "maj7",  "maj7" },
    { "m7",    "m7" },
    { "m7b5",  "m7♭5" },
    { "dim7",  "dim7" },
    { "mMaj7", "mMaj7" },
    { "6",     "6" },
    { "m6",    "m6" },
    { "9",     "9" },
    { "maj9",  "maj9" },
    { "m9",    "m9" },
    { "add9",  "add9" },
    { "7sus4", "7sus4" },
    { "11",    "11" },
    { "13",    "13" },
};

QString normalizeAccidentals(QString text)
{
    text.replace(QChar(0x266D), u'b');
    text.replace(QChar(0x266F), u'#');
    return text.trimmed();
}
}

ChordPickerModel::ChordPickerModel(QObject* parent)
    : AbstractElementPopupModel(PopupModelType::TYPE_HARMONY, parent)
{
    for (const QualityDef& quality : QUALITIES) {
        m_qualities.append(QVariantMap {
            { QStringLiteral("suffix"), QString::fromLatin1(quality.suffix) },
            { QStringLiteral("label"), QString::fromLatin1(quality.label) },
        });
    }

    connect(this, &AbstractElementPopupModel::dataChanged, this, [this]() {
        if (!m_userAdjusted) {
            readFromItem();
        }
    });
}

QStringList ChordPickerModel::roots() const
{
    return ROOTS;
}

QVariantList ChordPickerModel::qualities() const
{
    return m_qualities;
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

void ChordPickerModel::applyActive()
{
    if (s_active) {
        s_active->apply();
    }
}

void ChordPickerModel::setRoot(const QString& root)
{
    if (root.isEmpty()) {
        return;
    }

    if (m_root != root || !m_customSymbol.isEmpty()) {
        m_root = root;
        m_customSymbol.clear();
        m_userAdjusted = true;
        emit rootChanged();
        emit hasCustomSymbolChanged();
        emit previewChanged();
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
    }

    playPreview();
}

void ChordPickerModel::setBass(const QString& bass)
{
    if (m_bass != bass || !m_customSymbol.isEmpty()) {
        m_bass = bass;
        m_customSymbol.clear();
        m_userAdjusted = true;
        emit bassChanged();
        emit hasCustomSymbolChanged();
        emit previewChanged();
    }

    playPreview();
}

void ChordPickerModel::playPreview()
{
    Harmony* harmony = harmonyItem();
    if (!harmony || preview().isEmpty() || !globalContext()) {
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

    const String previous = harmony->harmonyName();
    harmony->setHarmony(muse::String::fromQString(preview()));
    if (!harmony->isRealizable()) {
        harmony->setHarmony(previous);
        return;
    }

    const RealizedHarmony::PitchMap pitches = harmony->getRealizedHarmony().notes();
    harmony->setHarmony(previous);

    Chord* chord = Factory::createChord(segment);
    chord->setScore(harmony->score());
    chord->setTrack(harmony->track());

    std::vector<Note*> notes;
    notes.reserve(pitches.size());
    for (const auto& pitch : pitches) {
        Note* note = Factory::createNote(chord);
        note->setScore(harmony->score());
        note->setTrack(harmony->track());
        note->setStaffIdx(harmony->staffIdx());

        NoteVal value;
        value.pitch = pitch.first;
        value.tpc1 = pitch.second;
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

void ChordPickerModel::apply()
{
    Harmony* harmony = harmonyItem();
    IF_ASSERT_FAILED(harmony) {
        return;
    }

    const QString symbol = preview();
    if (symbol.isEmpty()) {
        return;
    }

    const String chordText = muse::String::fromQString(symbol);
    INotationInteractionPtr notationInteraction = interaction();
    const bool editingThis = notationInteraction
                             && notationInteraction->isTextEditingStarted()
                             && notationInteraction->editedText() == harmony;

    if (editingThis) {
        TextCursor* cursor = harmony->cursor();
        IF_ASSERT_FAILED(cursor) {
            return;
        }

        cursor->movePosition(TextCursor::MoveOperation::Start, TextCursor::MoveMode::MoveAnchor);
        cursor->movePosition(TextCursor::MoveOperation::End, TextCursor::MoveMode::KeepAnchor);

        beginCommand(muse::TranslatableString("undoableAction", "Set chord symbol"));
        if (cursor->hasSelection()) {
            harmony->deleteSelectedText();
        }
        harmony->insertText(chordText);
        endCommand();

        // Ending text edit closes this popup and may destroy the model.
        notationInteraction->endEditText();
        return;
    }

    changeItemProperty(Pid::TEXT, chordText);
    emit chordApplied();
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

    assignSelection(root, quality, bass, custom);
}

void ChordPickerModel::assignSelection(const QString& root, const QString& quality, const QString& bass, const QString& customSymbol)
{
    const bool rootDiffers = m_root != root;
    const bool qualityDiffers = m_quality != quality;
    const bool bassDiffers = m_bass != bass;
    const bool customDiffers = m_customSymbol != customSymbol;

    m_root = root;
    m_quality = quality;
    m_bass = bass;
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
    if (text.contains(u'|') || text.contains(u'(') || text.contains(u')')) {
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
    quality = canonicalQuality(body.mid(matchedRoot.size()));
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
