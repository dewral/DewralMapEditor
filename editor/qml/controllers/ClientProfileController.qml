import QtQml
import Tibia 1.0

QtObject {
    id: controller
    required property var settings
    required property var mapView

    property var clientPaths: ({})
    property var customProfiles: []
    property var mapProfiles: ({})
    readonly property var knownVersions: [760, 772, 780, 792, 800, 810, 820, 840, 850, 854, 860, 870, 910, 920, 946, 954, 960, 986, 1010, 1030, 1041, 1077, 1098]
    property int loadedClientVersion: 0
    property string loadedClientKey: ""
    property string loadedClientFolder: ""

    function versionLabel(version) {
        if (version >= 10100)
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

    function load() {
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

    function loadProfileData(key) {
        var version = profileVer(key);
        if (!isCustomKey(key)) {
            Backend.tilesetStore.loadForVersion(version);
            Backend.brushStore.loadForVersion(version);
            Backend.creatureStore.loadForVersion(version);
            Backend.itemsXml.loadForVersion(version);
            return;
        }
        Backend.tilesetStore.loadForDir(key);
        if (!Backend.brushStore.loadForDir(key))
            Backend.brushStore.loadForVersion(version);
        if (!Backend.creatureStore.loadForDir(key))
            Backend.creatureStore.loadForVersion(version);
        if (!Backend.itemsXml.loadForDir(key))
            Backend.itemsXml.loadForVersion(version);
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
        if (!ensureClientVersion(key))
            return false;
        if (Backend.otbmReader.filePath !== "")
            rememberMapProfile(Backend.otbmReader.filePath, key);
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
        var version = reader.suggestedClientVersion();
        if (version <= 0)
            version = 772;
        var preferred = preferredProfileKey === undefined || preferredProfileKey === null ? "" : String(preferredProfileKey);
        var compatible = preferred !== "" && profileVer(preferred) === version;
        var remembered = reader.filePath !== "" ? (mapProfiles[reader.filePath] || "") : "";
        var key = compatible ? preferred : (remembered !== "" && (clientPaths[remembered] || "") !== "" ? remembered : resolveKeyForVersion(version));
        return ensureClientVersion(key);
    }

    function ensureClientVersion(key) {
        key = String(key);
        var version = profileVer(key);
        var folder = clientPaths[key] || "";
        var files = clientFiles(folder);
        if (version <= 0 || folder === "" || !files.dat || !files.spr || !files.otb)
            return false;

        if (loadedClientKey === key && loadedClientFolder === folder)
        {
            if (Backend.otbmReader.loading) {
                Backend.otbmReader.reportLoadingProgress(94, "Rebuilding sprite atlas...");
                mapView.rebuildAtlas();
            }
            return true;
        }

        var hasOtfi = Backend.otfiReader.loadFromFolder(folder);
        var datFile = hasOtfi ? folder + "/" + Backend.otfiReader.metadataFile : files.dat;
        var sprFile = hasOtfi ? folder + "/" + Backend.otfiReader.spritesFile : files.spr;

        Backend.datReader.clientVersion = version;
        Backend.datReader.setOtfiOverrides(hasOtfi, Backend.otfiReader.extended, Backend.otfiReader.frameDurations, Backend.otfiReader.frameGroups);
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(78, "Loading item definitions...");
        var datOk = Backend.datReader.loadFile(datFile, 0);
        var extendedSpr = hasOtfi ? Backend.otfiReader.extended : version >= 960;
        var alphaSpr = hasOtfi ? Backend.otfiReader.transparency : false;
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(83, "Loading item sprites...");
        var sprOk = Backend.sprReader.loadFile(sprFile, 0, extendedSpr, alphaSpr);
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(88, "Loading server items...");
        var otbOk = Backend.otbReader.loadFile(files.otb);

        if (!datOk || !sprOk || !otbOk) {
            loadedClientVersion = 0;
            loadedClientKey = "";
            loadedClientFolder = "";
            mapView.rebuildAtlas();
            return false;
        }

        loadedClientVersion = version;
        loadedClientKey = key;
        loadedClientFolder = folder;
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(90, "Preparing palette sprites...");
        Backend.preloadPaletteSprites();
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(92, "Loading editor palettes...");
        loadProfileData(key);
        if (Backend.otbmReader.loading)
            Backend.otbmReader.reportLoadingProgress(94, "Rebuilding sprite atlas...");
        mapView.rebuildAtlas();
        return true;
    }
}
