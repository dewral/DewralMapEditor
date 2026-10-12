import QtQuick

// Keep delegates and their sprite images alive when entries enter/leave a view.
ListModel {
    function updateEntries(entries) {
        const desired = {}, occurrences = {}, ordered = [];
        for (const entry of entries) {
            const base = entry.kind + ":" + entry.x + ":" + entry.y;
            const ordinal = occurrences[base] || 0;
            occurrences[base] = ordinal + 1;
            const key = base + ":" + ordinal;
            const row = {entryKey: key, encodedData: JSON.stringify(entry)};
            desired[key] = true; ordered.push(row);
        }
        for (let i = count - 1; i >= 0; --i)
            if (!desired[get(i).entryKey]) remove(i);
        const indexes = {};
        for (let i = 0; i < count; ++i) indexes[get(i).entryKey] = i;
        for (const row of ordered) {
            const index = indexes[row.entryKey];
            if (index === undefined) append(row);
            else if (get(index).encodedData !== row.encodedData)
                setProperty(index, "encodedData", row.encodedData);
        }
    }
}
