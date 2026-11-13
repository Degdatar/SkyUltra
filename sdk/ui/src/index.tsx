import * as React from "react";
import { enterSkyInputScope } from "./interaction.js";
import * as ContextMenu from "@radix-ui/react-context-menu";
import * as Dialog from "@radix-ui/react-dialog";
import * as DropdownMenu from "@radix-ui/react-dropdown-menu";
import * as Tabs from "@radix-ui/react-tabs";
import * as Tooltip from "@radix-ui/react-tooltip";
import { motion, useReducedMotion, type MotionProps } from "motion/react";
import { cva, type VariantProps } from "class-variance-authority";
import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

export * from "lucide-react";
export { SkyThemeScope, SkyRingMeter, SkySegmentedControl, SkyStatTile, SkyDataList, SkySparkline, SkyRadarChart, SkyChoice } from "./core.js";
export type { SkyRingMeterProps, SkySegmentOption, SkyDataRow, SkyChoiceOption } from "./core.js";
export { SkyHotkeyEditor, type SkyHotkeyEditorProps } from "./chrome.js";
export * from "./widgets.js";

function SkyPopupInputScope(): React.JSX.Element {
  const marker = React.useRef<HTMLSpanElement>(null);
  React.useEffect(() => {
    const parent = marker.current?.parentElement;
    if (!parent) return;
    let release: (() => void) | undefined;
    const update = (): void => {
      if (parent.getAttribute("data-state") !== "closed") release ??= enterSkyInputScope();
      else { release?.(); release = undefined; }
    };
    update();
    const observer = new MutationObserver(update);
    observer.observe(parent,{attributes:true,attributeFilter:["data-state"]});
    return () => { observer.disconnect(); release?.(); };
  }, []);
  return <span ref={marker} hidden aria-hidden="true" />;
}

/** Built-in palettes. The original three names remain supported. */
export type SkyTheme =
  | "darkmode"
  | "obsidian"
  | "frost"
  | "dragonstone"
  | "arcane"
  | "aurora"
  | "ember"
  | "glacier"
  | "verdant"
  | "aether"
  | "dwemer"
  | "sunspire"
  | "cyberneon"
  | "synthwave"
  | "venomneon"
  | "plasmaneon"
  | "crimsonneon";

const SkyThemeContext = React.createContext<SkyTheme>("frost");

function useSkyTheme(): SkyTheme {
  return React.useContext(SkyThemeContext);
}

/** Merge conditional and caller-supplied Tailwind classes without duplicates. */
export function cn(...values: ClassValue[]): string {
  return twMerge(clsx(values));
}

export interface SkyThemeProviderProps extends React.HTMLAttributes<HTMLDivElement> {
  theme?: SkyTheme;
}

/** Theme scope for SkyUltra components and consumer-defined CSS variables. */
export const SkyThemeProvider = React.forwardRef<HTMLDivElement, SkyThemeProviderProps>(
  ({ theme = "frost", className, ...props }, ref) => (
    <SkyThemeContext.Provider value={theme}>
      <div
        {...props}
        ref={ref}
        data-sky-theme={theme}
        data-sky-ui-style={theme === "darkmode" ? "velvet-dark" : "standard"}
        className={cn("sky-theme", theme === "darkmode" && "sky-theme--darkmode", className)}
      />
    </SkyThemeContext.Provider>
  ),
);
SkyThemeProvider.displayName = "SkyThemeProvider";

const buttonVariants = cva("sky-button", {
  variants: {
    variant: {
      primary: "sky-button--primary",
      secondary: "sky-button--secondary",
      ghost: "sky-button--ghost",
      danger: "sky-button--danger",
    },
    size: {
      sm: "sky-button--sm",
      md: "sky-button--md",
      lg: "sky-button--lg",
    },
  },
  defaultVariants: {
    variant: "primary",
    size: "md",
  },
});

