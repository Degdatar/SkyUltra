# Manifest scoped mod sample

[SDK walkthrough](../../../../docs/SDK_GUIDE.md) · [Native API](../../../../api/README.md) · [PrismaUI migration](../../../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../../../docs/THIRD_PARTY_LIBRARIES.md)

Copy this directory to:

```text
Data/SkyUltra/Degdatar/
```

Keep `manifest.json` at the mod root and install the page under `Files/`:
`Data/SkyUltra/Degdatar/manifest.json` and
`Data/SkyUltra/Degdatar/Files/index.html`. The Papyrus sample embeds its CSS
and JavaScript bridge in the HTML so its presentation does not depend on
loading separate `file://` subresources. The `styles.css` and
`SkyUltraBridge.js` files remain alongside the page as reference sources.

The manifest ID is `degdatar`. Its `papyrusPlugin` value must be the filename
of the ESP/ESM/ESL that owns the quest script. Papyrus can then register the
matching mod automatically with `SkyUltraNative.RegisterModForQuest(Self)`.
The top-level `browserEngine` value pins every view created for this mod to
Ultralight. Set it to `"cef"` to use CEF instead. If omitted, Ultralight is the
default. Mods pinned to different engines can run at the same time because the
standard SkyUltra build starts both engines. Host settings do not override a
mod's manifest choice.

After SkyUltra reports that its native API is ready, native plugins can call
`SkyUltra_GetAPI_V3()`, register the ID declared in `manifest.json`, and create
the view with `create_mod_view`. The view config's `initial_url` must be empty;
SkyUltra loads the manifest's `entry` file. Declare only the native page
requests that the page needs. The host rejects per-view allowlist entries that
are absent from `permissions.pageRequests`.
The example page expects the native plug-in to register a request callback for
`sample.getStatus`. It opens edge-to-edge using the current game viewport.
Papyrus-created views are opaque by default, and the embedded styles provide a
dark background and disable text selection. The bridge status changes to a
timeout message if the Papyrus quest does not answer within eight seconds.

In game, clicking the page gives it keyboard focus. Press **Escape** to hide the
browser overlay and return mouse and keyboard control to Skyrim. **Shift+F6**
also toggles the overlay and can reopen the Papyrus page after hiding it.

`permissions.networkHosts` is an exact ASCII hostname allowlist for HTTP and
HTTPS resource requests. It does not accept wildcards or URL paths. The permission
applies to every port on that exact hostname; an empty list denies remote
requests in CEF and in Ultralight editions that support the network callback.
Credentialed URLs are rejected by those network handlers. Add only the hosts the page
needs. In CEF, local file requests must resolve under this mod's directory.
The manifest entry file is still
allowed when `assetAccess` is false; other local assets are blocked.
`assetAccess` defaults to true when omitted so older schema-version-1 mods can
continue loading packaged CSS, scripts, and images. Ultralight pages receive a
private virtual file path whose files resolve only inside the registered mod
directory. With `assetAccess` false, the manifest entry page remains available
and sibling assets are blocked.

Ultralight's network callback is available only in editions that enable that
callback, so network-host enforcement may differ by Ultralight edition. The
Ultralight local-file confinement is implemented in source and awaits build
and in-game validation. Neither backend's content permissions sandbox native
SKSE plugins.

When `storage` is enabled, `get_mod_storage_directory` returns a dedicated
directory under `%LOCALAPPDATA%/SkyUltra/Mods/<id>/Storage`. When storage is
disabled, that call returns `SKYULTRA_UNSUPPORTED`.

This manifest is a web-content registration contract, not a sandbox for native
SKSE DLLs. Native plugins share Skyrim's process and can claim another mod ID.
