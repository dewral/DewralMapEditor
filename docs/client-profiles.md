# Startup and client profiles

The Tibia UI welcome window separates map actions and client configuration. Recent maps can be filtered by name or path, opened with Enter/double click, and removed from the list without deleting the map.

Use **Manage clients** on the welcome screen or **Map > Client profile > Manage clients** in the editor. Profiles have a stable ID independent of their display name. Existing numeric/custom profiles and map assignments migrate on first load; legacy custom resource directory names are retained separately.

Blank file fields use automatic detection. Explicit paths can point to different directories. DAT, SPR and OTB are required. XML and OTFI can be disabled individually. Multiple candidates require an explicit choice. OTFI paths/flags and manual format overrides use the same resolver during validation and loading.

**Validate files** parses a temporary set of readers asynchronously. It never publishes data into active models. Save persists the draft; changing or deleting a saved profile does not replace an open document's configuration snapshot. Missing/deleted map profiles prompt for a replacement.

Verification: `client_load_service_test`, QML `tst_client_profile.qml`, `tst_startup_profiles.qml`, `tst_startup_window.qml`, and native `--smoke-test --smoke-documents` with real 7.72 and 10.98 assets. Native diagnostics `--smoke-test --smoke-client-profiles --document-client <folder>` capture both windows using isolated settings.