export interface SkyButtonProps
  extends React.ButtonHTMLAttributes<HTMLButtonElement>, VariantProps<typeof buttonVariants> {}

export const SkyButton = React.forwardRef<HTMLButtonElement, SkyButtonProps>(
  ({ className, variant, size, type = "button", ...props }, ref) => (
    <button
      {...props}
      ref={ref}
      type={type}
      className={cn(buttonVariants({ variant, size }), className)}
    />
  ),
);
SkyButton.displayName = "SkyButton";

export interface SkyIconButtonProps
  extends Omit<SkyButtonProps, "children"> {
  /** Required accessible name for the icon-only control. */
  label: string;
  icon: React.ReactNode;
}

export const SkyIconButton = React.forwardRef<HTMLButtonElement, SkyIconButtonProps>(
  ({ label, icon, ...props }, ref) => (
    <SkyButton {...props} ref={ref} aria-label={label} title={props.title ?? label}>
      {icon}
    </SkyButton>
  ),
);
SkyIconButton.displayName = "SkyIconButton";

const badgeVariants = cva("sky-badge", {
  variants: {
    tone: {
      neutral: "sky-badge--neutral",
      accent: "sky-badge--accent",
      gold: "sky-badge--gold",
      success: "sky-badge--success",
      danger: "sky-badge--danger",
    },
  },
  defaultVariants: { tone: "neutral" },
});

export interface SkyBadgeProps
  extends React.HTMLAttributes<HTMLSpanElement>, VariantProps<typeof badgeVariants> {}

export const SkyBadge = React.forwardRef<HTMLSpanElement, SkyBadgeProps>(
  ({ className, tone, ...props }, ref) => (
    <span {...props} ref={ref} className={cn(badgeVariants({ tone }), className)} />
  ),
);
SkyBadge.displayName = "SkyBadge";

export const SkyPanel = React.forwardRef<HTMLElement, React.HTMLAttributes<HTMLElement>>(
  ({ className, ...props }, ref) => (
    <section {...props} ref={ref} className={cn("sky-panel", className)} />
  ),
);
SkyPanel.displayName = "SkyPanel";

export const SkyPanelHeader = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => (
    <div {...props} ref={ref} className={cn("sky-panel__header", className)} />
  ),
);
SkyPanelHeader.displayName = "SkyPanelHeader";

export const SkyPanelTitle = React.forwardRef<HTMLHeadingElement, React.HTMLAttributes<HTMLHeadingElement>>(
  ({ className, ...props }, ref) => (
    <h2 {...props} ref={ref} className={cn("sky-panel__title", className)} />
  ),
);
SkyPanelTitle.displayName = "SkyPanelTitle";

export const SkyPanelDescription = React.forwardRef<
  HTMLParagraphElement,
  React.HTMLAttributes<HTMLParagraphElement>
>(({ className, ...props }, ref) => (
  <p {...props} ref={ref} className={cn("sky-panel__description", className)} />
));
SkyPanelDescription.displayName = "SkyPanelDescription";

export const SkyPanelContent = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => (
    <div {...props} ref={ref} className={cn("sky-panel__content", className)} />
  ),
);
SkyPanelContent.displayName = "SkyPanelContent";

export const SkyCard = React.forwardRef<HTMLElement, React.HTMLAttributes<HTMLElement>>(
  ({ className, ...props }, ref) => <article {...props} ref={ref} className={cn("sky-card", className)} />,
);
SkyCard.displayName = "SkyCard";

export const SkyCardHeader = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => <div {...props} ref={ref} className={cn("sky-card__header", className)} />,
);
SkyCardHeader.displayName = "SkyCardHeader";

export const SkyCardTitle = React.forwardRef<HTMLHeadingElement, React.HTMLAttributes<HTMLHeadingElement>>(
  ({ className, ...props }, ref) => <h2 {...props} ref={ref} className={cn("sky-card__title", className)} />,
);
SkyCardTitle.displayName = "SkyCardTitle";

