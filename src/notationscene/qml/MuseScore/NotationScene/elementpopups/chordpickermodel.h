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

#pragma once

#include <qqmlintegration.h>

#include <vector>

#include <QObject>
#include <QStringList>
#include <QVariantList>

#include "../abstractelementpopupmodel.h"

namespace mu::engraving {
class Harmony;
}

namespace mu::notation {
class ChordPickerModel : public AbstractElementPopupModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QStringList pitchNames READ pitchNames NOTIFY pitchNamesChanged)
    Q_PROPERTY(QVariantList qualityGroups READ qualityGroups NOTIFY qualityGroupsChanged)
    Q_PROPERTY(QString root READ root NOTIFY rootChanged)
    Q_PROPERTY(QString quality READ quality NOTIFY qualityChanged)
    Q_PROPERTY(QString bass READ bass NOTIFY bassChanged)
    Q_PROPERTY(QString preview READ preview NOTIFY previewChanged)
    Q_PROPERTY(bool hasCustomSymbol READ hasCustomSymbol NOTIFY hasCustomSymbolChanged)
    Q_PROPERTY(bool advanced READ advanced WRITE setAdvanced NOTIFY advancedChanged)
    Q_PROPERTY(bool preferSharps READ preferSharps WRITE setPreferSharps NOTIFY preferSharpsChanged)
    Q_PROPERTY(bool literal READ literal WRITE setLiteral NOTIFY literalChanged)
    Q_PROPERTY(int inversion READ inversion NOTIFY inversionChanged)
    Q_PROPERTY(bool expanded READ expanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(QString noteNames READ noteNames NOTIFY noteNamesChanged)
    Q_PROPERTY(QString formula READ formula NOTIFY formulaChanged)
    Q_PROPERTY(QVariantList soundingNotes READ soundingNotes NOTIFY soundingNotesChanged)
    Q_PROPERTY(int pianoLow READ pianoLow NOTIFY pianoRangeChanged)
    Q_PROPERTY(int pianoHigh READ pianoHigh NOTIFY pianoRangeChanged)

public:
    explicit ChordPickerModel(QObject* parent = nullptr);
    ~ChordPickerModel() override;

    QStringList pitchNames() const;
    QVariantList qualityGroups() const;

    QString root() const;
    QString quality() const;
    QString bass() const;
    QString preview() const;
    bool hasCustomSymbol() const;
    bool advanced() const;
    bool preferSharps() const;
    bool literal() const;
    int inversion() const;
    bool expanded() const;
    QString noteNames() const;
    QString formula() const;
    QVariantList soundingNotes() const;
    int pianoLow() const;
    int pianoHigh() const;

    Q_INVOKABLE void init() override;
    Q_INVOKABLE void setRoot(const QString& root);
    Q_INVOKABLE void setQuality(const QString& quality);
    Q_INVOKABLE void setBass(const QString& bass);
    Q_INVOKABLE void setAdvanced(bool advanced);
    Q_INVOKABLE void setPreferSharps(bool preferSharps);
    Q_INVOKABLE void setLiteral(bool literal);
    Q_INVOKABLE void setExpanded(bool expanded);
    Q_INVOKABLE void cycleInversion();
    Q_INVOKABLE void apply();

    // Writes the symbol and moves to the next chord. Returns false when Enter
    // should keep its normal text-editing behavior.
    static bool applyActive();

signals:
    void pitchNamesChanged();
    void qualityGroupsChanged();
    void rootChanged();
    void qualityChanged();
    void bassChanged();
    void previewChanged();
    void hasCustomSymbolChanged();
    void advancedChanged();
    void preferSharpsChanged();
    void literalChanged();
    void inversionChanged();
    void expandedChanged();
    void noteNamesChanged();
    void formulaChanged();
    void soundingNotesChanged();
    void pianoRangeChanged();

private:
    struct ChordTone {
        int pc = 0;
        int tpc = 0;
        int interval = 0;
    };

    struct SoundingNote {
        int pitch = 0;
        int tpc = 0;
        bool bass = false;
        QString name;
    };

    bool ignoreTextEditingChanges() const override { return false; }

    void loadSettings();
    void writeBoolSetting(const char* key, bool value);
    void writeIntSetting(const char* key, int value);

    mu::engraving::Harmony* harmonyItem() const;
    void readFromItem();
    void assignSelection(const QString& root, const QString& quality, const QString& bass, const QString& customSymbol);
    bool parseSymbol(const QString& text, QString& root, QString& quality, QString& bass) const;
    QString matchRoot(const QString& text) const;
    QString canonicalQuality(const QString& quality) const;
    QString buildSymbol() const;
    QString spellPitchClass(int pitchClass) const;
    void refreshRealization();
    void rebuildVoicing();
    int rightHandCount() const;
    void playPreview();
    bool commitSymbol();
    void storePlaybackVoicing();
    void storePlaybackVoicingIfCurrent();
    bool previewMatchesItem() const;
    void syncVoicingFromItem();

    static ChordPickerModel* s_active;

    std::vector<ChordTone> m_tones;
    std::vector<SoundingNote> m_sounding;
    int m_rootPc = 0;
    int m_rootTpc = 0;
    int m_bassPc = 0;
    int m_bassTpc = 0;

    QString m_root = QStringLiteral("C");
    QString m_quality;
    QString m_bass;
    QString m_customSymbol;
    QString m_noteNames;
    QString m_formula;
    bool m_userAdjusted = false;
    bool m_advanced = false;
    bool m_preferSharps = true;
    bool m_literal = true;
    bool m_expanded = true;
    int m_inversion = 0;
    bool m_usingStoredVoicing = false;
    int m_pianoLow = 48;
    int m_pianoHigh = 84;
};
}
