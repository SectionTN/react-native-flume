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

describe('nativeProps sticky headers', () => {
  it('sorts and dedups sticky header indices', () => {
    const warn = vi.fn();
    expect(
      nativeProps({ data: [], stickyHeaderIndices: [8, 0, 4, 4] }, warn).stickyHeaderIndices,
    ).toEqual([0, 4, 8]);
  });
});

describe('nativeProps scroll events', () => {
  it('enables scroll events only with a handler', () => {
    const warn = vi.fn();
    expect(nativeProps({ data: [] }, warn).emitScrollEvents).toBe(false);
    expect(nativeProps({ data: [], onMomentumScrollEnd: () => {} }, warn)).toMatchObject({
      emitScrollEvents: true,
      scrollEventThrottle: 16,
    });
  });
});

describe('nativeProps viewability', () => {
  it('warns when both viewability thresholds are set', () => {
    const warn = vi.fn();
    expect(
      nativeProps(
        {
          data: [],
          onViewableItemsChanged: () => {},
          viewabilityConfig: {
            itemVisiblePercentThreshold: 50,
            viewAreaCoveragePercentThreshold: 10,
          },
        },
        warn,
      ),
    ).toMatchObject({
      emitViewabilityEvents: true,
      viewabilityConfig: { itemVisiblePercentThreshold: 50 },
    });
    expect(warn).toHaveBeenCalledWith(expect.stringContaining('exactly one'));
  });
});

describe('nativeProps refresh', () => {
  it('enables refresh only with onRefresh', () => {
    const warn = vi.fn();
    expect(nativeProps({ data: [], refreshing: true }, warn).refreshEnabled).toBe(false);
    expect(
      nativeProps(
        {
          data: [],
          onRefresh: () => {},
          refreshControl: { colors: ['#f00'], progressViewOffset: 20 },
        },
        warn,
      ),
    ).toMatchObject({
      refreshEnabled: true,
      refreshColors: ['#f00'],
      refreshProgressViewOffset: 20,
    });
  });
});

describe('nativeProps scroll props', () => {
  it('maps the scroll props', () => {
    const warn = vi.fn();
    expect(
      nativeProps(
        {
          data: [],
          decelerationRate: 'fast',
          keyboardShouldPersistTaps: 'handled',
          onPress: () => {},
        },
        warn,
      ),
    ).toMatchObject({
      decelerationRate: 0.99,
      keyboardShouldPersistTaps: 'handled',
      rowsPressable: true,
    });
    expect(nativeProps({ data: [], decelerationRate: 0.995 }, warn)).toMatchObject({
      decelerationRate: 0.995,
      rowsPressable: false,
    });
  });
});

describe('nativeProps anchoring', () => {
  it('passes maintainVisibleContentPosition through', () => {
    const warn = vi.fn();
    expect(
      nativeProps(
        { data: [], maintainVisibleContentPosition: { autoscrollToBottomThreshold: 0.2 } },
        warn,
      ).maintainVisibleContentPosition,
    ).toEqual({ autoscrollToBottomThreshold: 0.2 });
  });
});

describe('nativeProps estimates', () => {
  it('passes estimatedItemSize, -1 when absent', () => {
    const warn = vi.fn();
    expect(nativeProps({ data: [], estimatedItemSize: 72 }, warn).estimatedItemSize).toBe(72);
    expect(nativeProps({ data: [] }, warn).estimatedItemSize).toBe(-1);
  });
});