export const SkyCardDescription = React.forwardRef<HTMLParagraphElement, React.HTMLAttributes<HTMLParagraphElement>>(
  ({ className, ...props }, ref) => <p {...props} ref={ref} className={cn("sky-card__description", className)} />,
);
SkyCardDescription.displayName = "SkyCardDescription";

export const SkyCardContent = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => <div {...props} ref={ref} className={cn("sky-card__content", className)} />,
);
SkyCardContent.displayName = "SkyCardContent";

export const SkyCardFooter = React.forwardRef<HTMLDivElement, React.HTMLAttributes<HTMLDivElement>>(
  ({ className, ...props }, ref) => <div {...props} ref={ref} className={cn("sky-card__footer", className)} />,
);
SkyCardFooter.displayName = "SkyCardFooter";

export interface SkyNoticeProps extends React.HTMLAttributes<HTMLDivElement> {
  tone?: "info" | "success" | "warning" | "danger";
}

export const SkyNotice = React.forwardRef<HTMLDivElement, SkyNoticeProps>(
  ({ tone = "info", className, role, ...props }, ref) => (
    <div
      {...props}
      ref={ref}
      role={role ?? (tone === "danger" || tone === "warning" ? "alert" : "status")}
      data-tone={tone}
      className={cn("sky-notice", className)}
    />
  ),
);
SkyNotice.displayName = "SkyNotice";

export interface SkyToastProps extends Omit<React.HTMLAttributes<HTMLDivElement>, "role"> {
  tone?: "info" | "success" | "warning" | "danger";
  onDismiss?: () => void;
  dismissLabel?: string;
}

/** A compact, dismissible notification surface. Control its lifetime in the host page. */
export const SkyToast = React.forwardRef<HTMLDivElement, SkyToastProps>(
  ({ tone = "info", onDismiss, dismissLabel = "Dismiss notification", className, children, ...props }, ref) => (
    <div
      {...props}
      ref={ref}
      role={tone === "danger" || tone === "warning" ? "alert" : "status"}
      aria-live={tone === "danger" || tone === "warning" ? "assertive" : "polite"}
      data-tone={tone}
      className={cn("sky-toast", className)}
    >
      <div className="sky-toast__content">{children}</div>
      {onDismiss && (
        <button type="button" className="sky-toast__dismiss" aria-label={dismissLabel} onClick={onDismiss}>
          ×
        </button>
      )}
    </div>
  ),
);
SkyToast.displayName = "SkyToast";

export interface SkyControllerPromptProps extends React.HTMLAttributes<HTMLSpanElement> {
  button: string;
}

export const SkyControllerPrompt = React.forwardRef<HTMLSpanElement, SkyControllerPromptProps>(
  ({ button, className, children, ...props }, ref) => (
    <span {...props} ref={ref} className={cn("sky-controller-prompt", className)}>
      <kbd className="sky-controller-prompt__key">{button}</kbd>
      {children && <span>{children}</span>}
    </span>
  ),
);
SkyControllerPrompt.displayName = "SkyControllerPrompt";

export interface SkyHudProps extends React.HTMLAttributes<HTMLElement> {
  label?: string;
}

/** Low-profile status strip for health, stamina, magicka, or other live values. */
export const SkyHud = React.forwardRef<HTMLElement, SkyHudProps>(
  ({ label = "Status", className, ...props }, ref) => (
    <section {...props} ref={ref} aria-label={props["aria-label"] ?? label} className={cn("sky-hud", className)} />
  ),
);
SkyHud.displayName = "SkyHud";

export interface SkyHudStatProps extends React.HTMLAttributes<HTMLDivElement> {
  label: string;
  value: React.ReactNode;
  icon?: React.ReactNode;
  tone?: "accent" | "gold" | "success" | "danger";
}

