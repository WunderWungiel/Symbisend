import QtQuick 1.1
import com.nokia.symbian 1.1

// On/off row on the Settings page. Tapping the row or the switch emits
// toggled(); the owner writes the setting, which flows back via `checked`:
//
//   SettingsSwitch {
//       title: "Quick save"
//       checked: settings.quickSave
//       onToggled: settings.quickSave = value
//   }
//
// The owner may refuse a change (not write the setting, e.g. to ask
// something first); the switch then flips back to `checked`.
// With rowToggles: false, tapping the row only emits clicked() (for an
// owner that does something else on a row tap); the switch still toggles.
SettingsItem {
    id: root

    property bool checked: false
    property bool rowToggles: true
    signal toggled(bool value)

    onClicked: if (root.rowToggles) root.toggled(!root.checked)
    // The Switch flips its own `checked` when tapped, which breaks a plain
    // binding; push every change of the setting to it instead.
    onCheckedChanged: toggle.checked = root.checked

    Switch {
        id: toggle
        checked: root.checked
        platformInverted: root.platformInverted
        onClicked: {
            root.toggled(checked)
            toggle.checked = root.checked // undo it if the owner refused
        }
    }
}
