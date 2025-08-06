import * as React from 'react';
import { iconWithClassName } from './iconWithClassName';
import { Path, Svg } from 'react-native-svg';

const Loader2 = React.forwardRef<
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
    <Path d="M21 12a9 9 0 1 1-6.219-8.56" />
  </Svg>
));

Loader2.displayName = 'Loader2';

export { Loader2 };
export default iconWithClassName(Loader2);