export const SkyHudStat = React.forwardRef<HTMLDivElement, SkyHudStatProps>(
  ({ label, value, icon, tone = "accent", className, ...props }, ref) => (
    <div {...props} ref={ref} data-tone={tone} className={cn("sky-hud__stat", className)}>
      {icon && <span className="sky-hud__icon" aria-hidden="true">{icon}</span>}
      <span className="sky-hud__copy"><span className="sky-hud__label">{label}</span><strong className="sky-hud__value">{value}</strong></span>
    </div>
  ),
);
SkyHudStat.displayName = "SkyHudStat";

export interface SkySidebarProps extends React.HTMLAttributes<HTMLElement> {
  collapsed?: boolean;
}

export const SkySidebar = React.forwardRef<HTMLElement, SkySidebarProps>(
  ({ collapsed = false, className, ...props }, ref) => (
    <nav {...props} ref={ref} data-collapsed={collapsed || undefined} className={cn("sky-sidebar", className)} />
  ),
);
SkySidebar.displayName = "SkySidebar";

export interface SkySidebarItemProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  active?: boolean;
  icon?: React.ReactNode;
}

export const SkySidebarItem = React.forwardRef<HTMLButtonElement, SkySidebarItemProps>(
  ({ active = false, icon, className, children, ...props }, ref) => (
    <button
      {...props}
      ref={ref}
      type={props.type ?? "button"}
      aria-current={active ? "page" : undefined}
      data-active={active || undefined}
      className={cn("sky-sidebar__item", className)}
    >
      {icon && <span className="sky-sidebar__icon" aria-hidden="true">{icon}</span>}
      <span className="sky-sidebar__label">{children}</span>
    </button>
  ),
);
SkySidebarItem.displayName = "SkySidebarItem";

export interface SkyInventoryItem {
  id: string;
  name: string;
  detail?: string;
  icon?: React.ReactNode;
  badge?: React.ReactNode;
  count?: number;
  disabled?: boolean;
}

export interface SkyInventoryGridProps extends Omit<React.HTMLAttributes<HTMLDivElement>, "onSelect"> {
  items: SkyInventoryItem[];
  selectedId?: string;
  onSelect?: (item: SkyInventoryItem) => void;
  columns?: number;
  emptyState?: React.ReactNode;
}

/** Button-based, gamepad-focusable grid for inventories, spells, and collections. */
export const SkyInventoryGrid = React.forwardRef<HTMLDivElement, SkyInventoryGridProps>(
  ({ items, selectedId, onSelect, columns = 4, emptyState = "Nothing to show yet.", className, ...props }, ref) => {
    const safeColumns = Math.max(1, Math.min(8, Math.floor(Number.isFinite(columns) ? columns : 4)));
    return (
      <div
        {...props}
        ref={ref}
        className={cn("sky-inventory-grid", className)}
        style={{ ...props.style, "--sky-inventory-columns": safeColumns } as React.CSSProperties}
      >
        {items.length === 0 ? (
          <div className="sky-inventory-grid__empty">{emptyState}</div>
        ) : items.map((item) => (
          <button
            key={item.id}
            type="button"
            disabled={item.disabled}
            aria-pressed={selectedId === item.id}
            aria-label={`${item.name}${item.count && item.count > 1 ? `, ${item.count}` : ""}${item.detail ? `, ${item.detail}` : ""}`}
            data-selected={selectedId === item.id || undefined}
            className="sky-inventory-grid__item"
            onClick={() => onSelect?.(item)}
          >
            <span className="sky-inventory-grid__art">{item.icon ?? <span aria-hidden="true">—</span>}</span>
            <span className="sky-inventory-grid__name">{item.name}</span>
            {item.detail && <span className="sky-inventory-grid__detail">{item.detail}</span>}
            {(item.badge || (item.count && item.count > 1)) && (
              <span className="sky-inventory-grid__badge">{item.badge ?? `×${item.count}`}</span>
            )}
          </button>
        ))}
      </div>
    );
  },
);
SkyInventoryGrid.displayName = "SkyInventoryGrid";

