/** Shared linear magnitude domain for comparable game statistics, including modded values. */
export function skyStatMaximum(values: readonly number[], floor = 100): number {
  const peak = Math.max(Number.isFinite(floor) && floor > 0 ? floor : 1, ...values.filter(Number.isFinite).map(Math.abs));
  const step = 10 ** Math.floor(Math.log10(peak));
  return Math.ceil(peak / step) * step;
}
export function skyStatFraction(value: number, maximum: number): number {
  return Number.isFinite(value) && Number.isFinite(maximum) && maximum > 0 ? Math.min(1, Math.abs(value) / maximum) : 0;
}
