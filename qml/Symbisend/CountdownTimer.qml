import QtQuick 1.1

// Counts down once a second and then says so, for a "Close (3)" button.
Timer {
    // Seconds left.
    property int counter: 0

    signal elapsed()

    function begin(seconds) {
        counter = seconds
        start()
    }

    interval: 1000
    repeat: true
    onTriggered: {
        if (counter <= 1) {
            stop()
            elapsed()
        } else {
            counter--
        }
    }
}
