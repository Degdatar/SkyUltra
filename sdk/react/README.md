# SkyUltra React SDK

[SDK walkthrough](../../docs/SDK_GUIDE.md) · [Native API](../../api/README.md) · [PrismaUI migration](../../docs/PRISMAUI_MIGRATION.md) · [Magelight UI migration](../../docs/MAGELIGHT_UI_MIGRATION.md) · [Credits and libraries](../../docs/THIRD_PARTY_LIBRARIES.md)

`@skyultra/react` provides React hooks over `@skyultra/sdk`. React is a peer
dependency, so a web mod chooses its own compatible React release. The package
supports React 18.3 and React 19.x.

For reusable themed controls, build and install `@skyultra/ui` as well. Its
React/Vite example lives at `skyrim/samples/react-mod-starter/` and demonstrates
the SDK hooks, UI kit, and preview event injection together.

From `skyrim/sdk/react`, build `@skyultra/sdk` first, then build this package:

```powershell
Set-Location ..\typescript
npm install
npm run build
Set-Location ..\react
npm install
npm run build
```

Add both local packages and React to a mod page's dependencies. For example:

```json
{
  "dependencies": {
    "@skyultra/sdk": "file:../../sdk/typescript",
    "@skyultra/react": "file:../../sdk/react",
    "react": "^19.0.0",
    "react-dom": "^19.0.0"
  }
}
```

Declare the native event and request shapes once, then use the typed client and
hooks inside a React component:

```tsx
import {
  useSkyUltraClient,
  useSkyUltraEvent,
  useSkyUltraRequest,
} from "@skyultra/react";
import { useCallback, useState } from "react";

interface Events {
  inventoryUpdated: { count: number };
}

interface Requests {
  "sample.getStatus": {
    params: { verbose?: boolean };
    response: { ready: boolean };
  };
}

function InventoryStatus() {
  const sky = useSkyUltraClient<Events, Requests>();
  const request = useSkyUltraRequest(sky);
  const [count, setCount] = useState(0);
  const onInventoryUpdated = useCallback(
    (data: Events["inventoryUpdated"]) => setCount(data.count),
    [],
  );

  useSkyUltraEvent(sky, "inventoryUpdated", onInventoryUpdated);

  async function refresh() {
    const status = await request("sample.getStatus", { verbose: true });
    console.info(status.ready);
  }

  return <button onClick={refresh}>Items: {count}</button>;
}
```

The hook subscribes in an effect and removes its listener when the component
unmounts or the client, event name, or handler changes. Memoize handlers with
React's `useCallback` when they don't need to change. `request()` returns the
SDK Promise and accepts its `AbortSignal` options. Native manifest permissions
and page-request allowlists remain required; React and TypeScript do not grant
permissions.