export const SkyEquipmentPanel = React.forwardRef<HTMLElement, React.HTMLAttributes<HTMLElement>>(
  ({ className, ...props }, ref) => <section {...props} ref={ref} className={cn("sky-equipment-panel", className)} />,
);
SkyEquipmentPanel.displayName = "SkyEquipmentPanel";

export interface SkyEquipmentSlotProps extends Omit<React.ButtonHTMLAttributes<HTMLButtonElement>, "children"> {
  slot: string;
  item?: string;
  icon?: React.ReactNode;
}

export const SkyEquipmentSlot = React.forwardRef<HTMLButtonElement, SkyEquipmentSlotProps>(
  ({ slot, item, icon, className, ...props }, ref) => (
    <button
      {...props}
      ref={ref}
      type={props.type ?? "button"}
      aria-label={props["aria-label"] ?? `${slot}: ${item ?? "empty"}`}
      data-empty={!item || undefined}
      className={cn("sky-equipment-slot", className)}
    >
      <span className="sky-equipment-slot__icon" aria-hidden="true">{icon ?? <span>{item ? "✓" : "+"}</span>}</span>
      <span className="sky-equipment-slot__copy"><small>{slot}</small><strong>{item ?? "Empty slot"}</strong></span>
      <span className="sky-equipment-slot__arrow" aria-hidden="true">›</span>
    </button>
  ),
);
SkyEquipmentSlot.displayName = "SkyEquipmentSlot";

export interface SkyGameTooltipProps extends React.HTMLAttributes<HTMLDivElement> {
  title: string;
  subtitle?: string;
  rarity?: string;
  description?: string;
  footer?: React.ReactNode;
}

/** Structured, Skyrim-style item tooltip that also works as a standalone card. */
export const SkyGameTooltip = React.forwardRef<HTMLDivElement, SkyGameTooltipProps>(
  ({ title, subtitle, rarity = "common", description, footer, className, children, ...props }, ref) => (
    <div {...props} ref={ref} data-rarity={rarity.toLowerCase()} className={cn("sky-game-tooltip", className)}>
      <div className="sky-game-tooltip__heading"><div><strong>{title}</strong>{subtitle && <small>{subtitle}</small>}</div><span>{rarity}</span></div>
      {description && <p>{description}</p>}
      {children}
      {footer && <footer>{footer}</footer>}
    </div>
  ),
);
SkyGameTooltip.displayName = "SkyGameTooltip";

export const SkyLabel = React.forwardRef<HTMLLabelElement, React.LabelHTMLAttributes<HTMLLabelElement>>(
  ({ className, ...props }, ref) => <label {...props} ref={ref} className={cn("sky-label", className)} />,
);
SkyLabel.displayName = "SkyLabel";

export const SkyInput = React.forwardRef<HTMLInputElement, React.InputHTMLAttributes<HTMLInputElement>>(
  ({ className, ...props }, ref) => <input {...props} ref={ref} className={cn("sky-input", className)} />,
);
SkyInput.displayName = "SkyInput";

export const SkyTextarea = React.forwardRef<HTMLTextAreaElement, React.TextareaHTMLAttributes<HTMLTextAreaElement>>(
  ({ className, ...props }, ref) => <textarea {...props} ref={ref} className={cn("sky-textarea", className)} />,
);
SkyTextarea.displayName = "SkyTextarea";

export const SkySelect = React.forwardRef<HTMLSelectElement, React.SelectHTMLAttributes<HTMLSelectElement>>(
  ({ className, ...props }, ref) => <select {...props} ref={ref} className={cn("sky-select", className)} />,
);
SkySelect.displayName = "SkySelect";

export interface SkyCheckboxProps extends Omit<React.InputHTMLAttributes<HTMLInputElement>, "type"> {}

export const SkyCheckbox = React.forwardRef<HTMLInputElement, SkyCheckboxProps>(
  ({ className, ...props }, ref) => <input {...props} ref={ref} type="checkbox" className={cn("sky-checkbox", className)} />,
);
SkyCheckbox.displayName = "SkyCheckbox";

