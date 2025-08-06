import * as React from 'react';
import { iconWithClassName } from './iconWithClassName';
import { Path, Svg } from 'react-native-svg';

const ShoppingBag = React.forwardRef<
  React.ElementRef<typeof Svg>,
  React.ComponentPropsWithoutRef<typeof Svg>
>(({ className, ...props }, ref) => (
  <Svg
    ref={ref}
    width={24}
    height={24}
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    strokeWidth={2}
    strokeLinecap="round"
    strokeLinejoin="round"
    className={className}
    {...props}
  >
    <Path d="M6 2 3 6v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V6l-3-4Z" />
    <Path d="M3 6h18" />
    <Path d="M16 10a4 4 0 0 1-8 0" />
  </Svg>
));

ShoppingBag.displayName = 'ShoppingBag';

export { ShoppingBag };
export default iconWithClassName(ShoppingBag);