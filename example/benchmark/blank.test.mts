import { expect, it } from 'vitest';
import { blankFraction, cropFor, summarizeBlank } from './lib/blank.mts';

it('counts pixels within the tolerance', () => {
  const frame = Uint8Array.from([255, 0, 255, 250, 6, 250, 200, 60, 200, 255, 255, 255]);
  expect(blankFraction(frame, [255, 0, 255], 24)).toBe(0.5);
  expect(summarizeBlank([0, 0.004, 0.02, 1], 0.01)).toEqual({
    frames: 4,
    meanPercent: 25.6,
    maxPercent: 100,
    framesOver: 2,
  });
});

it('scales the list rect to the video and rounds it to multiples of 4', () => {
  expect(cropFor([0, 325, 1080, 1555], [1080, 2424], [810, 1818])).toEqual([0, 244, 808, 1164]);
});
