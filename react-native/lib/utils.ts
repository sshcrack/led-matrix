import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}
export const titleCase = (s: string) =>
  s.replace(/^_*(.)|_+(.)/g, (s, c, d) => c ? c.toUpperCase() : ' ' + d.toUpperCase())

export type ReactSetState<T> = React.Dispatch<React.SetStateAction<T>>


export const getImageUrl = (apiUrl: string, url: string) => apiUrl  + `/image?url=${encodeURIComponent(url)}`