export interface SkySwitchProps extends Omit<React.ButtonHTMLAttributes<HTMLButtonElement>, "onChange"> {
  checked: boolean;
  onCheckedChange: (checked: boolean) => void;
}

export const SkySwitch = React.forwardRef<HTMLButtonElement, SkySwitchProps>(
  ({ checked, onCheckedChange, className, ...props }, ref) => (
    <button
      {...props}
      ref={ref}
      type={props.type ?? "button"}
      role="switch"
      aria-checked={checked}
      data-checked={checked || undefined}
      className={cn("sky-switch", className)}
      onClick={(event) => {
        props.onClick?.(event);
        if (!event.defaultPrevented && !props.disabled) onCheckedChange(!checked);
      }}
    ><span className="sky-switch__thumb" /></button>
  ),
);
SkySwitch.displayName = "SkySwitch";

export const SkySlider = React.forwardRef<HTMLInputElement, React.InputHTMLAttributes<HTMLInputElement>>(
  ({ className, ...props }, ref) => <input {...props} ref={ref} type="range" className={cn("sky-slider", className)} />,
);
SkySlider.displayName = "SkySlider";

export interface SkyProgressProps extends Omit<React.HTMLAttributes<HTMLDivElement>, "children" | "role"> {
  /** Accessible name announced by assistive technology. */
  label: string;
  value: number;
  max?: number;
  tone?: "accent" | "gold" | "success" | "danger";
  size?: "sm" | "md";
}

/** Accessible, clamped progress meter for quests, stats, and resource bars. */
export const SkyProgress = React.forwardRef<HTMLDivElement, SkyProgressProps>(
  ({ label, value, max = 100, tone = "accent", size = "md", className, ...props }, ref) => {
    const safeMax = Number.isFinite(max) && max > 0 ? max : 100;
    const safeValue = Math.max(0, Math.min(safeMax, Number.isFinite(value) ? value : 0));
    const percent = (safeValue / safeMax) * 100;
    return (
      <div
        {...props}
        ref={ref}
        role="progressbar"
        aria-label={label}
        aria-valuemin={0}
        aria-valuemax={safeMax}
        aria-valuenow={safeValue}
        data-tone={tone}
        data-size={size}
        className={cn("sky-progress", className)}
      >
        <span className="sky-progress__track">
          <span className="sky-progress__indicator" style={{ width: `${percent}%` }} />
        </span>
      </div>
    );
  },
);
SkyProgress.displayName = "SkyProgress";

export type SkyRevealProps = Omit<
  React.ComponentPropsWithoutRef<typeof motion.div>,
  "initial" | "animate" | "transition"
> & {
  motionProps?: MotionProps;
  initial?: MotionProps["initial"];
  animate?: MotionProps["animate"];
  transition?: MotionProps["transition"];
};

/** Small opt-in entrance animation; consumer CSS remains in control of layout. */
export function SkyReveal({
  className,
  motionProps,
  initial,
  animate,
  transition,
  ...props
}: SkyRevealProps): React.JSX.Element {
  const reduceMotion = useReducedMotion();
  return (
    <motion.div
      {...props}
      {...motionProps}
      className={cn("sky-reveal", className)}
      initial={reduceMotion ? false : motionProps?.initial ?? initial ?? { opacity: 0, y: 8 }}
      animate={motionProps?.animate ?? animate ?? { opacity: 1, y: 0 }}
      transition={
        reduceMotion
          ? { duration: 0 }
          : motionProps?.transition ?? transition ?? { duration: 0.18, ease: "easeOut" }
      }
    />
  );
}

