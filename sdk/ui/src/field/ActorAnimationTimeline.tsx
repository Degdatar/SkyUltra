import * as React from 'react';
import type { SkyUltraActorAnimationClip } from '@skyultra/sdk';

export function ActorAnimationTimeline({clip,frame,busy,onFrameChange}: {
  clip?: SkyUltraActorAnimationClip;
  frame: number;
  busy: boolean;
  onFrameChange(frame: number): void;
}): React.JSX.Element {
  const [scrubbing, setScrubbing] = React.useState(false);
  const maxFrame = clip ? Math.max(1, clip.sampleCount - 1) : 32;
  const currentFrame = clip ? Math.max(0, Math.min(maxFrame, frame)) : 0;
  const progress = maxFrame > 0 ? Math.max(0, Math.min(100, (currentFrame / maxFrame) * 100)) : 0;
  const seconds = clip ? ((clip.duration * currentFrame) / maxFrame).toFixed(2) : '0.00';
  const label = clip ? 'Animation frame · ' + seconds + ' s' : 'Animation frame';
  const isDisabled = !clip || busy;
  return (
    <div
      className={
        'studio-timeline studio-scrubber' +
        (clip ? ' is-ready' : ' is-empty') +
        (scrubbing ? ' is-scrubbing' : '') +
        (isDisabled ? ' is-disabled' : '')
      }
      title={clip ? `${label} (frame ${currentFrame} / ${maxFrame})` : 'Load poses or capture animations, then choose a pose to select a frame.'}
    >
      <span className="studio-scrubber__frames" aria-hidden="true">
        {currentFrame}<em>/{maxFrame}</em>
      </span>
      <div className="studio-scrubber__track">
        <div className="studio-scrubber__fill" style={{ width: `${progress}%` }} />
        <span className="studio-scrubber__thumb" style={{ left: `${progress}%` }} />
        <input
          type="range"
          className="studio-scrubber__input"
          aria-label={label}
          min={0}
          max={maxFrame}
          step={1}
          value={currentFrame}
          disabled={isDisabled}
          onPointerDown={() => setScrubbing(true)}
          onPointerUp={() => setScrubbing(false)}
          onPointerCancel={() => setScrubbing(false)}
          onChange={event => onFrameChange(Number(event.target.value))}
        />
      </div>
      <span className="studio-scrubber__meta">
        {clip ? `${seconds} s` : 'Load poses or capture animations'}
      </span>
    </div>
  );
}
