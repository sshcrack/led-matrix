import * as React from 'react';
import { iconWithClassName } from './iconWithClassName';
import { Path, Svg } from 'react-native-svg';

const Power = React.forwardRef<
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
    <Path d="M12 2v10" />
    <Path d="M18.4 6.6a9 9 0 1 1-12.8 0" />
  </Svg>
));

Power.displayName = 'Power';

export { Power };
export default iconWithClassName(Power);
