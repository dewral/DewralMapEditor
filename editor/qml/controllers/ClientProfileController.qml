import QtQml
import Tibia 1.0

QtObject {
    id: controller
    required property var settings
    property var mapView: null

    property var clientPaths: ({})
    property var customProfiles: []
    property var mapProfiles: ({})
    readonly property var knownVersions: [740, 760, 772, 780, 792, 800, 810, 820, 840, 850, 854, 860, 870, 910, 920, 946, 954, 960, 970, 986, 1010, 1020, 1021, 1030, 1031, 1035, 1041, 1077, 1098, 10100, 1271, 1281, 1285, 1286, 1287, 1290, 1310, 1320, 1330]
    property int loadedClientVersion: 0
    property string loadedClientKey: ""
    property string loadedClientFolder: ""
    property string loadedClientFingerprint: ""

    function versionLabel(version) {
        if (version === 10100)
            return "10.100";
        return Math.floor(version / 100) + "." + ("0" + (version % 100)).slice(-2);
    }

    function profileVer(key) {
        var numeric = Number(key);
        if (!isNaN(numeric) && numeric > 0)
            return numeric;
        for (var i = 0; i < customProfiles.length; ++i)
            if (customProfiles[i].name === key)
                return customProfiles[i].base;
        return 0;
    }

    function profileLabel(key) {
        var numeric = Number(key);
        if (!isNaN(numeric) && numeric > 0)
            return versionLabel(numeric);
        return key + "  (" + versionLabel(profileVer(key)) + ")";
    }

    function allProfileKeys() {
        return knownVersions.map(function (version) {
            return String(version);
        }).concat(customProfiles.map(function (profile) {
            return profile.name;
        }));
    }

    function isCustomKey(key) {
        return isNaN(Number(key)) || Number(key) <= 0;
    }

    function addCustomProfile(name, base) {
        name = (name || "").trim();
        if (name === "" || !isNaN(Number(name)))
            return false;
        for (var i = 0; i < customProfiles.length; ++i)
            if (customProfiles[i].name.toLowerCase() === name.toLowerCase())
                return false;
        var copy = customProfiles.slice();
        copy.push({
            name: name,
            base: base
        });
        customProfiles = copy;
        settings.customProfilesJson = JSON.stringify(customProfiles);
        return true;
    }

    function removeCustomProfile(name) {
        customProfiles = customProfiles.filter(function (profile) {
            return profile.name !== name;
        });
        settings.customProfilesJson = JSON.stringify(customProfiles);
        var paths = JSON.parse(JSON.stringify(clientPaths));
        delete paths[name];
        clientPaths = paths;
        saveClientPaths();
    }

    property bool catalogLoaded: false
    function load() {
        catalogLoaded = true;
        try {
            clientPaths = JSON.parse(settings.clientPathsJson) || ({});
        } catch (e) {
            clientPaths = ({});
        }
        try {
            customProfiles = JSON.parse(settings.customProfilesJson) || [];
        } catch (e) {
            customProfiles = [];
        }
        try {
            mapProfiles = JSON.parse(settings.mapProfilesJson) || ({});
        } catch (e) {
            mapProfiles = ({});
        }

        if (Object.keys(clientPaths).length === 0 && settings.clientFolder !== "") {
            var paths = ({});
            paths["772"] = settings.clientFolder;
            clientPaths = paths;
            saveClientPaths();
        }
    }

    function saveClientPaths() {
        settings.clientPathsJson = JSON.stringify(clientPaths);
    }

    function setVersionFolder(key, folder) {
        var copy = JSON.parse(JSON.stringify(clientPaths));
        copy[String(key)] = folder;
        clientPaths = copy;
        saveClientPaths();
    }

    function clientFiles(folder) {
        if (!folder)
            return {
                dat: "",
                spr: "",
                otb: ""
            };
        return {
            dat: Backend.fileTools.findByExt(folder, "dat", "Tibia.dat"),
            spr: Backend.fileTools.findByExt(folder, "spr", "Tibia.spr"),
            otb: Backend.fileTools.findByExt(folder, "otb", "items.otb")
        };
    }

    function rememberMapProfile(mapPath, key) {
        if (mapPath === "" || key === "" || mapProfiles[mapPath] === key)
            return;
        var copy = JSON.parse(JSON.stringify(mapProfiles));
        copy[mapPath] = key;
        mapProfiles = copy;
        settings.mapProfilesJson = JSON.stringify(mapProfiles);
    }

    function switchMapProfile(key) {
        prepareClientVersion(key, function(success) {
            if (!success) return;
            settings.selectedClientKey = String(key);
            if (Backend.otbmReader.filePath !== "") rememberMapProfile(Backend.otbmReader.filePath, key);
        });
        return true;
    }

    function configuredProfileKeys() {
        var keys = Object.keys(clientPaths).filter(function (key) {
            return (clientPaths[key] || "") !== "";
        });
        keys.sort(function (a, b) {
            var na = Number(a), nb = Number(b);
            var ca = isNaN(na), cb = isNaN(nb);
            if (ca !== cb)
                return ca ? 1 : -1;
            return ca ? a.localeCompare(b) : na - nb;
        });
        return keys;
    }

    function resolveKeyForVersion(version) {
        if ((clientPaths[String(version)] || "") !== "")
            return String(version);
        for (var i = 0; i < customProfiles.length; ++i) {
            var profile = customProfiles[i];
            if (profile.base === version && (clientPaths[profile.name] || "") !== "")
                return profile.name;
        }
        return String(version);
    }

    function ensureClientLoaded(reader, preferredProfileKey) {
        return ensureClientForMap(reader.suggestedClientVersion(), reader.filePath, preferredProfileKey);
    }

    function profileMatchesVersion(key, version) {
        var base = profileVer(key);
        // OTB minor 3 is shared by 7.60 and 7.72. Respect an explicit 7.60 profile.
        return base === version || (version === 772 && base === 760);
    }

    function ensureClientForMap(version, path, preferredProfileKey) {
        if (version <= 0)
            version = 772;
        var preferred = preferredProfileKey === undefined || preferredProfileKey === null ? "" : String(preferredProfileKey);
        var compatible = preferred !== "" && profileMatchesVersion(preferred, version);
        var remembered = path !== "" ? (mapProfiles[path] || "") : "";
        var key = compatible ? preferred : (remembered !== "" && (clientPaths[remembered] || "") !== "" ? remembered : resolveKeyForVersion(version));
        return ensureClientVersion(key);
    }

    // Compatibility query only: expensive preparation is always asynchronous.
    function ensureClientVersion(key) {
        return loadedClientKey === String(key) && loadedClientFingerprint !== "";
    }
    property var pendingCallbacks: ({})
    property string pendingIdentity: ""
    property var pendingId: 0
    function prepareClientVersion(key, callback) {
        key = String(key);
        const identity = key + "|" + profileVer(key) + "|" + (clientPaths[key] || "");
        if (pendingIdentity === identity && pendingCallbacks[pendingId]) {
            pendingCallbacks[pendingId].push(callback);
            return pendingId;
        }
        const id = Backend.clientLoader.request(key, profileVer(key), clientPaths[key] || "", isCustomKey(key));
        pendingIdentity = identity; pendingId = id;
        pendingCallbacks[id] = [callback];
        return id;
    }
    function keyForMap(version, path, preferredProfileKey) {
        if (version <= 0) version = 772;
        const preferred = preferredProfileKey === undefined || preferredProfileKey === null ? "" : String(preferredProfileKey);
        const remembered = path !== "" ? (mapProfiles[path] || "") : "";
        return preferred !== "" && profileMatchesVersion(preferred, version) ? preferred
            : remembered !== "" && (clientPaths[remembered] || "") !== "" ? remembered : resolveKeyForVersion(version);
    }
    function prepareClientForMap(version, path, preferredKey, callback) {
        return prepareClientVersion(keyForMap(version, path, preferredKey), callback);
    }
    function prepareClientLoaded(reader, preferredKey, callback) {
        return prepareClientForMap(reader.suggestedClientVersion(), reader.filePath, preferredKey, callback);
    }
    property Connections clientEvents: Connections {
        target: typeof Backend.clientLoader !== "undefined" ? Backend.clientLoader : null
        function onProgressChanged() {
            if (Backend.clientLoader.busy)
                Backend.otbmReader.reportLoadingProgress(Backend.clientLoader.progress, Backend.clientLoader.stage);
        }
        function onFinished(id, key, fingerprint, success, error) {
            const callbacks = controller.pendingCallbacks[id];
            if (!callbacks) return;
            delete controller.pendingCallbacks[id];
            if (id === controller.pendingId) controller.pendingIdentity = "";
            if (success) {
                const changed = controller.loadedClientKey !== key || controller.loadedClientFingerprint !== fingerprint;
                controller.loadedClientKey = key;
                controller.loadedClientFolder = controller.clientPaths[key] || "";
                controller.loadedClientVersion = controller.profileVer(key);
                controller.loadedClientFingerprint = fingerprint;
                if (controller.mapView) {
                    controller.mapView.setClientAtlasIdentity(fingerprint);
                    if (changed || Backend.otbmReader.loading) controller.mapView.rebuildAtlas();
                }
            }
            callbacks.forEach(callback => callback(success, error));
        }
    }
}
