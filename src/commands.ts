export type ScrollCommands = {
  start(): { id: number; done: Promise<void> };
  // A command sent again takes a new id and keeps its Promise; with none pending the id is just fresh.
  move(): number;
  endThrough(id: number): void;
  endAll(): void;
};

// Native ends commands in order and a newer one interrupts the one before, so one Promise is pending at most.
export function createScrollCommands(): ScrollCommands {
  let last = 0;
  let pending: { id: number; resolve: () => void } | null = null;
  const endPending = () => {
    pending?.resolve();
    pending = null;
  };
  return {
    start() {
      endPending();
      const id = ++last;
      const done = new Promise<void>((resolve) => {
        pending = { id, resolve };
      });
      return { id, done };
    },
    move() {
      last += 1;
      if (pending !== null) pending.id = last;
      return last;
    },
    endThrough(id) {
      if (pending !== null && pending.id <= id) endPending();
    },
    endAll: endPending,
  };
}
