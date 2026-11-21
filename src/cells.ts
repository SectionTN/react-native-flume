import { type ComponentType, createElement, isValidElement, type ReactElement } from 'react';
import type { SettledProps } from './props';
import type { CellInfo, ReactCell } from './types';

// FlatList's ListItemComponent: a component takes the info as props, an element renders as given.
export function itemComponentCell<T>(
  component: ComponentType<CellInfo<T>> | ReactElement,
): ReactCell<T> {
  return isValidElement(component) ? () => component : (info) => createElement(component, info);
}

export type ResolvedCell<T> =
  | { kind: 'worklet' }
  | { kind: 'react'; render: ReactCell<T> }
  | { kind: 'none' };

export type CellKinds = { reactByDefault: boolean; reactTypes: string[]; signature: string };

type Renderers<T> = Pick<SettledProps<T>, 'renderCell' | 'renderItem' | 'reactCells'>;

function reactCellFor<T>(reactCells: Renderers<T>['reactCells'], type: string) {
  return reactCells !== undefined && Object.hasOwn(reactCells, type) ? reactCells[type] : undefined;
}

// The spec's order: the type's reactCells entry, then the worklet, then renderItem.
export function resolveCell<T>(type: string, props: Renderers<T>): ResolvedCell<T> {
  const react = reactCellFor(props.reactCells, type);
  if (react !== undefined) return { kind: 'react', render: react };
  if (props.renderCell !== undefined) return { kind: 'worklet' };
  if (props.renderItem !== undefined) return { kind: 'react', render: props.renderItem };
  return { kind: 'none' };
}

// What native needs to give each type a holder of the right kind; the signature keys memos on it.
export function cellKinds<T>(props: Pick<SettledProps<T>, 'renderCell' | 'reactCells'>): CellKinds {
  const reactCells = props.reactCells ?? {};
  const reactTypes = Object.keys(reactCells)
    .filter((type) => reactCells[type] !== undefined)
    .sort();
  const reactByDefault = props.renderCell === undefined;
  return {
    reactByDefault,
    reactTypes,
    signature: `${reactByDefault} ${JSON.stringify(reactTypes)}`,
  };
}
