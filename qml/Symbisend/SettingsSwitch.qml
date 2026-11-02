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
SettingsItem {
    id: root

    property bool checked: false
    signal toggled(bool value)

    onClicked: root.toggled(!root.checked)
    // The Switch flips its own `checked` when tapped, which breaks a plain
    // binding; push every change of the setting to it instead.
    onCheckedChanged: toggle.checked = root.checked

    Switch {
        id: toggle
        checked: root.checked
        platformInverted: root.platformInverted
        onClicked: root.toggled(checked)
    }
}
