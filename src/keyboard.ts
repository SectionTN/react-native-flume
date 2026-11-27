import { useEffect, useRef } from 'react';
import { Keyboard, type KeyboardEvent, type KeyboardEventName } from 'react-native';

// Subscribes only while a handler is given, and calls the latest one without subscribing again.
export function useKeyboardEvent(
  name: KeyboardEventName,
  handler: ((event: KeyboardEvent) => void) | null | undefined,
): void {
  const latest = useRef(handler);
  latest.current = handler;
  const given = handler != null;
  useEffect(() => {
    if (!given) return undefined;
    const subscription = Keyboard.addListener(name, (event) => latest.current?.(event));
    return () => subscription.remove();
  }, [name, given]);
}
