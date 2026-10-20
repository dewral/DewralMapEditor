.pragma library

// Resolve a brush variant to the entry actually displayed in its palette.
// A definition alone does not make a brush visible in the editor.
function locate(candidate, map, tilesets, store, collectionOnly) {
    if (!candidate || !candidate.serverId)
        return null;
    const kind = candidate.kind;
    const categories = collectionOnly ? ["collection"]
        : kind === "ground" || kind === "wall" ? ["terrain", "doodad", "item", "collection"]
        : kind === "door" ? ["door"]
        : ["doodad", "terrain", "item", "collection"];
    for (const category of categories) {
        const names = tilesets.namesFor(category);
        for (const tileset of names) {
            for (const id of tilesets.itemsFor(category, tileset)) {
                const matches = kind === "raw" ? id === candidate.serverId
                    : kind === "door" ? (id === candidate.serverId
                        || (store.canSwitchDoor(candidate.serverId)
                            && id === store.switchedDoorItem(candidate.serverId)))
                    : map.typedBrushName(kind, id) === candidate.name;
                if (matches)
                    return {kind: kind, name: candidate.name || "",
                            serverId: candidate.serverId, displayedServerId: id,
                            category: category, tileset: tileset};
            }
        }
    }
    return null;
}

function options(context, map, tilesets, store) {
    const result = ({});
    if (!context.singleTile)
        return result;
    const brushes = context.brushes || ({});
    for (const kind of ["ground", "wall", "carpet", "table", "doodad", "door"]) {
        let location = locate(brushes[kind], map, tilesets, store, false);
        // Another item in the stack may have a visible brush of the same type.
        if (!location && (kind === "wall" || kind === "carpet" || kind === "table")) {
            for (const candidate of context.collectionBrushes || []) {
                if (candidate.kind === kind)
                    location = locate(candidate, map, tilesets, store, false);
                if (location)
                    break;
            }
        }
        // Loaded ground and wall brushes remain usable without a palette entry.
        // Prefer listed brushes above, then reveal the clicked item's RAW entry.
        if (!location && (kind === "ground" || kind === "wall")) {
            const candidates = [brushes[kind]].concat(context.collectionBrushes || []);
            for (const candidate of candidates) {
                if (!candidate || candidate.kind !== kind || !candidate.serverId
                        || !candidate.name
                        || map.typedBrushName(kind, candidate.serverId) !== candidate.name)
                    continue;
                location = {kind: kind, name: candidate.name,
                            serverId: candidate.serverId, displayedServerId: candidate.serverId,
                            category: "raw", tileset: tilesets.tilesetForItem("raw", candidate.serverId)};
                break;
            }
        }
        if (location)
            result[kind] = location;
    }
    for (const candidate of context.collectionBrushes || []) {
        const location = locate(candidate, map, tilesets, store, true);
        if (location) {
            result.collection = location;
            break;
        }
    }
    return result;
}
