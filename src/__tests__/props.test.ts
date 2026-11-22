import { createElement } from 'react';
import type { RefreshControlProps } from 'react-native';
import { describe, expect, it, vi } from 'vitest';
import { contentPadding, nativeProps, refreshOf, resolvePadding, stickyIndicesOf } from '../props';
import type { FlumeProps } from '../types';

// Stands in for React Native's RefreshControl.
const RefreshControlStub = (_props: RefreshControlProps) => null;

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

  it('reports keys that are not padding, gaps or the background', () => {
    expect(contentPadding({ padding: 4, backgroundColor: 'red', margin: 2 }).unsupported).toEqual([
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
    nativeProps({ data: [], contentContainerStyle: { padding: 4, borderWidth: 1 } }, warn);
    expect(warn).toHaveBeenCalledWith(expect.stringContaining('borderWidth'));
  });

  it('names every contentContainerStyle key it lays out when it warns', () => {
    const warn = vi.fn();
    nativeProps(
      {
        data: [],
        contentContainerStyle: {
          padding: 4,
          flexGrow: 1,
          justifyContent: 'center',
          borderWidth: 1,
        },
      },
      warn,
    );
    expect(warn).toHaveBeenCalledWith(
      'contentContainerStyle supports padding, gaps, backgroundColor, flexGrow and justifyContent only; ignored: borderWidth',
    );
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

  it('reads refresh from a RefreshControl element, which wins over onRefresh', () => {
    const fromElement = () => {};
    const element = createElement(RefreshControlStub, {
      refreshing: true,
      onRefresh: fromElement,
      colors: ['red'],
      tintColor: 'blue',
      progressViewOffset: 20,
    });
    expect(refreshOf({ data: [], onRefresh: () => {}, refreshControl: element })).toEqual({
      onRefresh: fromElement,
      refreshing: true,
      colors: ['red'],
      tintColor: 'blue',
      progressViewOffset: 20,
      enabled: true,
    });
  });

  it('keeps refresh off when the RefreshControl element is disabled', () => {
    const element = createElement(RefreshControlStub, {
      refreshing: false,
      onRefresh: () => {},
      enabled: false,
    });
    expect(nativeProps({ data: [], refreshControl: element }, vi.fn()).refreshEnabled).toBe(false);
  });

  it('reads the config object of 0.1 with the top-level props', () => {
    const onRefresh = () => {};
    expect(
      refreshOf({ data: [], onRefresh, refreshing: false, refreshControl: { colors: ['red'] } }),
    ).toEqual({
      onRefresh,
      refreshing: false,
      colors: ['red'],
      progressViewOffset: 0,
      enabled: true,
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

  it('drops the FlatList top threshold on an inverted list, where FlatList measures it at the bottom', () => {
    const flatList = { minIndexForVisible: 0, autoscrollToTopThreshold: 10 };
    const sent = (inverted: boolean) =>
      nativeProps({ data: [], inverted, maintainVisibleContentPosition: flatList }, vi.fn())
        .maintainVisibleContentPosition?.autoscrollToTopThreshold;
    expect(sent(false)).toBe(10);
    expect(sent(true)).toBe(-1);
  });

  it('treats the nullable props of FlatList and FlashList as not given', () => {
    expect(
      nativeProps(
        {
          data: [],
          horizontal: null,
          inverted: null,
          viewabilityConfig: null,
          onEndReachedThreshold: null,
          onStartReachedThreshold: null,
          refreshing: null,
        },
        vi.fn(),
      ),
    ).toMatchObject({
      horizontal: false,
      inverted: false,
      viewabilityConfig: undefined,
      endReachedThreshold: 2,
      startReachedThreshold: 2,
      refreshing: false,
    });
  });

  it('accepts the anchoring shapes of FlatList and LegendList', () => {
    const anchoring = (value: FlumeProps<unknown>['maintainVisibleContentPosition']) =>
      nativeProps({ data: [], maintainVisibleContentPosition: value }, vi.fn())
        .maintainVisibleContentPosition;
    expect(anchoring(false)?.disabled).toBe(true);
    expect(anchoring({ data: false, size: false })?.disabled).toBe(true);
    expect(anchoring({ data: true })?.disabled).toBe(false);
    expect(anchoring(true)).toEqual(anchoring(undefined));
    expect(anchoring(null)).toEqual(anchoring(undefined));
    expect(anchoring({ minIndexForVisible: 0, autoscrollToTopThreshold: 10 })).toMatchObject({
      disabled: false,
      autoscrollToTopThreshold: 10,
    });
    expect(
      anchoring({ minIndexForVisible: 0, autoscrollToTopThreshold: null })
        ?.autoscrollToTopThreshold,
    ).toBe(-1);
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

describe('nativeProps gaps', () => {
  const none = { mainGap: 0, gridRowGap: 0, crossGap: 0, laneInsetStart: 0, laneInsetEnd: 0 };

  it('sends no gaps without container styles', () => {
    expect(nativeProps({ data: [] }, vi.fn())).toMatchObject(none);
  });

  it('spaces the rows of a vertical list with rowGap, then gap, from contentContainerStyle', () => {
    const warn = vi.fn();
    expect(nativeProps({ data: [], contentContainerStyle: { gap: 8 } }, warn)).toMatchObject({
      mainGap: 8,
    });
    expect(
      nativeProps({ data: [], contentContainerStyle: { gap: 8, rowGap: 4, columnGap: 6 } }, warn),
    ).toMatchObject({ mainGap: 4 });
    expect(warn).not.toHaveBeenCalled();
  });

  it('spaces the rows of a horizontal list with columnGap, then gap', () => {
    const warn = vi.fn();
    const horizontal = (contentContainerStyle: object) =>
      nativeProps({ data: [], horizontal: true, contentContainerStyle }, warn);
    expect(horizontal({ gap: 8, columnGap: 6, rowGap: 4 })).toMatchObject({ mainGap: 6 });
    expect(horizontal({ gap: 8 })).toMatchObject({ mainGap: 8 });
    expect(horizontal({ rowGap: 4 })).toMatchObject({ mainGap: 0 });
    expect(warn).not.toHaveBeenCalled();
  });

  it('spaces grid columns and grid rows and insets the lanes from columnWrapperStyle', () => {
    const warn = vi.fn();
    const grid = (columnWrapperStyle: object) =>
      nativeProps({ data: [], numColumns: 3, columnWrapperStyle }, warn);
    expect(grid({ gap: 8, rowGap: 12, paddingHorizontal: 16 })).toMatchObject({
      crossGap: 8,
      gridRowGap: 12,
      laneInsetStart: 16,
      laneInsetEnd: 16,
      mainGap: 0,
    });
    expect(grid({ gap: 8, columnGap: 6 })).toMatchObject({ crossGap: 6, gridRowGap: 0 });
    expect(grid([{ paddingHorizontal: 4 }, { paddingLeft: 10 }, { paddingEnd: 6 }])).toMatchObject({
      laneInsetStart: 10,
      laneInsetEnd: 6,
    });
    expect(grid({ paddingStart: 12, paddingRight: 2 })).toMatchObject({
      laneInsetStart: 12,
      laneInsetEnd: 2,
    });
    expect(warn).not.toHaveBeenCalled();
  });

  it('reads a negative gap as 0, as Yoga does', () => {
    expect(
      nativeProps(
        {
          data: [],
          numColumns: 3,
          contentContainerStyle: { gap: -4 },
          columnWrapperStyle: { gap: -8, rowGap: -2 },
        },
        vi.fn(),
      ),
    ).toMatchObject({ mainGap: 0, crossGap: 0, gridRowGap: 0 });
  });

  it('warns about the columnWrapperStyle keys it does not lay out', () => {
    const warn = vi.fn();
    nativeProps(
      {
        data: [],
        numColumns: 2,
        columnWrapperStyle: { gap: 8, backgroundColor: 'red', paddingTop: 4, padding: 2 },
      },
      warn,
    );
    expect(warn).toHaveBeenCalledTimes(1);
    expect(warn).toHaveBeenCalledWith(
      'columnWrapperStyle supports gap, columnGap, rowGap and horizontal padding only; ignored: backgroundColor, paddingTop, the vertical part of padding',
    );
  });

  it('ignores columnWrapperStyle on one column, which FlatList rejects', () => {
    const warn = vi.fn();
    const one = nativeProps(
      { data: [], horizontal: true, numColumns: 3, columnWrapperStyle: { gap: 8, padding: 4 } },
      warn,
    );
    expect(one).toMatchObject(none);
    expect(warn).toHaveBeenCalledWith(
      'columnWrapperStyle applies to grids only (numColumns above 1); ignored',
    );
  });

  it('takes the gap keys of contentContainerStyle without a warning', () => {
    const warn = vi.fn();
    nativeProps({ data: [], contentContainerStyle: { padding: 4, gap: 8, rowGap: 2 } }, warn);
    expect(warn).not.toHaveBeenCalled();
  });
});

describe('content background and percent padding', () => {
  it('takes backgroundColor and percent padding from contentContainerStyle without a warning', () => {
    const warn = vi.fn();
    const out = nativeProps(
      { data: [], contentContainerStyle: { backgroundColor: 'red', padding: '5%', paddingTop: 8 } },
      warn,
    );
    expect(out).toMatchObject({
      contentBackgroundColor: 'red',
      contentPaddingTop: 8,
      paddingPercentTop: 0,
      contentPaddingLeft: 0,
      paddingPercentLeft: 0.05,
      paddingPercentBottom: 0.05,
      paddingPercentRight: 0.05,
    });
    expect(warn).not.toHaveBeenCalled();
  });

  it('resolves percent padding against the list width on every edge', () => {
    const padding = contentPadding({ padding: '5%', paddingLeft: 12 });
    expect(resolvePadding(padding, 400)).toEqual({ top: 20, left: 12, bottom: 20, right: 20 });
    expect(resolvePadding(padding, -1)).toEqual({ top: 0, left: 12, bottom: 0, right: 0 });
  });

  it('reads a percent with a leading decimal point, as React Native does', () => {
    expect(resolvePadding(contentPadding({ padding: '.5%' }), 400)).toEqual(
      resolvePadding(contentPadding({ padding: '0.5%' }), 400),
    );
    expect(resolvePadding(contentPadding({ padding: '.5%' }), 400).top).toBe(2);
  });

  it('warns about a padding string that is not a percent', () => {
    const warn = vi.fn();
    nativeProps({ data: [], contentContainerStyle: { padding: 'auto' as never } }, warn);
    expect(warn).toHaveBeenCalledWith(expect.stringContaining('padding'));
  });
});

describe('flexGrow and justifyContent', () => {
  it('reads justifyContent only with flexGrow, and a footer with flex: 1 grows', () => {
    const warn = vi.fn();
    const Footer = () => null;
    const grown = nativeProps(
      {
        data: [],
        contentContainerStyle: { flexGrow: 1, justifyContent: 'center' },
        ListFooterComponent: Footer,
        ListFooterComponentStyle: { flex: 1 },
      },
      warn,
    );
    expect(grown).toMatchObject({ contentJustify: 'center', growFooter: true });
    const end = { flexGrow: 1, justifyContent: 'flex-end' } as const;
    expect(nativeProps({ data: [], contentContainerStyle: end }, warn)).toMatchObject({
      contentJustify: 'end',
      growFooter: false,
    });
    const still = { justifyContent: 'center' } as const;
    expect(nativeProps({ data: [], contentContainerStyle: still }, warn)).toMatchObject({
      contentJustify: 'start',
    });
    expect(warn).not.toHaveBeenCalled();
  });

  it('lets an explicit flexGrow on the footer win over flex, as Yoga does', () => {
    const Footer = () => null;
    const footer = (ListFooterComponentStyle: object) =>
      nativeProps(
        {
          data: [],
          contentContainerStyle: { flexGrow: 1 },
          ListFooterComponent: Footer,
          ListFooterComponentStyle,
        },
        vi.fn(),
      ).growFooter;
    expect(footer({ flex: 1, flexGrow: 0 })).toBe(false);
    expect(footer({ flexGrow: 1 })).toBe(true);
    expect(footer({ flex: 1 })).toBe(true);
  });

  it('still warns about alignItems and about a spacing justifyContent', () => {
    const warn = vi.fn();
    nativeProps({ data: [], contentContainerStyle: { flexGrow: 1, alignItems: 'center' } }, warn);
    expect(warn).toHaveBeenCalledWith(expect.stringContaining('alignItems'));
    const spaced = { flexGrow: 1, justifyContent: 'space-between' } as const;
    nativeProps({ data: [], contentContainerStyle: spaced }, warn);
    expect(warn).toHaveBeenCalledWith(expect.stringContaining('justifyContent'));
  });
});
