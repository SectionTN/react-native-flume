import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    include: ['src/**/__tests__/**/*.test.{ts,tsx}', 'example/benchmark/**/*.test.mts'],
    environment: 'node',
    passWithNoTests: true,
  },
});
