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

    readonly property color bassKeyColor: "#E08A2C"
    readonly property color whiteKeyColor: "#fbfbfb"
    readonly property color blackKeyColor: "#2a2a2c"
    readonly property color keyBorderColor: "#b7b7bc"
    // Chord name and formula stay in this column, so choosing another chord
    // does not change the popup width. Advanced mode grows with its labels.
    readonly property int detailColumnWidth: 220
    readonly property int rootButtonWidth: 32
    readonly property int rootGap: 4
    readonly property int qualityPad: 4
    readonly property int toneRowHeight: 16
    readonly property int pianoHeight: 60

    function inversionLabel(inversion) {
        if (inversion <= 0) {
            return qsTrc("notation", "Inversion")
        }
        const teen = inversion % 100
        let suffix = "th"
        if (teen < 11 || teen > 13) {
            switch (inversion % 10) {
            case 1: suffix = "st"; break
            case 2: suffix = "nd"; break
            case 3: suffix = "rd"; break
            }
        }
        return qsTrc("notation", "%1 inv").arg(inversion + suffix)
    }

    // A chord has at most one of each pitch class, so inversion stops at 11.
    // The button keeps this widest label's width; a shorter one must not shrink the popup.
    readonly property string widestInversionLabel: {
        let widest = root.inversionLabel(0)
        let widestWidth = inversionFont.boundingRect(widest).width
        for (let inversion = 1; inversion <= 11; ++inversion) {
            const label = root.inversionLabel(inversion)
            const labelWidth = inversionFont.boundingRect(label).width
            if (labelWidth > widestWidth) {
                widest = label
                widestWidth = labelWidth
            }
        }
        return widest
    }

    readonly property int inversionControlWidth: {
        const fitted = Math.ceil(inversionSizer.implicitWidth)
        return fitted > 40 ? fitted : 0
    }

    readonly property var toneColumns: {
        const names = pickerModel.noteNames.length > 0 ? pickerModel.noteNames.split(" ") : []
        const degrees = pickerModel.formula.length > 0 ? pickerModel.formula.split(" ") : []
        const columns = []
        const count = Math.max(names.length, degrees.length)
        for (let i = 0; i < count; ++i) {
            columns.push({
                name: i < names.length ? names[i] : "",
                degree: i < degrees.length ? degrees[i] : ""
            })
        }
        return columns
    }

    // Plain arrays, so nested repeaters are not stuck on QVariantLists.
    readonly property var qualityTable: {
        const groups = []
        const source = pickerModel.qualityGroups
        if (!source) {
            return groups
        }
        for (let g = 0; g < source.length; ++g) {
            const columns = []
            const sourceColumns = source[g].columns
            if (!sourceColumns) {
                continue
            }
            for (let c = 0; c < sourceColumns.length; ++c) {
                const buttons = []
                const column = sourceColumns[c]
                for (let i = 0; i < column.length; ++i) {
                    const label = column[i] ? column[i].label : ""
                    if (!label) {
                        continue
                    }
                    buttons.push({
                        suffix: column[i].suffix,
                        label: label
                    })
                }
                columns.push(buttons)
            }
            groups.push({
                title: root.qualityGroupTitle(source[g].key),
                columns: columns
            })
        }
        return groups
    }

    readonly property var playedNotes: {
        const notes = []
        const source = pickerModel.soundingNotes
        if (!source) {
            return notes
        }
        for (let i = 0; i < source.length; ++i) {
            notes.push({
                pitch: source[i].pitch,
                bass: source[i].bass === true,
                name: source[i].name
            })
        }
        return notes
    }

    contentWidth: pickerModel.expanded ? content.implicitWidth : 0
    contentHeight: pickerModel.expanded ? content.implicitHeight : 0

    // PreferAbove pins the window bottom to the parent top and ignores y.
    // The parent is lifted so that top stays a gap above the open/close button.
    readonly property int toggleGap: 4
    readonly property int pickerLift: toggleButton.height + toggleGap + toggleGap

    margins: 0
    showArrow: false
    // Padding is PopupView's drop-shadow gutter. Collapsed, the window is only
    // that gutter, and the shadow draws on the open/close button.
    readonly property int shadowPadding: 12
    padding: pickerModel.expanded ? shadowPadding : 0
    useDropShadow: pickerModel.expanded
    placementPolicies: PopupView.PreferAbove
    focusPolicies: PopupView.DefaultFocus & ~PopupView.ClickFocus
    closePolicies: PopupView.NoAutoClose

    function qualityGroupTitle(key) {
        switch (key) {
        case "major": return qsTrc("notation", "Major")
        case "minor": return qsTrc("notation", "Minor")
        case "dominant": return qsTrc("notation", "Dominant")
        case "dimaug": return qsTrc("notation", "Dim / Aug")
        case "sus": return qsTrc("notation", "Sus")
        }
        return ""
    }

    function updatePosition() {
        // The loader is the first parent, and it is anchored to the chord container.
        if (!parent || parent.anchors.fill) {
            return
        }

        const rect = root.elementRect
        const lift = root.pickerLift
        parent.x = rect.x
        parent.y = rect.y - lift
        parent.width = rect.width
        parent.height = rect.height + lift

        root.x = Math.round((parent.width - root.width) / 2)
        root.placementPolicies = PopupView.PreferAbove
        root.popupPosition = PopupPosition.Top
        Qt.callLater(root.repositionWindowIfNeed)
    }

    onContentHeightChanged: updatePosition()
    onContentWidthChanged: updatePosition()
    onPickerLiftChanged: updatePosition()

    model: ChordPickerModel {
        id: pickerModel

        onExpandedChanged: root.updatePosition()
    }

    component QuietButton: FlatButton {
        id: quiet

        property bool selected: false

        accentButton: false
        transparent: true
        minWidth: 16
        margins: 8
        backgroundRadius: 4
        textFont.pixelSize: 12
        iconFont.pixelSize: 14
        iconColor: selected ? ui.theme.accentColor : ui.theme.fontPrimaryColor
        opacity: 1

        backgroundItem: Rectangle {
            radius: 4
            color: quiet.selected
                   ? Qt.alpha(ui.theme.accentColor, quiet.mouseArea.containsMouse ? 0.22 : 0.14)
                   : (quiet.mouseArea.containsMouse ? Qt.alpha(ui.theme.fontPrimaryColor, 0.08) : "transparent")
            border.width: 0
        }
    }

    NavigationPanel {
        id: chordPickerNavPanel
        name: "ChordPicker"
        direction: NavigationPanel.Horizontal
        accessible.name: qsTrc("notation", "Chord symbol picker")

        onNavigationEvent: function(event) {
            if (event.type === NavigationEvent.Escape) {
                pickerModel.setExpanded(false)
            }
        }
    }

    ColumnLayout {
        id: content
        spacing: 0

        ColumnLayout {
            visible: pickerModel.expanded
            spacing: 0

            Item {
                id: piano

                Layout.fillWidth: true
                Layout.preferredHeight: root.pianoHeight
                implicitWidth: 360
                implicitHeight: root.pianoHeight
                clip: true

                readonly property int keyLow: pickerModel.pianoLow
                readonly property int keyHigh: Math.max(pickerModel.pianoLow, pickerModel.pianoHigh)

                function isBlack(pitch) {
                    const pc = ((pitch % 12) + 12) % 12
                    return pc === 1 || pc === 3 || pc === 6 || pc === 8 || pc === 10
                }

                function roleFor(pitch) {
                    const notes = root.playedNotes
                    for (let i = 0; i < notes.length; ++i) {
                        if (notes[i].pitch === pitch) {
                            return notes[i].bass ? "bass" : "chord"
                        }
                    }
                    return ""
                }

                function keyColor(pitch, black) {
                    const role = roleFor(pitch)
                    if (role === "bass") {
                        return root.bassKeyColor
                    }
                    if (role === "chord") {
                        return ui.theme.accentColor
                    }
                    return black ? root.blackKeyColor : root.whiteKeyColor
                }

                readonly property var whitePitches: {
                    const pitches = []
                    for (let pitch = piano.keyLow; pitch <= piano.keyHigh; ++pitch) {
                        if (!piano.isBlack(pitch)) {
                            pitches.push(pitch)
                        }
                    }
                    return pitches
                }

                readonly property var blackPitches: {
                    const pitches = []
                    for (let pitch = piano.keyLow; pitch <= piano.keyHigh; ++pitch) {
                        if (piano.isBlack(pitch)) {
                            pitches.push(pitch)
                        }
                    }
                    return pitches
                }

                readonly property real whiteWidth: width / Math.max(1, whitePitches.length)
                readonly property real blackWidth: whiteWidth * 0.58

                function blackKeyX(pitch) {
                    let ordinal = 0
                    for (let cursor = piano.keyLow; cursor < pitch + 1; ++cursor) {
                        if (!piano.isBlack(cursor)) {
                            ordinal++
                        }
                    }
                    return ordinal * piano.whiteWidth - piano.blackWidth / 2
                }

                function keyCenterX(pitch) {
                    if (piano.isBlack(pitch)) {
                        return piano.blackKeyX(pitch) + piano.blackWidth / 2
                    }
                    let ordinal = 0
                    for (let cursor = piano.keyLow; cursor < pitch; ++cursor) {
                        if (!piano.isBlack(cursor)) {
                            ordinal++
                        }
                    }
                    return ordinal * piano.whiteWidth + piano.whiteWidth / 2
                }

                readonly property real blackKeyHeight: height * 0.62

                Rectangle {
                    anchors.fill: parent
                    color: "#c8c8cc"
                }

                Repeater {
                    model: piano.whitePitches

                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        x: index * piano.whiteWidth
                        width: piano.whiteWidth
                        height: piano.height
                        color: piano.keyColor(modelData, false)
                        border.width: 1
                        border.color: root.keyBorderColor
                    }
                }

                Repeater {
                    model: piano.blackPitches

                    delegate: Rectangle {
                        required property var modelData

                        x: piano.blackKeyX(modelData)
                        y: 0
                        width: piano.blackWidth
                        height: piano.blackKeyHeight
                        radius: 2
                        color: piano.keyColor(modelData, true)
                        border.width: 1
                        border.color: root.keyBorderColor
                    }
                }

                Repeater {
                    model: root.playedNotes

                    delegate: Text {
                        required property var modelData

                        readonly property bool black: piano.isBlack(modelData.pitch)

                        visible: modelData.pitch >= piano.keyLow && modelData.pitch <= piano.keyHigh
                        text: modelData.name
                        color: "white"
                        font.pixelSize: 10
                        font.bold: true
                        font.family: ui.theme.bodyBoldFont.family
                        style: Text.Outline
                        styleColor: Qt.rgba(0, 0, 0, 0.45)
                        x: piano.keyCenterX(modelData.pitch) - width / 2
                        y: black ? piano.blackKeyHeight - height - 4 : piano.height - height - 5
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: ui.theme.strokeColor
            }

            RowLayout {
                Layout.margins: 8
                spacing: 10

                ColumnLayout {
                    id: detailColumn
                    Layout.alignment: Qt.AlignTop
                    Layout.minimumWidth: Math.max(root.detailColumnWidth, detailSwitches.implicitWidth)
                    Layout.preferredWidth: Math.max(root.detailColumnWidth, detailSwitches.implicitWidth)
                    spacing: 6

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: chordNameLabel.implicitHeight + 12
                        radius: 4
                        color: "transparent"
                        border.width: 1
                        border.color: ui.theme.strokeColor

                        StyledTextLabel {
                            id: chordNameLabel
                            anchors.fill: parent
                            anchors.margins: 4
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: pickerModel.preview
                            font.bold: true
                            font.pixelSize: 18
                            font.family: ui.theme.bodyBoldFont.family
                        }
                    }

                    RowLayout {
                        visible: root.toneColumns.length > 0
                        spacing: 8

                        Column {
                            spacing: 0

                            StyledTextLabel {
                                width: implicitWidth
                                height: root.toneRowHeight
                                verticalAlignment: Text.AlignVCenter
                                text: qsTrc("notation", "Notes")
                                font.pixelSize: 11
                                opacity: 0.55
                            }

                            StyledTextLabel {
                                width: implicitWidth
                                height: root.toneRowHeight
                                verticalAlignment: Text.AlignVCenter
                                text: qsTrc("notation", "Formula")
                                font.pixelSize: 11
                                opacity: 0.55
                            }
                        }

                        Repeater {
                            model: root.toneColumns

                            delegate: Column {
                                id: toneColumn
                                required property var modelData

                                spacing: 0

                                readonly property real columnWidth: Math.max(16, nameLabel.implicitWidth, degreeLabel.implicitWidth)

                                StyledTextLabel {
                                    id: nameLabel
                                    width: toneColumn.columnWidth
                                    height: root.toneRowHeight
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    text: toneColumn.modelData.name
                                    font.bold: true
                                    font.pixelSize: 12
                                }

                                StyledTextLabel {
                                    id: degreeLabel
                                    width: toneColumn.columnWidth
                                    height: root.toneRowHeight
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    text: toneColumn.modelData.degree
                                    font.pixelSize: 12
                                    opacity: 0.72
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        id: detailSwitches
                        spacing: 6

                        Row {
                            spacing: 4

                            Rectangle {
                                implicitWidth: spellingRow.implicitWidth + 2
                                implicitHeight: spellingRow.implicitHeight + 2
                                width: implicitWidth
                                height: implicitHeight
                                radius: 4
                                color: "transparent"
                                border.width: 1
                                border.color: ui.theme.strokeColor

                                Row {
                                    id: spellingRow
                                    x: 1
                                    y: 1
                                    spacing: 0

                                    QuietButton {
                                        id: sharpButton
                                        icon: IconCode.SHARP
                                        selected: pickerModel.preferSharps

                                        navigation.panel: chordPickerNavPanel
                                        navigation.order: 40
                                        accessible.name: qsTrc("notation", "Spell roots with sharps")
                                        toolTipTitle: qsTrc("notation", "Sharps")

                                        onClicked: pickerModel.setPreferSharps(true)
                                    }

                                    Rectangle {
                                        width: 1
                                        height: sharpButton.height
                                        color: ui.theme.strokeColor
                                    }

                                    QuietButton {
                                        icon: IconCode.FLAT
                                        selected: !pickerModel.preferSharps

                                        navigation.panel: chordPickerNavPanel
                                        navigation.order: 41
                                        accessible.name: qsTrc("notation", "Spell roots with flats")
                                        toolTipTitle: qsTrc("notation", "Flats")

                                        onClicked: pickerModel.setPreferSharps(false)
                                    }
                                }
                            }

                            Rectangle {
                                implicitWidth: voicingRow.implicitWidth + 2
                                implicitHeight: voicingRow.implicitHeight + 2
                                width: implicitWidth
                                height: implicitHeight
                                radius: 4
                                color: "transparent"
                                border.width: 1
                                border.color: ui.theme.strokeColor

                                Row {
                                    id: voicingRow
                                    x: 1
                                    y: 1
                                    spacing: 0

                                    QuietButton {
                                        id: literalButton

                                        TextMetrics {
                                            id: literalMetrics
                                            font: ui.theme.bodyFont
                                            text: qsTrc("notation", "Literal")
                                        }

                                        text: literalMetrics.text
                                        width: Math.ceil(literalMetrics.boundingRect.width) + 20
                                        selected: pickerModel.literal

                                        navigation.panel: chordPickerNavPanel
                                        navigation.order: 42
                                        accessible.name: qsTrc("notation", "Literal voicing")

                                        onClicked: pickerModel.setLiteral(true)
                                    }

                                    Rectangle {
                                        width: 1
                                        height: literalButton.height
                                        color: ui.theme.strokeColor
                                    }

                                    QuietButton {
                                        TextMetrics {
                                            id: jazzMetrics
                                            font: ui.theme.bodyFont
                                            text: qsTrc("notation", "Jazz")
                                        }

                                        text: jazzMetrics.text
                                        width: Math.ceil(jazzMetrics.boundingRect.width) + 20
                                        selected: !pickerModel.literal

                                        navigation.panel: chordPickerNavPanel
                                        navigation.order: 43
                                        accessible.name: qsTrc("notation", "Jazz voicing")

                                        onClicked: pickerModel.setLiteral(false)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            implicitWidth: modeRow.implicitWidth + 2
                            implicitHeight: modeRow.implicitHeight + 2
                            radius: 4
                            color: "transparent"
                            border.width: 1
                            border.color: ui.theme.strokeColor

                            Row {
                                id: modeRow
                                x: 1
                                y: 1
                                spacing: 0

                                QuietButton {
                                    id: simpleButton
                                    text: qsTrc("notation", "Simple")
                                    selected: !pickerModel.advanced

                                    navigation.panel: chordPickerNavPanel
                                    navigation.order: 44
                                    accessible.name: qsTrc("notation", "Simple chords")

                                    onClicked: pickerModel.setAdvanced(false)
                                }

                                Rectangle {
                                    width: 1
                                    height: simpleButton.height
                                    color: ui.theme.strokeColor
                                }

                                QuietButton {
                                    text: qsTrc("notation", "Advanced")
                                    selected: pickerModel.advanced

                                    navigation.panel: chordPickerNavPanel
                                    navigation.order: 45
                                    accessible.name: qsTrc("notation", "Advanced chords")

                                    onClicked: pickerModel.setAdvanced(true)
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: qualityGrid.implicitWidth
                    Layout.alignment: Qt.AlignTop
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        ColumnLayout {
                            id: pitchCaptions
                            spacing: 2
                            Layout.alignment: Qt.AlignTop

                            Item {
                                implicitWidth: Math.max(rootCaption.implicitWidth, bassCaption.implicitWidth)
                                implicitHeight: 30

                                StyledTextLabel {
                                    id: rootCaption
                                    anchors.centerIn: parent
                                    text: qsTrc("notation", "Root")
                                    font: ui.theme.bodyFont
                                    opacity: 0.6
                                }
                            }

                            Item {
                                implicitWidth: parent.width
                                implicitHeight: 16

                                StyledTextLabel {
                                    id: bassCaption
                                    anchors.centerIn: parent
                                    text: qsTrc("notation", "Bass")
                                    font: ui.theme.bodyFont
                                    opacity: 0.6
                                }
                            }
                        }

                        Row {
                            spacing: root.rootGap
                            Layout.alignment: Qt.AlignTop

                            Repeater {
                                model: pickerModel.pitchNames

                                delegate: Column {
                                    id: noteColumn
                                    required property string modelData
                                    required property int index

                                    width: root.rootButtonWidth
                                    spacing: 2

                                    FlatButton {
                                        width: root.rootButtonWidth
                                        minWidth: 0
                                        margins: 2
                                        textFont.pixelSize: 12
                                        text: noteColumn.modelData
                                        accentButton: !pickerModel.hasCustomSymbol && pickerModel.root === noteColumn.modelData

                                        navigation.panel: chordPickerNavPanel
                                        navigation.order: 10 + noteColumn.index
                                        accessible.name: qsTrc("notation", "Root %1").arg(noteColumn.modelData)

                                        onClicked: pickerModel.setRoot(noteColumn.modelData)
                                    }

                                    Item {
                                        width: root.rootButtonWidth
                                        height: 16

                                        readonly property bool selected: !pickerModel.hasCustomSymbol
                                                                         && (pickerModel.bass.length === 0
                                                                             ? pickerModel.root
                                                                             : pickerModel.bass) === noteColumn.modelData

                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 10
                                            height: 10
                                            radius: 5
                                            border.width: 1
                                            border.color: parent.selected ? ui.theme.accentColor : ui.theme.fontPrimaryColor
                                            color: parent.selected ? ui.theme.accentColor : "transparent"
                                            opacity: parent.selected ? 1 : 0.4
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: pickerModel.setBass(noteColumn.modelData)
                                        }

                                        Accessible.role: Accessible.RadioButton
                                        Accessible.name: qsTrc("notation", "Bass %1").arg(noteColumn.modelData)
                                        Accessible.checked: selected
                                    }
                                }
                            }
                        }

                        QuietButton {
                            Layout.alignment: Qt.AlignTop
                            Layout.minimumWidth: root.inversionControlWidth > 0 ? root.inversionControlWidth : implicitWidth
                            Layout.preferredWidth: root.inversionControlWidth > 0 ? root.inversionControlWidth : implicitWidth
                            Layout.maximumWidth: root.inversionControlWidth > 0 ? root.inversionControlWidth : 10000
                            orientation: Qt.Horizontal
                            icon: IconCode.LOOP
                            text: root.inversionLabel(pickerModel.inversion)

                            navigation.panel: chordPickerNavPanel
                            navigation.order: 22
                            accessible.name: root.inversionLabel(pickerModel.inversion)
                            toolTipTitle: root.inversionLabel(pickerModel.inversion)

                            onClicked: pickerModel.cycleInversion()
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 10
                        }

                        FlatButton {
                            Layout.alignment: Qt.AlignTop
                            minWidth: 16
                            margins: 4
                            icon: IconCode.TICK_RIGHT_ANGLE
                            accentButton: true
                            enabled: pickerModel.preview.length > 0

                            navigation.panel: chordPickerNavPanel
                            navigation.order: 45
                            accessible.name: qsTrc("notation", "Insert")
                            toolTipTitle: qsTrc("notation", "Insert chord symbol")

                            onClicked: pickerModel.apply()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Item {
                            Layout.preferredWidth: pitchCaptions.implicitWidth
                            Layout.minimumWidth: pitchCaptions.implicitWidth
                            Layout.maximumWidth: pitchCaptions.implicitWidth
                        }

                        RowLayout {
                            id: qualityGrid
                            spacing: 14

                            Repeater {
                                model: root.qualityTable

                                delegate: ColumnLayout {
                                    id: groupColumn
                                    required property var modelData
                                    required property int index

                                    Layout.alignment: Qt.AlignTop
                                    spacing: 4

                                    StyledTextLabel {
                                        Layout.fillWidth: true
                                        text: groupColumn.modelData.title
                                        font.pixelSize: 11
                                        font.family: ui.theme.bodyFont.family
                                        horizontalAlignment: Text.AlignLeft
                                        opacity: 0.55
                                    }

                                    RowLayout {
                                        spacing: 4

                                        Repeater {
                                            model: groupColumn.modelData.columns

                                            delegate: ColumnLayout {
                                                id: qualityColumn
                                                required property var modelData
                                                required property int index

                                                Layout.alignment: Qt.AlignTop
                                                spacing: 4

                                                Repeater {
                                                    model: qualityColumn.modelData

                                                    delegate: FlatButton {
                                                        id: qualityButton
                                                        required property var modelData
                                                        required property int index

                                                        readonly property string labelText: String(modelData.label)
                                                        // Body font is what the label actually draws. Size the
                                                        // button to that, with a little room so it never ellipsizes.
                                                        readonly property int fittedWidth: Math.max(28, Math.ceil(Math.max(labelMetrics.width, labelMetrics.boundingRect.width)) + root.qualityPad * 2)

                                                        TextMetrics {
                                                            id: labelMetrics
                                                            font: ui.theme.bodyFont
                                                            text: qualityButton.labelText
                                                        }

                                                        Layout.fillWidth: true
                                                        Layout.minimumWidth: fittedWidth
                                                        Layout.preferredWidth: fittedWidth
                                                        clip: false
                                                        minWidth: fittedWidth
                                                        margins: root.qualityPad
                                                        textFont: ui.theme.bodyFont
                                                        text: labelText
                                                        accentButton: !pickerModel.hasCustomSymbol
                                                                      && pickerModel.quality === qualityButton.modelData.suffix

                                                        navigation.panel: chordPickerNavPanel
                                                        navigation.order: 60 + groupColumn.index * 40
                                                                          + qualityColumn.index * 8 + index
                                                        accessible.name: labelText

                                                        onClicked: pickerModel.setQuality(qualityButton.modelData.suffix)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

    }

    FontMetrics {
        id: inversionFont
        font.family: ui.theme.bodyFont.family
        font.pixelSize: 12
        font.weight: ui.theme.bodyFont.weight
        font.bold: ui.theme.bodyFont.bold
    }

    // Sizes the inversion button. Not shown.
    QuietButton {
        id: inversionSizer
        visible: false
        enabled: false
        orientation: Qt.Horizontal
        icon: IconCode.LOOP
        text: root.widestInversionLabel
    }

    FlatButton {
        id: toggleButton

        parent: root.parent
        x: parent ? Math.round((parent.width - width) / 2) : 0
        // The parent top is pickerLift above the chord. Sit in that strip, a gap above the symbol.
        y: parent ? root.pickerLift - height - root.toggleGap : 0
        z: 2

        minWidth: 16
        margins: 6
        icon: IconCode.MUSIC_NOTES
        iconColor: pickerModel.expanded ? "white" : ui.theme.fontPrimaryColor
        accentButton: pickerModel.expanded
        backgroundRadius: 8

        navigation.panel: chordPickerNavPanel
        navigation.order: 0
        accessible.name: qsTrc("notation", "Show chord picker")
        toolTipTitle: qsTrc("notation", "Show chord picker")

        onClicked: pickerModel.setExpanded(!pickerModel.expanded)
    }
}
