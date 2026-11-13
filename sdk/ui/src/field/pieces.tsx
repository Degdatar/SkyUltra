import * as React from "react";
import { Icon, itemIcon, type IconName } from "./icons.js";
import type { Item } from "./model.js";
export function SectionTitle({ eyebrow, title, children }: { eyebrow: string; title: string; children?: React.ReactNode }): React.JSX.Element { return <div className="section-title"><div><span className="eyebrow">{eyebrow}</span><h2>{title}</h2></div>{children}</div>; }
export function Empty({ title, detail, icon = "archive" }: { title: string; detail: string; icon?: IconName }): React.JSX.Element { return <div className="empty-state"><Icon name={icon} size={30} /><strong>{title}</strong><span>{detail}</span></div>; }
export function ItemArtwork({ item, large = false }: { item: Pick<Item, "category">; large?: boolean }): React.JSX.Element { return <span className={"item-art" + (large ? " item-art--large" : "")}><Icon name={itemIcon(item.category)} size={large ? 72 : 28} /></span>; }
