import QtQml
import Tibia 1.0

QtObject {
    id: controller
    required property var settings
    property var mapView: null

    property var clientPaths: ({})
    property var customProfiles: []
    property var mapProfiles: ({})
    property var profileEntries: ({})
    property var documentConfigurations: new Map()
    function copy(value) { return JSON.parse(JSON.stringify(value)); }
    function profile(key) {
        if(profileEntries[key])return copy(profileEntries[key]);
        return {id:String(key),name:isCustomKey(key) ? String(key) : "Tibia "+versionLabel(profileVer(key)),
            version:profileVer(key),folder:clientPaths[key] || "",files:{},flags:{},resourceKey:isCustomKey(key) ? String(key) : ""};
    }
    function persistProfiles() {
        if(typeof settings.clientProfilesJson!=="undefined")settings.clientProfilesJson=JSON.stringify({version:1,profiles:profileEntries});
    }
    function saveProfile(value) {
        const entry=copy(value), entries=copy(profileEntries);
        if(!entry.id)entry.id=Backend.clientLoader.newProfileId();
        entry.name=String(entry.name || "").trim();
        if(!entry.name || !(entry.version>0))return "Enter a name and client version.";
        for(const id of Object.keys(entries))if(id!==entry.id && entries[id].name.toLowerCase()===entry.name.toLowerCase())return "This profile name already exists.";
        entries[entry.id]=entry;profileEntries=entries;
        const paths=copy(clientPaths);paths[entry.id]=entry.folder || "";clientPaths=paths;
        persistProfiles();saveClientPaths();return "";
    }
    function deleteProfile(id) {
        const entries=copy(profileEntries);delete entries[id];profileEntries=entries;
        const paths=copy(clientPaths);delete paths[id];clientPaths=paths;
        customProfiles=customProfiles.filter(p=>p.name!==id);
        settings.customProfilesJson=JSON.stringify(customProfiles);
        persistProfiles();saveClientPaths();
    }
    function profileStatus(key) {
        const p=profile(key), resolved=Backend.clientLoader.resolveProfile(p);
        if(!resolved.success)return "Missing or ambiguous files";
        return p.validation && p.validation.success && p.validation.fingerprint===resolved.fingerprint ? "Ready" : "Not validated";
    }
    property var activeConfiguration: null
    function bindCurrentDocument(key) { documentConfigurations.set(Backend.docMgr.current,copy(activeConfiguration && activeConfiguration.id===key ? activeConfiguration : profile(key))); }

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
        if(profileEntries[key])return Number(profileEntries[key].version);
        var numeric = Number(key);
        if (!isNaN(numeric) && numeric > 0)
            return numeric;
        for (var i = 0; i < customProfiles.length; ++i)
            if (customProfiles[i].name === key)
                return customProfiles[i].base;
        return 0;
    }

    function profileLabel(key) {
        if(profileEntries[key])return profileEntries[key].name;
        var numeric = Number(key);
        if (!isNaN(numeric) && numeric > 0)
            return versionLabel(numeric);
        return key + "  (" + versionLabel(profileVer(key)) + ")";
    }

    function allProfileKeys() {
        return Array.from(new Set(knownVersions.map(String).concat(customProfiles.map(p=>p.name),Object.keys(profileEntries))));
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

    function removeCustomProfile(name) { deleteProfile(name); }

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
        try {
            const saved=typeof settings.clientProfilesJson!=="undefined" && settings.clientProfilesJson ? JSON.parse(settings.clientProfilesJson) : null;
            if(saved && saved.version===1)profileEntries=saved.profiles || {};
            else {
                const entries={};
                const keys=Array.from(new Set(Object.keys(clientPaths).concat(customProfiles.map(p=>p.name))));
                for(const key of keys)entries[key]=profile(key);
                profileEntries=entries;persistProfiles();
            }
        } catch(error) { profileEntries={}; }
    }

    function saveClientPaths() {
        settings.clientPathsJson = JSON.stringify(clientPaths);
    }

    function setVersionFolder(key, folder) {
        var paths = JSON.parse(JSON.stringify(clientPaths));
        paths[String(key)] = folder;
        clientPaths = paths;
        saveClientPaths();
        const entry=profile(key);entry.folder=folder;delete entry.validation;
        const entries=copy(profileEntries);entries[key]=entry;profileEntries=entries;persistProfiles();
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
            bindCurrentDocument(String(key));
            Backend.docMgr.setCurrentProfileKey(String(key));
            settings.selectedClientKey = String(key);
            if (Backend.otbmReader.filePath !== "") rememberMapProfile(Backend.otbmReader.filePath, key);
        });
        return true;
    }

    function configuredProfileKeys() {
        return Array.from(new Set(Object.keys(profileEntries).concat(Object.keys(clientPaths).filter(k=>clientPaths[k]))));
    }

    function resolveKeyForVersion(version) {
        const matches=configuredProfileKeys().filter(key=>profileMatchesVersion(key,version));
        return matches.length===1 ? matches[0] : String(version);
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
        const key=keyForMap(version,path,preferredProfileKey);
        return ensureClientVersion(key);
    }

    // Compatibility query only: expensive preparation is always asynchronous.
    function ensureClientVersion(key) {
        return loadedClientKey === String(key) && loadedClientFingerprint !== "";
    }
    property var pendingConfigurations: ({})
    property var pendingCallbacks: ({})
    property string pendingIdentity: ""
    property var pendingId: 0
    function prepareClientVersion(key, callback) {
        key = String(key);
        return prepareConfiguration(profile(key),callback);
    }
    function prepareConfiguration(config,callback) {
        const key=config.id;
        const identity=JSON.stringify(config);
        if (pendingIdentity === identity && pendingCallbacks[pendingId]) {
            pendingCallbacks[pendingId].push(callback);
            return pendingId;
        }
        const id = Backend.clientLoader.requestProfile(config);
        pendingIdentity = identity; pendingId = id;
        pendingConfigurations[id]=copy(config);
        pendingCallbacks[id] = [callback];
        return id;
    }
    function keyForMap(version, path, preferredProfileKey) {
        if (version <= 0) version = 772;
        const preferred = preferredProfileKey === undefined || preferredProfileKey === null ? "" : String(preferredProfileKey);
        const remembered = path !== "" ? (mapProfiles[path] || "") : "";
        return remembered !== "" ? remembered
            : preferred !== "" && profileMatchesVersion(preferred, version) ? preferred : resolveKeyForVersion(version);
    }
    function prepareClientForMap(version, path, preferredKey, callback) {
        return prepareClientVersion(keyForMap(version, path, preferredKey), callback);
    }
    function prepareClientLoaded(reader, preferredKey, callback) {
        const previous=documentConfigurations.get(reader);
        if(previous)return prepareConfiguration(previous,callback);
        const key=keyForMap(reader.suggestedClientVersion(),reader.filePath,preferredKey);
        const config=profile(key);
        return prepareConfiguration(config,function(success,error) {
            if(success)documentConfigurations.set(reader,copy(config));
            callback(success,error);
        });
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
            const config=controller.pendingConfigurations[id];
            delete controller.pendingConfigurations[id];
            delete controller.pendingCallbacks[id];
            if (id === controller.pendingId) controller.pendingIdentity = "";
            if (success) {
                const changed = controller.loadedClientKey !== key || controller.loadedClientFingerprint !== fingerprint;
                controller.loadedClientKey = key;
                controller.activeConfiguration=controller.copy(config);
                controller.loadedClientFolder = config.folder || "";
                controller.loadedClientVersion = config.version;
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
