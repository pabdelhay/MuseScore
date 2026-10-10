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
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents
import MuseScore.NotationScene

AbstractElementPopup {
    id: root

    property alias notationViewNavigationSection: chordPickerNavPanel.section
    property alias navigationOrderStart: chordPickerNavPanel.order
    readonly property alias navigationOrderEnd: chordPickerNavPanel.order

    property bool qualityOpen: false
    property bool bassOpen: false

    readonly property string bassLabel: pickerModel.bass.length === 0
                                         ? qsTrc("notation", "None")
                                         : pickerModel.bass
    readonly property string qualityChipText: pickerModel.qualityLabel + (qualityOpen ? " ▴" : " ▾")
    readonly property string bassChipText: bassLabel + (bassOpen ? " ▴" : " ▾")

    // Plain arrays, so the repeaters are not stuck on nested QVariantLists.
    readonly property var qualityTable: {
        const table = []
        const rows = pickerModel.qualityRows
        if (!rows) {
            return table
        }
        for (let r = 0; r < rows.length; ++r) {
            const items = []
            const row = rows[r]
            for (let i = 0; i < row.length; ++i) {
                items.push({
                    suffix: row[i].suffix,
                    label: row[i].label
                })
            }
            table.push(items)
        }
        return table
    }

    readonly property var bassNotes: {
        const notes = [""]
        const roots = pickerModel.roots
        if (!roots) {
            return notes
        }
        for (let i = 0; i < roots.length; ++i) {
            notes.push(roots[i])
        }
        return notes
    }

    contentWidth: content.implicitWidth
    contentHeight: content.implicitHeight

    margins: 3
    showArrow: false
    placementPolicies: PopupView.PreferAbove
    focusPolicies: PopupView.DefaultFocus & ~PopupView.ClickFocus

    function qualityRowTitle(row) {
        switch (row) {
        case 0: return qsTrc("notation", "Major")
        case 1: return qsTrc("notation", "Minor")
        case 2: return qsTrc("notation", "Sus")
        case 3: return qsTrc("notation", "Dim")
        }
        return ""
    }

    function updatePosition() {
        if (!parent) {
            return
        }

        const gap = 4
        const popupHeight = Math.max(30, root.contentHeight) + root.padding * 2 + root.margins * 2
        root.y = -popupHeight - gap
        root.popupPosition = PopupPosition.Top
        Qt.callLater(root.repositionWindowIfNeed)
    }

    onContentHeightChanged: updatePosition()
    onContentWidthChanged: updatePosition()

    model: ChordPickerModel {
        id: pickerModel

        onChordApplied: {
            root.close()
        }
    }

    NavigationPanel {
        id: chordPickerNavPanel
        name: "ChordPicker"
        direction: NavigationPanel.Horizontal
        accessible.name: qsTrc("notation", "Chord symbol picker")

        onNavigationEvent: function(event) {
            if (event.type === NavigationEvent.Escape) {
                root.close()
            }
        }
    }

    ColumnLayout {
        id: content
        spacing: 4

        ColumnLayout {
            visible: root.qualityOpen
            spacing: 2

            Repeater {
                model: root.qualityTable

                delegate: RowLayout {
                    id: qualityRow
                    required property var modelData
                    required property int index

                    spacing: 2

                    StyledTextLabel {
                        Layout.preferredWidth: 48
                        Layout.alignment: Qt.AlignVCenter
                        text: root.qualityRowTitle(qualityRow.index)
                        font: ui.theme.bodyFont
                        horizontalAlignment: Text.AlignRight
                        opacity: 0.7
                    }

                    Repeater {
                        model: qualityRow.modelData

                        delegate: FlatButton {
                            required property var modelData
                            required property int index

                            Layout.alignment: Qt.AlignVCenter
                            minWidth: 20
                            margins: 5
                            text: modelData.label
                            accentButton: !pickerModel.hasCustomSymbol && pickerModel.quality === modelData.suffix

                            navigation.panel: chordPickerNavPanel
                            navigation.order: 20 + qualityRow.index * 20 + index
                            accessible.name: modelData.label

                            onClicked: {
                                pickerModel.setQuality(modelData.suffix)
                                root.qualityOpen = false
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            visible: root.bassOpen
            spacing: 2

            Repeater {
                model: [root.bassNotes.slice(0, 9), root.bassNotes.slice(9)]

                delegate: RowLayout {
                    id: bassRow
                    required property var modelData
                    required property int index

                    spacing: 2

                    Repeater {
                        model: bassRow.modelData

                        delegate: FlatButton {
                            required property string modelData
                            required property int index

                            readonly property bool isNone: modelData.length === 0

                            Layout.alignment: Qt.AlignVCenter
                            minWidth: 28
                            margins: 5
                            text: isNone ? qsTrc("notation", "None") : modelData
                            accentButton: !pickerModel.hasCustomSymbol && pickerModel.bass === modelData

                            navigation.panel: chordPickerNavPanel
                            navigation.order: 100 + bassRow.index * 9 + index
                            accessible.name: isNone
                                             ? qsTrc("notation", "No bass note")
                                             : qsTrc("notation", "Bass %1").arg(modelData)

                            onClicked: {
                                pickerModel.setBass(modelData)
                                root.bassOpen = false
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            id: bar
            spacing: 2

            FlatButton {
                Layout.alignment: Qt.AlignVCenter
                transparent: true
                icon: IconCode.ARROW_DOWN

                navigation.panel: chordPickerNavPanel
                navigation.order: 0
                accessible.name: qsTrc("notation", "Lower root")
                toolTipTitle: qsTrc("notation", "Lower root a half step")

                onClicked: pickerModel.stepRoot(-1)
            }

            StyledTextLabel {
                Layout.preferredWidth: 32
                Layout.alignment: Qt.AlignVCenter
                text: pickerModel.root
                font: ui.theme.bodyBoldFont
                horizontalAlignment: Text.AlignHCenter
            }

            FlatButton {
                Layout.alignment: Qt.AlignVCenter
                transparent: true
                icon: IconCode.ARROW_UP

                navigation.panel: chordPickerNavPanel
                navigation.order: 1
                accessible.name: qsTrc("notation", "Raise root")
                toolTipTitle: qsTrc("notation", "Raise root a half step")

                onClicked: pickerModel.stepRoot(1)
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                color: ui.theme.strokeColor
            }

            FlatButton {
                Layout.alignment: Qt.AlignVCenter
                Layout.maximumWidth: 148
                minWidth: 44
                margins: 6
                text: root.qualityChipText
                accentButton: root.qualityOpen

                navigation.panel: chordPickerNavPanel
                navigation.order: 2
                accessible.name: qsTrc("notation", "Quality %1").arg(pickerModel.qualityLabel)
                toolTipTitle: qsTrc("notation", "Chord quality")

                onClicked: {
                    root.qualityOpen = !root.qualityOpen
                    if (root.qualityOpen) {
                        root.bassOpen = false
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                color: ui.theme.strokeColor
            }

            FlatButton {
                Layout.alignment: Qt.AlignVCenter
                minWidth: 52
                margins: 6
                text: root.bassChipText
                accentButton: root.bassOpen

                navigation.panel: chordPickerNavPanel
                navigation.order: 3
                accessible.name: qsTrc("notation", "Bass %1").arg(root.bassLabel)
                toolTipTitle: qsTrc("notation", "Bass note")

                onClicked: {
                    root.bassOpen = !root.bassOpen
                    if (root.bassOpen) {
                        root.qualityOpen = false
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                Layout.topMargin: 4
                Layout.bottomMargin: 4
                color: ui.theme.strokeColor
            }

            FlatButton {
                Layout.alignment: Qt.AlignVCenter
                icon: IconCode.TICK_RIGHT_ANGLE
                accentButton: true
                enabled: pickerModel.preview.length > 0

                navigation.panel: chordPickerNavPanel
                navigation.order: 4
                accessible.name: qsTrc("notation", "Insert")
                toolTipTitle: qsTrc("notation", "Insert chord symbol")

                onClicked: pickerModel.apply()
            }
        }
    }
}
