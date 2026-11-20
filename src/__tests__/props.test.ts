import { describe, expect, it, vi } from 'vitest';
import { contentPadding, nativeProps, stickyIndicesOf } from '../props';

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

describe('stickyIndicesOf', () => {
  it('sorts and dedups sticky header indices', () => {
    expect(stickyIndicesOf([8, 0, 4, 4])).toEqual([0, 4, 8]);
  });

  it('drops negative sticky indices', () => {
    expect(stickyIndicesOf([-1, 2, 0])).toEqual([0, 2]);
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
  it('treats null data and null callbacks as not given', () => {
    expect(nativeProps({ data: null, onRefresh: null, onScroll: null }, vi.fn())).toMatchObject({
      refreshEnabled: false,
      emitScrollEvents: false,
    });
  });

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

  it('keeps a horizontal list from bouncing vertically unless asked', () => {
    const warn = vi.fn();
    expect(nativeProps({ data: [], horizontal: true }, warn).alwaysBounceVertical).toBe(false);
    expect(nativeProps({ data: [] }, warn).alwaysBounceVertical).toBe(true);
    expect(nativeProps({ data: [], alwaysBounceVertical: false }, warn).alwaysBounceVertical).toBe(
      false,
    );
  });

  it('passes the interactive keyboard dismiss mode through', () => {
    const warn = vi.fn();
    expect(
      nativeProps({ data: [], keyboardDismissMode: 'interactive' }, warn).keyboardDismissMode,
    ).toBe('interactive');
  });
});

describe('nativeProps anchoring', () => {
  it('sends the anchoring config complete, so an unset threshold never reads as 0', () => {
    const warn = vi.fn();
    expect(nativeProps({ data: [] }, warn).maintainVisibleContentPosition).toEqual({
      disabled: false,
      autoscrollToTopThreshold: -1,
      autoscrollToBottomThreshold: -1,
      animateAutoScrollToBottom: true,
      startRenderingFromBottom: false,
    });
    expect(
      nativeProps(
        { data: [], maintainVisibleContentPosition: { autoscrollToBottomThreshold: 0 } },
        warn,
      ).maintainVisibleContentPosition?.autoscrollToBottomThreshold,
    ).toBe(0);
  });
});

describe('nativeProps estimates', () => {
  it('passes estimatedItemSize, -1 when absent', () => {
    const warn = vi.fn();
    expect(nativeProps({ data: [], estimatedItemSize: 72 }, warn).estimatedItemSize).toBe(72);
    expect(nativeProps({ data: [] }, warn).estimatedItemSize).toBe(-1);
  });
});

describe('nativeProps list components', () => {
  it('maps the list components to flags', () => {
    const warn = vi.fn();
    const Header = () => null;
    const flags = nativeProps(
      { data: [], ListHeaderComponent: Header, ListEmptyComponent: Header },
      warn,
    );
    expect(flags).toMatchObject({ listHeader: true, listFooter: false, listEmpty: true });
    expect(nativeProps({ data: [] }, warn)).toMatchObject({ listHeader: false, listEmpty: false });
  });
});
