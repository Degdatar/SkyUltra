export interface ActorCamera { yaw: number; pitch: number; zoom: number; panX: number; panY: number; }
const TWO_PI = Math.PI * 2;
export function wrapAngleRadians(angle: number): number {
  if (!Number.isFinite(angle)) return 0;
  let wrapped = angle % TWO_PI;
  if (wrapped > Math.PI) wrapped -= TWO_PI;
  else if (wrapped < -Math.PI) wrapped += TWO_PI;
  return wrapped;
}
export function defaultActorCamera(): ActorCamera { return { yaw: 0, pitch: 0, zoom: 1, panX: 0, panY: 0 }; }
/** Screen-relative pan keeps dragging intuitive at every zoom and aspect ratio. */
export function dragActorCamera(camera: ActorCamera, mode: 'rotate' | 'pan', dx: number, dy: number, width: number, height: number): ActorCamera {
  if (![dx, dy, width, height].every(Number.isFinite) || width <= 0 || height <= 0) return camera;
  return mode === 'pan'
    ? { ...camera, panX: Math.max(-2, Math.min(2, camera.panX + dx / width)), panY: Math.max(-2, Math.min(2, camera.panY + dy / height)) }
    : { ...camera, yaw: wrapAngleRadians(camera.yaw + dx * .008), pitch: wrapAngleRadians(camera.pitch + dy * .008) };
}
