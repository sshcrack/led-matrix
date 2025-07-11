import * as React from 'react';
import { Platform, View, Modal, type ViewProps } from 'react-native';
import Animated, { FadeIn, FadeOut } from 'react-native-reanimated';
import { cn } from '~/lib/utils';
import { TextClassContext } from '~/components/ui/text';

interface DialogContextValue {
  open: boolean;
  onOpenChange: (open: boolean) => void;
}

const DialogContext = React.createContext<DialogContextValue | null>(null);

const useDialogContext = () => {
  const context = React.useContext(DialogContext);
  if (!context) {
    throw new Error('Dialog components must be used within a Dialog');
  }
  return context;
};

interface DialogProps {
  open: boolean;
  onOpenChange: (open: boolean) => void;
  children: React.ReactNode;
}

const Dialog = ({ open, onOpenChange, children }: DialogProps) => {
  return (
    <DialogContext.Provider value={{ open, onOpenChange }}>
      {children}
    </DialogContext.Provider>
  );
};

interface DialogTriggerProps extends ViewProps {
  children: React.ReactNode;
}

const DialogTrigger = React.forwardRef<View, DialogTriggerProps>(
  ({ children, ...props }, ref) => {
    return (
      <View ref={ref} {...props}>
        {children}
      </View>
    );
  }
);
DialogTrigger.displayName = 'DialogTrigger';

interface DialogPortalProps {
  children: React.ReactNode;
}

const DialogPortal = ({ children }: DialogPortalProps) => {
  const { open, onOpenChange } = useDialogContext();
  
  return (
    <Modal
      visible={open}
      transparent={true}
      animationType="fade"
      onRequestClose={() => onOpenChange(false)}
      statusBarTranslucent={true}
    >
      {children}
    </Modal>
  );
};

interface DialogOverlayProps extends ViewProps {
  className?: string;
}

const DialogOverlay = React.forwardRef<View, DialogOverlayProps>(
  ({ className, children, ...props }, ref) => {
    const { onOpenChange } = useDialogContext();

    return (
      <View
        ref={ref}
        className={cn(
          'flex-1 bg-black/80 justify-center items-center p-4',
          className
        )}
        style={{
          minHeight: '100%',
          width: '100%',
        }}
        onTouchStart={(e) => {
          if (e.target === e.currentTarget) {
            onOpenChange(false);
          }
        }}
        {...props}
      >
        {children}
      </View>
    );
  }
);
DialogOverlay.displayName = 'DialogOverlay';

interface DialogContentProps extends ViewProps {
  className?: string;
  children: React.ReactNode;
}

const DialogContent = React.forwardRef<View, DialogContentProps>(
  ({ className, children, ...props }, ref) => {
    return (
      <DialogPortal>
        <DialogOverlay>
          <Animated.View
            ref={ref}
            entering={FadeIn.duration(200)}
            exiting={FadeOut.duration(200)}
            className={cn(
              'bg-card p-6 rounded-lg shadow-lg w-full',
              className
            )}
            style={{
              maxWidth: 400,
              maxHeight: '80%',
              minWidth: 320,
              width: Platform.OS === 'web' ? 400 : '90%',
            }}
            onTouchStart={(e) => {
              e.stopPropagation();
            }}
            {...props}
          >
            <TextClassContext.Provider value="text-card-foreground">
              {children}
            </TextClassContext.Provider>
          </Animated.View>
        </DialogOverlay>
      </DialogPortal>
    );
  }
);
DialogContent.displayName = 'DialogContent';

interface DialogHeaderProps extends ViewProps {
  className?: string;
}

const DialogHeader = React.forwardRef<View, DialogHeaderProps>(
  ({ className, ...props }, ref) => (
    <View
      ref={ref}
      className={cn('flex flex-col space-y-1.5 text-center sm:text-left', className)}
      {...props}
    />
  )
);
DialogHeader.displayName = 'DialogHeader';

interface DialogFooterProps extends ViewProps {
  className?: string;
}

const DialogFooter = React.forwardRef<View, DialogFooterProps>(
  ({ className, ...props }, ref) => (
    <View
      ref={ref}
      className={cn('flex flex-col-reverse sm:flex-row sm:justify-end sm:space-x-2', className)}
      {...props}
    />
  )
);
DialogFooter.displayName = 'DialogFooter';

interface DialogTitleProps extends ViewProps {
  className?: string;
}

const DialogTitle = React.forwardRef<View, DialogTitleProps>(
  ({ className, ...props }, ref) => (
    <View
      ref={ref}
      className={cn('text-lg font-semibold leading-none tracking-tight', className)}
      {...props}
    />
  )
);
DialogTitle.displayName = 'DialogTitle';

interface DialogDescriptionProps extends ViewProps {
  className?: string;
}

const DialogDescription = React.forwardRef<View, DialogDescriptionProps>(
  ({ className, ...props }, ref) => (
    <View
      ref={ref}
      className={cn('text-sm text-muted-foreground', className)}
      {...props}
    />
  )
);
DialogDescription.displayName = 'DialogDescription';

export {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogOverlay,
  DialogPortal,
  DialogTitle,
  DialogTrigger,
};
