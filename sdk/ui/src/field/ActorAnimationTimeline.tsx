import * as React from 'react';
import type { SkyUltraActorAnimationClip } from '@skyultra/sdk';
import { SkyRangeSlider } from '../core.js';

export function ActorAnimationTimeline({clip,frame,busy,onFrameChange}: {
  clip?: SkyUltraActorAnimationClip;
  frame: number;
  busy: boolean;
  onFrameChange(frame: number): void;
}): React.JSX.Element {
  const label = clip ? 'Animation frame · ' + (clip.duration * frame / (clip.sampleCount - 1)).toFixed(2) + ' s' : 'Animation frame';
  return <div className="studio-timeline">
    <SkyRangeSlider label={label} value={clip ? frame : 0} min={0} max={clip ? clip.sampleCount - 1 : 32} step={1} onValueChange={onFrameChange} disabled={!clip || busy} />
    {!clip && <p className="studio-note">Load poses or capture animations, then choose a pose to select a frame.</p>}
  </div>;
}
