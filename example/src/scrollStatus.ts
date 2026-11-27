import { useCallback, useRef, useState } from 'react';

type Status = { name: string; ms: number | null; open: number };

// The last scroll command, when its Promise settled, and how many scroll Promises are still open.
export function useScrollStatus(): [string, (name: string, scroll?: Promise<void>) => void] {
  const [status, setStatus] = useState<Status | null>(null);
  const latest = useRef(0);
  const open = useRef(0);
  const track = useCallback((name: string, scroll?: Promise<void>) => {
    if (scroll === undefined) return;
    const id = ++latest.current;
    const start = Date.now();
    open.current += 1;
    setStatus({ name, ms: null, open: open.current });
    scroll.then(() => {
      open.current -= 1;
      const ms = Date.now() - start;
      setStatus((current) =>
        id === latest.current || current === null
          ? { name, ms, open: open.current }
          : { ...current, open: open.current },
      );
    });
  }, []);
  const line =
    status === null
      ? 'no scroll yet'
      : `${status.name} ${status.ms === null ? 'waiting' : `settled in ${status.ms} ms`}, ${status.open} open`;
  return [line, track];
}