type DialogOverlayProps = React.ComponentPropsWithoutRef<typeof Dialog.Overlay>;
export const SkyDialogOverlay = React.forwardRef<
React.ElementRef<typeof Dialog.Overlay>, DialogOverlayProps
>(({ className, ...props }, ref) => {
  const theme = useSkyTheme();
  return (
    <Dialog.Overlay
      {...props}
      ref={ref}
      data-sky-theme={theme}
      className={cn("sky-dialog__overlay", className)}
    />
  );
});
SkyDialogOverlay.displayName = "SkyDialogOverlay";

type DialogContentProps = React.ComponentPropsWithoutRef<typeof Dialog.Content>;
export const SkyDialogContent = React.forwardRef<
React.ElementRef<typeof Dialog.Content>, DialogContentProps
>(({ className, children, ...props }, ref) => {
  const theme = useSkyTheme();
  return (
    <Dialog.Portal>
      <SkyDialogOverlay />
      <Dialog.Content
        {...props}
        ref={ref}
        data-sky-theme={theme}
        className={cn("sky-dialog__content", className)}
      ><SkyPopupInputScope />{children}</Dialog.Content>
    </Dialog.Portal>
  );
});
SkyDialogContent.displayName = "SkyDialogContent";

export const SkyDialogTitle = React.forwardRef<
React.ElementRef<typeof Dialog.Title>, React.ComponentPropsWithoutRef<typeof Dialog.Title>
>(({ className, ...props }, ref) => (
  <Dialog.Title {...props} ref={ref} className={cn("sky-dialog__title", className)} />
));
SkyDialogTitle.displayName = "SkyDialogTitle";

export const SkyDialogDescription = React.forwardRef<
React.ElementRef<typeof Dialog.Description>, React.ComponentPropsWithoutRef<typeof Dialog.Description>
>(({ className, ...props }, ref) => (
  <Dialog.Description {...props} ref={ref} className={cn("sky-dialog__description", className)} />
));
SkyDialogDescription.displayName = "SkyDialogDescription";

export const SkyDialogClose = React.forwardRef<
React.ElementRef<typeof Dialog.Close>, React.ComponentPropsWithoutRef<typeof Dialog.Close>
>(({ className, ...props }, ref) => (
  <Dialog.Close {...props} ref={ref} className={cn("sky-dialog__close", className)} />
));
SkyDialogClose.displayName = "SkyDialogClose";

export const SkyDialogRoot = Dialog.Root;
export const SkyDialogTrigger = Dialog.Trigger;

export const SkyTabsRoot = Tabs.Root;

export const SkyTabsList = React.forwardRef<
React.ElementRef<typeof Tabs.List>, React.ComponentPropsWithoutRef<typeof Tabs.List>
>(({ className, ...props }, ref) => (
  <Tabs.List {...props} ref={ref} className={cn("sky-tabs__list", className)} />
));
SkyTabsList.displayName = "SkyTabsList";

export const SkyTabsTrigger = React.forwardRef<
React.ElementRef<typeof Tabs.Trigger>, React.ComponentPropsWithoutRef<typeof Tabs.Trigger>
>(({ className, ...props }, ref) => (
  <Tabs.Trigger {...props} ref={ref} className={cn("sky-tabs__trigger", className)} />
));
SkyTabsTrigger.displayName = "SkyTabsTrigger";

export const SkyTabsContent = React.forwardRef<
React.ElementRef<typeof Tabs.Content>, React.ComponentPropsWithoutRef<typeof Tabs.Content>
>(({ className, ...props }, ref) => (
  <Tabs.Content {...props} ref={ref} className={cn("sky-tabs__content", className)} />
));
SkyTabsContent.displayName = "SkyTabsContent";

export const SkyTooltipProvider = Tooltip.Provider;
export const SkyTooltipRoot = Tooltip.Root;
export const SkyTooltipTrigger = Tooltip.Trigger;
export const SkyTooltipContent = React.forwardRef<
React.ElementRef<typeof Tooltip.Content>, React.ComponentPropsWithoutRef<typeof Tooltip.Content>
>(({ className, ...props }, ref) => {
  const theme = useSkyTheme();
  return (
    <Tooltip.Portal>
      <Tooltip.Content
        {...props}
        ref={ref}
        data-sky-theme={theme}
        className={cn("sky-tooltip", className)}
      />
    </Tooltip.Portal>
  );
});
SkyTooltipContent.displayName = "SkyTooltipContent";

