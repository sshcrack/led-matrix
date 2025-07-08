import * as React from 'react';
import { iconWithClassName } from './iconWithClassName';
import { Path, Svg } from 'react-native-svg';

const Calendar = React.forwardRef<
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
    <Path d="M21 10V6a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v4" />
    <Path d="M21 10v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V10" />
    <Path d="M3 10h18" />
    <Path d="M7 2v4" />
    <Path d="M17 2v4" />
  </Svg>
));

Calendar.displayName = 'Calendar';

export { Calendar };
export default iconWithClassName(Calendar);
