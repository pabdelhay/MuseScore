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

    readonly property int popupWidth: 468

    contentWidth: content.width
    contentHeight: content.height

    margins: 12
    showArrow: false
    placementPolicies: PopupView.PreferAbove
    focusPolicies: PopupView.DefaultFocus & ~PopupView.ClickFocus

    model: ChordPickerModel {
        id: pickerModel

        onChordApplied: {
            root.close()
        }
    }

    ColumnLayout {
        id: content

        width: root.popupWidth
        spacing: 10

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

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("notation", "Chord symbol")
            font: ui.theme.largeBodyBoldFont
            horizontalAlignment: Text.AlignLeft
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("notation", "Choosing a root, quality, or bass plays the chord. Press Enter to insert it.")
            font: ui.theme.bodyFont
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.WordWrap
            opacity: 0.75
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: ui.theme.backgroundSecondaryColor
            border.color: ui.theme.strokeColor
            border.width: 1
            radius: 4

            StyledTextLabel {
                anchors.centerIn: parent
                text: pickerModel.preview.length > 0 ? pickerModel.preview : "—"
                font: ui.theme.largeBodyBoldFont
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("notation", "Root")
            font: ui.theme.bodyBoldFont
            horizontalAlignment: Text.AlignLeft
        }

        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            width: root.popupWidth
            spacing: 4

            Repeater {
                model: pickerModel.roots

                delegate: FlatButton {
                    required property string modelData
                    required property int index

                    text: modelData
                    implicitWidth: Math.max(36, text.length * 9 + 16)
                    implicitHeight: 28
                    accentButton: !pickerModel.hasCustomSymbol && pickerModel.root === modelData

                    navigation.panel: chordPickerNavPanel
                    navigation.order: index
                    accessible.name: modelData

                    onClicked: pickerModel.setRoot(modelData)
                }
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("notation", "Quality")
            font: ui.theme.bodyBoldFont
            horizontalAlignment: Text.AlignLeft
        }

        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            width: root.popupWidth
            spacing: 4

            Repeater {
                model: pickerModel.qualities

                delegate: FlatButton {
                    required property var modelData
                    required property int index

                    text: modelData.label
                    implicitWidth: Math.max(36, text.length * 9 + 16)
                    implicitHeight: 28
                    accentButton: !pickerModel.hasCustomSymbol && pickerModel.quality === modelData.suffix

                    navigation.panel: chordPickerNavPanel
                    navigation.order: 100 + index
                    accessible.name: modelData.label

                    onClicked: pickerModel.setQuality(modelData.suffix)
                }
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("notation", "Bass")
            font: ui.theme.bodyBoldFont
            horizontalAlignment: Text.AlignLeft
        }

        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            width: root.popupWidth
            spacing: 4

            FlatButton {
                text: qsTrc("notation", "None")
                implicitWidth: Math.max(36, text.length * 9 + 16)
                implicitHeight: 28
                accentButton: !pickerModel.hasCustomSymbol && pickerModel.bass.length === 0

                navigation.panel: chordPickerNavPanel
                navigation.order: 200
                accessible.name: text

                onClicked: pickerModel.setBass("")
            }

            Repeater {
                model: pickerModel.roots

                delegate: FlatButton {
                    required property string modelData
                    required property int index

                    text: modelData
                    implicitWidth: Math.max(36, text.length * 9 + 16)
                    implicitHeight: 28
                    accentButton: !pickerModel.hasCustomSymbol && pickerModel.bass === modelData

                    navigation.panel: chordPickerNavPanel
                    navigation.order: 201 + index
                    accessible.name: qsTrc("notation", "Bass %1").arg(modelData)

                    onClicked: pickerModel.setBass(modelData)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            FlatButton {
                Layout.fillWidth: true
                implicitHeight: 32
                text: qsTrc("notation", "Type instead")

                navigation.panel: chordPickerNavPanel
                navigation.order: 300
                accessible.name: text

                onClicked: root.close()
            }

            FlatButton {
                Layout.fillWidth: true
                implicitHeight: 32
                text: qsTrc("notation", "Insert")
                accentButton: true
                enabled: pickerModel.preview.length > 0

                navigation.panel: chordPickerNavPanel
                navigation.order: 301
                accessible.name: text

                onClicked: pickerModel.apply()
            }
        }
    }
}