export const SkyDropdownMenuRoot = DropdownMenu.Root;
export const SkyDropdownMenuTrigger = DropdownMenu.Trigger;
export const SkyDropdownMenuGroup = DropdownMenu.Group;
export const SkyDropdownMenuPortal = DropdownMenu.Portal;

export const SkyDropdownMenuContent = React.forwardRef<
React.ElementRef<typeof DropdownMenu.Content>, React.ComponentPropsWithoutRef<typeof DropdownMenu.Content>
>(({ className, children, sideOffset = 6, ...props }, ref) => {
  const theme = useSkyTheme();
  return (
    <DropdownMenu.Portal>
      <DropdownMenu.Content
        {...props}
        ref={ref}
        sideOffset={sideOffset}
        data-sky-theme={theme}
        className={cn("sky-menu", className)}
      ><SkyPopupInputScope />{children}</DropdownMenu.Content>
    </DropdownMenu.Portal>
  );
});
SkyDropdownMenuContent.displayName = "SkyDropdownMenuContent";

export const SkyDropdownMenuItem = React.forwardRef<
React.ElementRef<typeof DropdownMenu.Item>, React.ComponentPropsWithoutRef<typeof DropdownMenu.Item>
>(({ className, ...props }, ref) => (
  <DropdownMenu.Item {...props} ref={ref} className={cn("sky-menu__item", className)} />
));
SkyDropdownMenuItem.displayName = "SkyDropdownMenuItem";

export const SkyDropdownMenuSeparator = React.forwardRef<
React.ElementRef<typeof DropdownMenu.Separator>, React.ComponentPropsWithoutRef<typeof DropdownMenu.Separator>
>(({ className, ...props }, ref) => (
  <DropdownMenu.Separator {...props} ref={ref} className={cn("sky-menu__separator", className)} />
));
SkyDropdownMenuSeparator.displayName = "SkyDropdownMenuSeparator";

export const SkyContextMenuRoot = ContextMenu.Root;
export const SkyContextMenuTrigger = ContextMenu.Trigger;
export const SkyContextMenuGroup = ContextMenu.Group;
export const SkyContextMenuPortal = ContextMenu.Portal;

export const SkyContextMenuContent = React.forwardRef<
  React.ElementRef<typeof ContextMenu.Content>, React.ComponentPropsWithoutRef<typeof ContextMenu.Content>
>(({ className, children, ...props }, ref) => {
  const theme = useSkyTheme();
  return (
    <ContextMenu.Portal>
      <ContextMenu.Content {...props} ref={ref} data-sky-theme={theme} className={cn("sky-menu", className)}><SkyPopupInputScope />{children}</ContextMenu.Content>
    </ContextMenu.Portal>
  );
});
SkyContextMenuContent.displayName = "SkyContextMenuContent";

export const SkyContextMenuItem = React.forwardRef<
  React.ElementRef<typeof ContextMenu.Item>, React.ComponentPropsWithoutRef<typeof ContextMenu.Item>
>(({ className, ...props }, ref) => (
  <ContextMenu.Item {...props} ref={ref} className={cn("sky-menu__item", className)} />
));
SkyContextMenuItem.displayName = "SkyContextMenuItem";

export const SkyContextMenuSeparator = React.forwardRef<
  React.ElementRef<typeof ContextMenu.Separator>, React.ComponentPropsWithoutRef<typeof ContextMenu.Separator>
>(({ className, ...props }, ref) => (
  <ContextMenu.Separator {...props} ref={ref} className={cn("sky-menu__separator", className)} />
));
SkyContextMenuSeparator.displayName = "SkyContextMenuSeparator";
