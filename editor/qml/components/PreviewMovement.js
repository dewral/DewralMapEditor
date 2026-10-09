.pragma library

function direction(key, modifiers, hotkeys) {
    if (hotkeys.capturing) return null;
    if (!(modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier))) {
        switch (key) {
        case Qt.Key_Left: return {dx: -1, dy: 0};
        case Qt.Key_Right: return {dx: 1, dy: 0};
        case Qt.Key_Up: return {dx: 0, dy: -1};
        case Qt.Key_Down: return {dx: 0, dy: 1};
        case Qt.Key_Home: return {dx: -1, dy: -1};
        case Qt.Key_PageUp: return {dx: 1, dy: -1};
        case Qt.Key_End: return {dx: -1, dy: 1};
        case Qt.Key_PageDown: return {dx: 1, dy: 1};
        }
    }
    if (hotkeys.matches("preview_left", key, modifiers)) return {dx: -1, dy: 0};
    if (hotkeys.matches("preview_right", key, modifiers)) return {dx: 1, dy: 0};
    if (hotkeys.matches("preview_up", key, modifiers)) return {dx: 0, dy: -1};
    if (hotkeys.matches("preview_down", key, modifiers)) return {dx: 0, dy: 1};
    return null;
}

function press(heldKeys, key, modifiers, hotkeys, autoRepeat) {
    const movement = direction(key, modifiers, hotkeys);
    if (!movement) return false;
    if (!autoRepeat) heldKeys[key] = movement;
    return true;
}

function release(heldKeys, key, autoRepeat) {
    if (!heldKeys[key]) return false;
    // Track the physical key so releasing a modifier first cannot leave walking stuck.
    if (!autoRepeat) delete heldKeys[key];
    return true;
}

function vector(heldKeys) {
    let left = false, right = false, up = false, down = false;
    for (const key of Object.keys(heldKeys)) {
        const movement = heldKeys[key];
        left = left || movement.dx < 0;
        right = right || movement.dx > 0;
        up = up || movement.dy < 0;
        down = down || movement.dy > 0;
    }
    let dx = (right ? 1 : 0) - (left ? 1 : 0);
    let dy = (down ? 1 : 0) - (up ? 1 : 0);
    // Keep the preview's dedicated diagonal keys and their existing precedence.
    for (const key of [Qt.Key_Home, Qt.Key_PageUp, Qt.Key_End, Qt.Key_PageDown]) {
        if (heldKeys[key]) { dx = heldKeys[key].dx; dy = heldKeys[key].dy; }
    }
    return {dx: dx, dy: dy};
}
