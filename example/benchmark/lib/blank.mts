// Share of rgb24 pixels whose three channels are each within tolerance of the sentinel.
export function blankFraction(
  rgb: Uint8Array,
  sentinel: readonly [number, number, number],
  tolerance: number,
): number {
  const pixels = Math.floor(rgb.length / 3);
  if (pixels === 0) return 0;
  const [r, g, b] = sentinel;
  let hits = 0;
  for (let i = 0; i < pixels * 3; i += 3) {
    if (
      Math.abs((rgb[i] ?? 0) - r) <= tolerance &&
      Math.abs((rgb[i + 1] ?? 0) - g) <= tolerance &&
      Math.abs((rgb[i + 2] ?? 0) - b) <= tolerance
    ) {
      hits++;
    }
  }
  return hits / pixels;
}

export type BlankSummary = {
  frames: number;
  meanPercent: number;
  maxPercent: number;
  framesOver: number;
};

const oneDecimal = (value: number) => Math.round(value * 10) / 10;

export function summarizeBlank(fractions: number[], threshold: number): BlankSummary {
  const frames = fractions.length;
  const sum = fractions.reduce((total, f) => total + f, 0);
  return {
    frames,
    meanPercent: frames === 0 ? 0 : oneDecimal((sum / frames) * 100),
    maxPercent: oneDecimal(Math.max(0, ...fractions) * 100),
    framesOver: fractions.filter((f) => f > threshold).length,
  };
}

// The list rect [left, top, right, bottom] in screen px, scaled to the video and shrunk to multiples of 4.
export function cropFor(
  rect: [number, number, number, number],
  screen: [number, number],
  video: [number, number],
): [number, number, number, number] {
  const sx = video[0] / screen[0];
  const sy = video[1] / screen[1];
  const up = (value: number) => Math.ceil(value / 4) * 4;
  const down = (value: number) => Math.floor(value / 4) * 4;
  const clamp = (value: number, max: number) => Math.min(Math.max(value, 0), max);
  return [
    clamp(up(rect[0] * sx), video[0]),
    clamp(up(rect[1] * sy), video[1]),
    clamp(down(rect[2] * sx), video[0]),
    clamp(down(rect[3] * sy), video[1]),
  ];
}
