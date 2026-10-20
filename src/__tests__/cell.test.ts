import { describe, expect, it } from 'vitest';
import { cell } from '../cell';

describe('cell builders', () => {
  it('view carries style and children', () => {
    const node = cell.view({ flexDirection: 'row', padding: 12 }, [cell.text('a')]);
    expect(node).toEqual({
      type: 'view',
      style: { flexDirection: 'row', padding: 12 },
      children: [{ type: 'text', text: 'a' }],
    });
  });

  it('text keeps options beside the style', () => {
    expect(cell.text('hi', { fontSize: 16 }, { allowFontScaling: false })).toEqual({
      type: 'text',
      text: 'hi',
      style: { fontSize: 16 },
      allowFontScaling: false,
    });
  });

  it('image separates source, resizeMode and tintColor from the style', () => {
    expect(
      cell.image({
        source: 'https://x/a.png',
        width: 40,
        height: 40,
        borderRadius: 20,
        resizeMode: 'cover',
      }),
    ).toEqual({
      type: 'image',
      source: 'https://x/a.png',
      resizeMode: 'cover',
      style: { width: 40, height: 40, borderRadius: 20 },
    });
  });

  it('pressable separates id and pressedOpacity from the style', () => {
    expect(cell.pressable({ id: 'like', padding: 8 }, [cell.text('Like')])).toEqual({
      type: 'pressable',
      id: 'like',
      style: { padding: 8 },
      children: [{ type: 'text', text: 'Like' }],
    });
  });

  it('spacer is a flex view or a fixed view', () => {
    expect(cell.spacer()).toEqual({ type: 'view', style: { flex: 1 } });
    expect(cell.spacer(8)).toEqual({ type: 'view', style: { flexBasis: 8 } });
  });
});
