import * as React from 'react';
import type { SkyWebViewAPI } from '@skyultra/sdk';
import { ModelStage } from './ActorStage.js';
import type { Item } from './model.js';

/** Native world-model inspector with camera, lighting and transparent PNG export. */
export function ItemStage({item,bridge,obscured=false,onCapturePortrait}: {item:Item;bridge:SkyWebViewAPI;obscured?:boolean;onCapturePortrait?:()=>Promise<string|void>|string|void}): React.JSX.Element {
  return <ModelStage subject={{kind:'item',item}} bridge={bridge} obscured={obscured} onCapturePortrait={onCapturePortrait} />;
}
