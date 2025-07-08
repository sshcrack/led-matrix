import * as React from 'react';
import { iconWithClassName } from './iconWithClassName';
import { Path, Svg } from 'react-native-svg';

const Activity = React.forwardRef<
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
    <Path d="M22 12h-4l-3 9L9 3l-3 9H2" />
  </Svg>
));

Activity.displayName = 'Activity';

export { Activity };
export default iconWithClassName(Activity);
