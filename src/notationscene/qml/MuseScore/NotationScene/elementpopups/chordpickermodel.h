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

    Q_PROPERTY(QStringList roots READ roots CONSTANT)
    Q_PROPERTY(QVariantList qualityRows READ qualityRows CONSTANT)
    Q_PROPERTY(QString root READ root NOTIFY rootChanged)
    Q_PROPERTY(QString quality READ quality NOTIFY qualityChanged)
    Q_PROPERTY(QString qualityLabel READ qualityLabel NOTIFY qualityLabelChanged)
    Q_PROPERTY(QString bass READ bass NOTIFY bassChanged)
    Q_PROPERTY(QString preview READ preview NOTIFY previewChanged)
    Q_PROPERTY(bool hasCustomSymbol READ hasCustomSymbol NOTIFY hasCustomSymbolChanged)

public:
    explicit ChordPickerModel(QObject* parent = nullptr);
    ~ChordPickerModel() override;

    QStringList roots() const;
    QVariantList qualityRows() const;

    QString root() const;
    QString quality() const;
    QString qualityLabel() const;
    QString bass() const;
    QString preview() const;
    bool hasCustomSymbol() const;

    Q_INVOKABLE void init() override;
    Q_INVOKABLE void stepRoot(int direction);
    Q_INVOKABLE void setRoot(const QString& root);
    Q_INVOKABLE void setQuality(const QString& quality);
    Q_INVOKABLE void setBass(const QString& bass);
    Q_INVOKABLE void apply();

    static void applyActive();

signals:
    void rootChanged();
    void qualityChanged();
    void qualityLabelChanged();
    void bassChanged();
    void previewChanged();
    void hasCustomSymbolChanged();
    void chordApplied();

private:
    bool ignoreTextEditingChanges() const override { return false; }

    mu::engraving::Harmony* harmonyItem() const;
    void readFromItem();
    void assignSelection(const QString& root, const QString& quality, const QString& bass, const QString& customSymbol);
    bool parseSymbol(const QString& text, QString& root, QString& quality, QString& bass) const;
    QString matchRoot(const QString& text) const;
    QString canonicalQuality(const QString& quality) const;
    QString buildSymbol() const;
    void playPreview();

    static ChordPickerModel* s_active;

    QVariantList m_qualityRows;
    QString m_root = QStringLiteral("C");
    QString m_quality;
    QString m_bass;
    QString m_customSymbol;
    bool m_userAdjusted = false;
};
}
