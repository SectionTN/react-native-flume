import { describe, expect, it, vi } from 'vitest';
import { contentPadding, nativeProps } from '../props';

describe('contentPadding', () => {
  it('resolves the padding shorthands like React Native', () => {
    expect(contentPadding({ padding: 4, paddingHorizontal: 8, paddingLeft: 12 })).toMatchObject({
      top: 4,
      left: 12,
      bottom: 4,
      right: 8,
    });
  });

  it('flattens style arrays and maps start and end', () => {
    expect(contentPadding([{ padding: 4 }, { paddingStart: 10, paddingEnd: 6 }])).toMatchObject({
      left: 10,
      right: 6,
    });
  });

  it('reports keys that are not padding', () => {
    expect(contentPadding({ padding: 4, backgroundColor: 'red', margin: 2 }).unsupported).toEqual([
      'backgroundColor',
      'margin',
    ]);
  });
});

describe('nativeProps', () => {
  it('maps the layout props and warns on a horizontal grid', () => {
    const warn = vi.fn();
    const out = nativeProps(
      {
        data: [],
        horizontal: true,
        numColumns: 2,
        inverted: true,
        contentContainerStyle: { paddingTop: 8 },
        drawDistance: 500,
        initialScrollIndex: 3,
      },
      warn,
    );
    expect(out).toMatchObject({
      horizontal: true,
      numColumns: 1,
      inverted: true,
      contentPaddingTop: 8,
      drawDistance: 500,
      initialScrollIndex: 3,
    });
    expect(warn).toHaveBeenCalledWith(
      expect.stringContaining('numColumns does not support horizontal'),
    );
  });

  it('warns once about non-padding keys in contentContainerStyle', () => {
    const warn = vi.fn();
    nativeProps({ data: [], contentContainerStyle: { padding: 4, backgroundColor: 'red' } }, warn);
    expect(warn).toHaveBeenCalledWith(expect.stringContaining('backgroundColor'));
  });
});

describe('nativeProps separator', () => {
  it('maps separator to flat props', () => {
    const warn = vi.fn();
    expect(
      nativeProps({ data: [], separator: { size: 1, color: '#ddd', insetStart: 72 } }, warn),
    ).toMatchObject({
      separatorSize: 1,
      separatorColor: '#ddd',
      separatorInsetStart: 72,
      separatorInsetEnd: 0,
    });
  });
});
