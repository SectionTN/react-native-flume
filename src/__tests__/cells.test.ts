import { describe, expect, it } from 'vitest';
import { cellKinds, resolveCell } from '../cells';

const worklet = () => ({ type: 'view' as const });
const row = () => null;
const voice = () => null;

describe('resolveCell', () => {
  it('takes reactCells, then renderCell, then renderItem', () => {
    const props = { renderCell: worklet, renderItem: row, reactCells: { voice } };
    expect(resolveCell('voice', props)).toEqual({ kind: 'react', render: voice });
    expect(resolveCell('message', props)).toEqual({ kind: 'worklet' });
    expect(resolveCell('message', { renderItem: row })).toEqual({ kind: 'react', render: row });
  });

  it('reports a type without a renderer', () => {
    expect(resolveCell('x', { reactCells: { voice } })).toEqual({ kind: 'none' });
  });
});

describe('cellKinds', () => {
  it('makes a list without renderCell React by default', () => {
    expect(cellKinds({ reactCells: { b: voice, a: voice } })).toMatchObject({
      reactByDefault: true,
      reactTypes: ['a', 'b'],
    });
    expect(cellKinds({ renderCell: worklet })).toMatchObject({
      reactByDefault: false,
      reactTypes: [],
    });
  });
});
