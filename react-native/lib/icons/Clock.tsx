import * as React from 'react';
import { iconWithClassName } from './iconWithClassName';
import { Path, Svg, Circle } from 'react-native-svg';

const Clock = React.forwardRef<
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
    <Circle cx="12" cy="12" r="10" />
    <Path d="M12 6v6l4 2" />
  </Svg>
));

Clock.displayName = 'Clock';

export { Clock };
export default iconWithClassName(Clock);
