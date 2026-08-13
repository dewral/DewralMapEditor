import QtQml

QtObject {
    id: controller
    required property var settings
    property string profileKey: ""

    property var customPalettes: ({})
    property var recentByProfile: ({})
    property var favoritesByProfile: ({})
    readonly property var customPaletteNames: Object.keys(customPalettes).sort()
    readonly property int iconSizePx: settings.iconSize
    readonly property string storageKey: profileKey && profileKey.length > 0
                                         ? profileKey : "default"
    readonly property var recentBrushIds: recentByProfile[storageKey] || []
    readonly property var favoriteBrushIds: favoritesByProfile[storageKey] || []

    function load() {
        try {
            customPalettes = JSON.parse(settings.customPalettesJson) || ({});
        } catch (e) {
            customPalettes = ({});
        }
        try {
            recentByProfile = JSON.parse(settings.recentBrushesJson) || ({});
        } catch (e) {
            recentByProfile = ({});
        }
        try {
            favoritesByProfile = JSON.parse(settings.favoriteBrushesJson) || ({});
        } catch (e) {
            favoritesByProfile = ({});
        }
    }

    function save() {
        settings.customPalettesJson = JSON.stringify(customPalettes);
    }

    function saveQuickCollections() {
        settings.recentBrushesJson = JSON.stringify(recentByProfile);
        settings.favoriteBrushesJson = JSON.stringify(favoritesByProfile);
    }

    function recordBrushUse(serverId) {
        if (serverId <= 0)
            return;
        var current = recentBrushIds.slice();
        if (current.length > 0 && current[0] === serverId)
            return;
        const existing = current.indexOf(serverId);
        if (existing >= 0)
            current.splice(existing, 1);
        current.unshift(serverId);
        if (current.length > 20)
            current.length = 20;
        var copy = JSON.parse(JSON.stringify(recentByProfile));
        copy[storageKey] = current;
        recentByProfile = copy;
        saveQuickCollections();
    }

    function isFavorite(serverId) {
        return favoriteBrushIds.indexOf(serverId) >= 0;
    }

    function toggleFavorite(serverId) {
        if (serverId <= 0)
            return;
        var current = favoriteBrushIds.slice();
        const existing = current.indexOf(serverId);
        if (existing >= 0)
            current.splice(existing, 1);
        else
            current.unshift(serverId);
        var copy = JSON.parse(JSON.stringify(favoritesByProfile));
        copy[storageKey] = current;
        favoritesByProfile = copy;
        saveQuickCollections();
    }

    function clearRecentBrushes() {
        var copy = JSON.parse(JSON.stringify(recentByProfile));
        copy[storageKey] = [];
        recentByProfile = copy;
        saveQuickCollections();
    }

    function addCustomPalette(name) {
        if (!name || customPalettes[name] !== undefined)
            return false;
        var copy = JSON.parse(JSON.stringify(customPalettes));
        copy[name] = [];
        customPalettes = copy;
        save();
        return true;
    }

    function deleteCustomPalette(name) {
        var copy = JSON.parse(JSON.stringify(customPalettes));
        delete copy[name];
        customPalettes = copy;
        save();
    }

    function addItemToPalette(name, serverId) {
        if (customPalettes[name] === undefined)
            return;
        var copy = JSON.parse(JSON.stringify(customPalettes));
        if (copy[name].indexOf(serverId) < 0)
            copy[name].push(serverId);
        customPalettes = copy;
        save();
    }

    function removeItemFromPalette(name, serverId) {
        if (customPalettes[name] === undefined)
            return;
        var copy = JSON.parse(JSON.stringify(customPalettes));
        var index = copy[name].indexOf(serverId);
        if (index >= 0)
            copy[name].splice(index, 1);
        customPalettes = copy;
        save();
    }
}
