import type { Plugin } from "vite";

export interface SkyUltraVitePluginOptions {
  /** Manifest path relative to the Vite project root. Defaults to public/manifest.json. */
  manifest?: string;
  /** Inject the native bridge during dev serving. Set false for a standalone SDK mock preview. Defaults to true. */
  devBridge?: boolean;
}

export interface SkyUltraModManifest {
  schemaVersion: 1;
  id: string;
  name: string;
  version: string;
  entry: string;
  /** Pins every manifest-backed view for this mod to the selected browser engine. */
  browserEngine?: "ultralight" | "cef";
  /** Optional owning ESP/ESM/ESL filename for automatic Papyrus quest registration. */
  papyrusPlugin?: string;
  permissions: {
    storage: boolean;
    assetAccess?: boolean;
    networkHosts?: string[];
    pageRequests: string[];
  };
}

/** Validates a schema-version-1 manifest using SkyUltra's native loader rules. */
export declare function validateSkyUltraManifest(
  manifestPath: string,
  projectRoot?: string,
): Promise<SkyUltraModManifest>;

/** Validates manifests and packages page files under Data/SkyUltra/<ModName>/Files.
 * Defaults to readable asset names without hash suffixes. Explicit Rollup naming
 * options remain under the mod author's control. */
export declare function skyultra(options?: SkyUltraVitePluginOptions): Plugin;
