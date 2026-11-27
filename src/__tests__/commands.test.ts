import { describe, expect, it } from 'vitest';
import { createScrollCommands } from '../commands';

// Settles a Promise's callbacks queued so far, so a test can read whether it resolved.
const flush = () => new Promise<void>((resolve) => setTimeout(resolve, 0));

function track(done: Promise<void>) {
  const state = { resolved: false, rejected: false };
  done.then(
    () => {
      state.resolved = true;
    },
    () => {
      state.rejected = true;
    },
  );
  return state;
}

describe('createScrollCommands', () => {
  it('resolves a command when its end arrives', async () => {
    const commands = createScrollCommands();
    const { id, done } = commands.start();
    const state = track(done);
    await flush();
    expect(state.resolved).toBe(false);
    commands.endThrough(id);
    await flush();
    expect(state.resolved).toBe(true);
  });

  it('resolves the older command when a newer one starts', async () => {
    const commands = createScrollCommands();
    const first = track(commands.start().done);
    const second = track(commands.start().done);
    await flush();
    expect(first.resolved).toBe(true);
    expect(second.resolved).toBe(false);
  });

  it('keeps the newer command pending when an older end arrives late', async () => {
    const commands = createScrollCommands();
    const first = commands.start();
    const second = track(commands.start().done);
    commands.endThrough(first.id);
    await flush();
    expect(second.resolved).toBe(false);
  });

  it('resolves everything on endAll', async () => {
    const commands = createScrollCommands();
    const state = track(commands.start().done);
    commands.endAll();
    await flush();
    expect(state.resolved).toBe(true);
  });

  it('never rejects', async () => {
    const commands = createScrollCommands();
    const states = [track(commands.start().done), track(commands.start().done)];
    commands.endThrough(99);
    commands.endAll();
    await flush();
    expect(states.every((state) => state.resolved && !state.rejected)).toBe(true);
  });

  it('keeps a moved command pending when its old id ends', async () => {
    const commands = createScrollCommands();
    const { id, done } = commands.start();
    const state = track(done);
    const moved = commands.move();
    commands.endThrough(id);
    await flush();
    expect(moved).toBeGreaterThan(id);
    expect(state.resolved).toBe(false);
  });

  it('resolves a moved command when its new id ends', async () => {
    const commands = createScrollCommands();
    const state = track(commands.start().done);
    commands.endThrough(commands.move());
    await flush();
    expect(state.resolved).toBe(true);
  });

  it('gives a fresh id when nothing is pending', () => {
    const commands = createScrollCommands();
    const { id } = commands.start();
    commands.endThrough(id);
    const moved = commands.move();
    const next = commands.start();
    expect(moved).toBeGreaterThan(id);
    expect(next.id).toBeGreaterThan(moved);
  });

  it('keeps working after endAll', async () => {
    const commands = createScrollCommands();
    commands.endAll();
    const { id, done } = commands.start();
    const state = track(done);
    commands.endThrough(id);
    await flush();
    expect(state.resolved).toBe(true);
  });
});
