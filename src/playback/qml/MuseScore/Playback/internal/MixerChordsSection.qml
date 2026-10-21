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

import Muse.Ui
import Muse.UiComponents
import MuseScore.Playback

MixerPanelSection {
    id: root

    headerTitle: qsTrc("playback", "Chords")

    Item {
        id: content

        required property MixerChannelItem channelItem

        readonly property MixerChannelItem chordChannel: channelItem.chordChannelItem
        readonly property bool hasChords: Boolean(chordChannel)

        // Channel faders run down to -∞ (-60 dB), which parks everyday levels
        // at the end of a knob. The chord dial rides a tighter -24..+12 dB scale.
        readonly property real chordVolumeMin: -24

        width: root.channelItemWidth
        height: 28

        property string accessibleName: (Boolean(root.needReadChannelName) ? channelItem.title + " " : "") + root.headerTitle

        StyledTextLabel {
            anchors.centerIn: parent

            visible: !content.hasChords
            text: "-"
        }

        Item {
            id: controls

            anchors.centerIn: parent
            visible: content.hasChords

            width: volumeDial.width + muteButton.width + soloButton.width + 8
            height: volumeDial.height

            KnobControl {
                id: volumeDial

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                radius: 12

                from: content.chordVolumeMin
                to: content.chordChannel ? content.chordChannel.volumeLevelMax : 12
                value: content.chordChannel
                       ? Math.max(content.chordVolumeMin,
                                  Math.min(content.chordChannel.volumeLevel, content.chordChannel.volumeLevelMax))
                       : 0
                stepSize: 1

                enabled: content.hasChords && !content.chordChannel.hasVolumeAutomation
                opacity: enabled ? 1.0 : ui.theme.itemOpacityDisabled

                navigation.name: "ChordsVolumeDial"
                navigation.panel: content.channelItem.panel
                navigation.row: root.navigationRowStart
                navigation.accessible.name: content.accessibleName + " " + Math.round(value * 10) / 10
                navigation.onActiveChanged: {
                    if (navigation.active) {
                        root.navigateControlIndexChanged({row: navigation.row, column: navigation.column})
                    }
                }

                onNewValueRequested: function(newValue) {
                    if (content.chordChannel) {
                        content.chordChannel.volumeLevel = Math.round(newValue * 10) / 10
                    }
                }
            }

            FlatToggleButton {
                id: muteButton

                anchors.left: volumeDial.right
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter

                height: 20
                width: 20

                icon: IconCode.MUTE
                checked: content.chordChannel ? content.chordChannel.muted : false

                enabled: content.hasChords && !(content.chordChannel.muted && content.chordChannel.forceMute)

                navigation.name: "ChordsMuteButton"
                navigation.panel: content.channelItem.panel
                navigation.row: root.navigationRowStart + 1
                navigation.accessible.name: content.accessibleName + " " + qsTrc("playback", "Mute")
                navigation.onActiveChanged: {
                    if (navigation.active) {
                        root.navigateControlIndexChanged({row: navigation.row, column: navigation.column})
                    }
                }

                onToggled: {
                    if (content.chordChannel) {
                        content.chordChannel.muted = !checked
                    }
                }
            }

            FlatToggleButton {
                id: soloButton

                anchors.left: muteButton.right
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter

                height: 20
                width: 20

                icon: IconCode.SOLO
                checked: content.chordChannel ? content.chordChannel.solo : false

                enabled: content.hasChords && (!content.chordChannel.muted || content.chordChannel.forceMute)

                navigation.name: "ChordsSoloButton"
                navigation.panel: content.channelItem.panel
                navigation.row: root.navigationRowStart + 2
                navigation.accessible.name: content.accessibleName + " " + qsTrc("playback", "Solo")
                navigation.onActiveChanged: {
                    if (navigation.active) {
                        root.navigateControlIndexChanged({row: navigation.row, column: navigation.column})
                    }
                }

                onToggled: {
                    if (content.chordChannel) {
                        content.chordChannel.solo = !content.chordChannel.solo
                    }
                }
            }
        }
    }
}
