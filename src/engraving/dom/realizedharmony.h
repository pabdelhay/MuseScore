/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#pragma once

#include <map>
#include <vector>

#include "../types/fraction.h"

namespace mu::engraving {
class Harmony;

//voicing modes to use
enum class Voicing : signed char {
    INVALID = -1,
    AUTO = 0,
    ROOT_ONLY,
    CLOSE,
    DROP_2,
    SIX_NOTE,
    FOUR_NOTE,
    THREE_NOTE,
    // Close right-hand voicing with a bass note below, matching the chord picker.
    // Honors RealizedHarmony::inversion().
    KEYBOARD
};

//duration to realize notes for
enum class HDuration : signed char {
    INVALID = -1,
    UNTIL_NEXT_CHORD_SYMBOL = 0,    //lasts until the next chord symbol or end of the schore
    STOP_AT_MEASURE_END,            //lasts until next chord symbol or measure end
    SEGMENT_DURATION                //lasts for the duration of the segment
};

//-----------------------------------------
//    Realized Harmony
///     holds information and functions
///     to assist in the realization of chord
///     symbols. This is what is used to
///     allow for chord symbol playback
//-----------------------------------------
class RealizedHarmony
{
public:
    using PitchMap = std::multimap<int, int>;   //map from pitch to tpc

    RealizedHarmony()
        : m_harmony(0), m_notes(PitchMap()), m_dirty(1) {}
    RealizedHarmony(Harmony* h)
        : m_harmony(h), m_notes(PitchMap()), m_dirty(1) {}

    // One chord tone, ordered by interval from the root when several are voiced together.
    struct KeyboardTone {
        int pitchClass = 0;
        int tpc = 0;
        int interval = 0;
    };

    void setVoicing(Voicing);
    void setDuration(HDuration);
    void setLiteral(bool);
    void setInversion(int inversion);
    void setDirty(bool dirty) { cascadeDirty(dirty); }   //set dirty flag and cascade
    void setHarmony(Harmony* h) { m_harmony = h; }

    Voicing voicing() const { return m_voicing; }
    HDuration duration() const { return m_duration; }
    bool literal() const { return m_literal; }
    int inversion() const { return m_inversion; }
    Harmony* harmony() { return m_harmony; }

    // Pitch classes the chord picker shows, without the bass note and without
    // jazz approach tones taken from the following chord.
    std::vector<KeyboardTone> keyboardTones(int rootTpc, bool literal) const;

    // The voicing the chord picker previews: chord tones stacked in close
    // position from C4, rotated by inversion, with the bass around C3.
    static PitchMap voiceKeyboard(const std::vector<KeyboardTone>& tones, int bassPitchClass, int bassTpc, int inversion,
                                  int transposeOffset);

    bool valid() const { return !m_dirty && m_harmony; }

    const PitchMap& notes() const;
    const PitchMap generateNotes(int rootTpc, int bassTpc, bool literal, Voicing voicing, int transposeOffset, int inversion = 0) const;

    void update(int rootTpc, int bassTpc, int transposeOffset = 0);   //updates the notes map

    Fraction getActualDuration(int utick, HDuration durationType = HDuration::INVALID) const;

private:
    PitchMap getIntervals(int rootTpc, bool literal = true, bool followNext = true) const;
    PitchMap normalizeNoteMap(const PitchMap& intervals, int rootTpc, int rootPitch, size_t max = 128, bool enforceMaxAsGoal = false) const;
    void cascadeDirty(bool dirty);

    Harmony* m_harmony = nullptr;

    PitchMap m_notes;

    Voicing m_voicing = Voicing::AUTO;
    HDuration m_duration = HDuration::INVALID;
    int m_inversion = 0;

    //whether or not the current notes QMap is up to date
    bool m_dirty = false;
    bool m_literal = false;   //use all notes when possible and do not add any notes
};
}
