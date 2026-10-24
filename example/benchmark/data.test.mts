import { expect, it } from 'vitest';
import { changeChats, makeBenchChats, makeFeed } from '../src/bench/data.ts';

it('builds the same chronological chat every time, every fifth message a voice note', () => {
  const chats = makeBenchChats(10);
  expect(chats).toEqual(makeBenchChats(10));
  expect(chats.map((chat) => chat.id)).toEqual(Array.from({ length: 10 }, (_, i) => `c0-${i}`));
  expect(chats.filter((chat) => chat.kind === 'voice').map((chat) => chat.id)).toEqual([
    'c0-4',
    'c0-9',
  ]);
});

it('changes the newest items and keeps their ids', () => {
  const chats = makeBenchChats(1000);
  const changed = changeChats(chats, 500);
  expect(changed[0]).toBe(chats[0]);
  expect(changed[999]).not.toBe(chats[999]);
  expect(changed[999]?.id).toBe(chats[999]?.id);
  expect(changed.filter((chat, i) => chat !== chats[i])).toHaveLength(500);
});

it('puts a photo on every other post', () => {
  expect(makeFeed(4).map((post) => post.photo !== undefined)).toEqual([true, false, true, false]);
});
