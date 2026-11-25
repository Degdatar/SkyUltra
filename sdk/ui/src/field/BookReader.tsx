import * as React from 'react';
import { useEffect, useMemo, useRef, useState } from 'react';
import type { Item, SkyUltraItemActionResult } from '@skyultra/sdk';
import { Icon } from './icons.js';
import { palettes, type Palette } from './model.js';

export type BookReaderDesignChoice = 'auto' | 'codex' | 'scroll' | 'grimoire' | 'dwemer' | 'scholar' | 'datapad';
export type BookReaderResolvedDesign = Exclude<BookReaderDesignChoice, 'auto'>;
export type BookReaderFontScale = 'sm' | 'md' | 'lg' | 'xl';

export interface ReadingBookState {
  id: string;
  title: string;
  text: string;
  pages: string[];
  images: string[];
  style: 'book' | 'note' | 'map';
}

export interface BookReaderProps {
  book: ReadingBookState;
  playerName?: string;
  theme?: Palette;
  design?: BookReaderDesignChoice;
  themeSync?: boolean;
  onChangeDesign?: (next: BookReaderDesignChoice) => void;
  onToggleThemeSync?: (next: boolean) => void;
  onOpenNative?: (book: ReadingBookState) => void;
  onClose: () => void;
  standalone?: boolean;
  canTake?: boolean;
  isStealing?: boolean;
  taken?: boolean;
  busyTaking?: boolean;
  onTakeBook?: () => void;
  pauseWorld?: boolean;
  onTogglePauseWorld?: () => void;
  bookValue?: number;
  bookWeight?: number;
  teachesSkill?: boolean;
  skillName?: string;
  teachesSpell?: boolean;
  spellName?: string;
  notice?: string | null;
  activateKeyLabel?: string;
  activateKeyCode?: string;
  activateKey?: string;
}

const BOOKMARK_STORAGE_KEY = 'skyultra.bookBookmarks.v1';

export const bookReaderDesignOptions: Array<{ value: BookReaderDesignChoice; label: string; note: string }> = [
  { value: 'auto', label: 'Auto', note: 'Match book & active theme' },
  { value: 'codex', label: 'Codex', note: 'Illuminated bound tome' },
  { value: 'scroll', label: 'Scroll', note: 'Continuous folio manuscript' },
  { value: 'grimoire', label: 'Grimoire', note: 'Arcane runic margin filigree' },
  { value: 'dwemer', label: 'Dwemer', note: 'Brass & geometric lexicon' },
  { value: 'scholar', label: 'Scholar', note: 'High-clarity archive index' },
  { value: 'datapad', label: 'Datapad', note: 'Sharp telemetry terminal' },
];

export function parseBookActionResult(item: Pick<Item, 'id' | 'name'>, result: SkyUltraItemActionResult): ReadingBookState | null {
  if (result.bookText === undefined) return null;
  const cleanTitle = (result.bookTitle || item.name).replace(/^\$+/, '').trim();
  const pages = (result.bookPages && result.bookPages.length > 0 ? result.bookPages : [result.bookText])
    .map((p) => p.trim())
    .filter(Boolean);
  const images = result.bookImages ?? [];
  const style: 'book' | 'note' | 'map' =
    result.bookStyle ??
    (images.length > 0 && (/map/i.test(cleanTitle) || pages.length <= 1)
      ? 'map'
      : pages.length > 1
        ? 'book'
        : 'note');
  return {
    id: item.id,
    title: cleanTitle,
    text: result.bookText,
    pages: pages.length > 0 ? pages : [result.bookText],
    images,
    style,
  };
}

function readBookmarks(): Record<string, number> {
  try {
    if (typeof window === 'undefined') return {};
    const raw = window.localStorage?.getItem(BOOKMARK_STORAGE_KEY);
    if (!raw) return {};
    const parsed = JSON.parse(raw) as unknown;
    if (!parsed || typeof parsed !== 'object') return {};
    const clean: Record<string, number> = {};
    for (const [key, val] of Object.entries(parsed)) {
      if (typeof val === 'number' && Number.isInteger(val) && val >= 0) {
        clean[key] = val;
      }
    }
    return clean;
  } catch {
    return {};
  }
}

function writeBookmarks(next: Record<string, number>): void {
  try {
    if (typeof window === 'undefined') return;
    window.localStorage?.setItem(BOOKMARK_STORAGE_KEY, JSON.stringify(next));
  } catch {
    // Ignore restricted localStorage environments
  }
}

function highlightQuery(text: string, query: string): React.ReactNode {
  const q = query.trim();
  if (!q) return text;
  const lowerText = text.toLowerCase();
  const lowerQuery = q.toLowerCase();
  if (!lowerText.includes(lowerQuery)) return text;
  const parts: React.ReactNode[] = [];
  let cursor = 0;
  let key = 0;
  while (cursor < text.length) {
    const idx = lowerText.indexOf(lowerQuery, cursor);
    if (idx < 0) {
      parts.push(text.slice(cursor));
      break;
    }
    if (idx > cursor) parts.push(text.slice(cursor, idx));
    parts.push(<mark key={key++} className="book-search-hit">{text.slice(idx, idx + q.length)}</mark>);
    cursor = idx + q.length;
  }
  return parts;
}

function renderFormattedPageContent(rawPage: string, isFirstFolio: boolean, searchQuery: string): React.JSX.Element {
  const blocks = rawPage
    .replace(/\r\n/g, '\n')
    .split(/\n{2,}/)
    .map((b) => b.trim())
    .filter(Boolean);
  let appliedDropCap = false;
  return (
    <div className="inventory-book-folio__content">
      {blocks.map((block, idx) => {
        if (/^[-—―*~_=]{2,}$/.test(block)) {
          return <hr key={idx} className="inventory-book-folio__rule" />;
        }
        const lines = block.split('\n');
        const isShortHeading = idx === 0 && isFirstFolio && lines.length <= 3 && block.length < 90 && !/[.!?:]$/.test(block);
        if (isShortHeading) {
          return (
            <div key={idx} className="inventory-book-folio__title-block">
              {lines.map((line, lIdx) => <p key={lIdx}>{highlightQuery(line, searchQuery)}</p>)}
            </div>
          );
        }
        const canDropCap = isFirstFolio && !appliedDropCap && !searchQuery.trim() && /^[A-Za-z]/.test(block) && block.length > 45;
        if (canDropCap) {
          appliedDropCap = true;
          const firstChar = block.charAt(0);
          const rest = block.slice(1);
          return (
            <p key={idx} className="inventory-book-folio__paragraph has-dropcap">
              <span className="inventory-book-folio__dropcap" aria-hidden="true">{firstChar}</span>
              <span>{firstChar}{rest}</span>
            </p>
          );
        }
        return (
          <p key={idx} className="inventory-book-folio__paragraph">
            {highlightQuery(block, searchQuery)}
          </p>
        );
      })}
    </div>
  );
}

type BookLeaf =
  | { kind: 'cover' | 'back'; src?: string; title: string }
  | { kind: 'plate'; src: string; index: number }
  | { kind: 'text'; text: string; pageNo: number }
  | { kind: 'finis' };

function buildBookLeaves(book: ReadingBookState, hasRealText: boolean): BookLeaf[] {
  const coverSrc = book.images[0];
  const interior = book.images.slice(1);
  const leaves: BookLeaf[] = [{ kind: 'cover', src: coverSrc, title: book.title }];
  const pages = hasRealText ? book.pages : [];
  pages.forEach((text, idx) => {
    leaves.push({ kind: 'text', text, pageNo: idx + 1 });
    if (idx === 0) interior.forEach((src, pIdx) => leaves.push({ kind: 'plate', src, index: pIdx + 1 }));
  });
  if (pages.length === 0) interior.forEach((src, pIdx) => leaves.push({ kind: 'plate', src, index: pIdx + 1 }));
  if (leaves.length % 2 === 0) leaves.push({ kind: 'finis' });
  leaves.push({ kind: 'back', src: coverSrc, title: book.title });
  return leaves;
}

function leafLabel(leaf: BookLeaf | undefined): string {
  if (!leaf) return '';
  switch (leaf.kind) {
    case 'cover': return 'Front Cover';
    case 'back': return 'Back Cover';
    case 'plate': return `Plate ${leaf.index}`;
    case 'finis': return 'The End';
    default: return `Folio ${leaf.pageNo}`;
  }
}

function renderBookLeaf(
  leaf: BookLeaf | undefined,
  side: 'left' | 'right',
  title: string,
  ownerName: string,
  searchQuery: string,
): React.JSX.Element {
  const base = `inventory-book-folio inventory-book-folio--${side}`;
  if (!leaf) return <article className={base + ' is-blank'} />;
  if (leaf.kind === 'cover' || leaf.kind === 'back') {
    return (
      <article className={`${base} inventory-book-folio--cover is-${leaf.kind}`}>
        {leaf.src ? (
          <img className="inventory-book-cover__art" src={leaf.src} alt={`${title} ${leaf.kind === 'cover' ? 'cover' : 'back cover'}`} />
        ) : (
          <div className="inventory-book-cover__leather">
            <img className="inventory-book-cover__logo" src="./SkyUltraLogoMark.png" alt="SkyUltra" draggable={false} />
            {leaf.kind === 'cover' && <strong>{title}</strong>}
            <small>{leaf.kind === 'cover' ? 'SkyUltra Archives' : ownerName}</small>
          </div>
        )}
      </article>
    );
  }
  if (leaf.kind === 'plate') {
    return (
      <article className={`${base} inventory-book-folio--plate`}>
        <div className="inventory-book-plate__frame"><img src={leaf.src} alt={`${title} plate ${leaf.index}`} /></div>
        <small className="inventory-book-plate__caption">Illuminated Plate {leaf.index}</small>
      </article>
    );
  }
  if (leaf.kind === 'finis') {
    return (
      <article className={base}>
        <div className="inventory-book-folio__endpiece">
          <small>The End</small>
        </div>
      </article>
    );
  }
  if (leaf.kind !== 'text') return <article className={base} />;
  return (
    <article className={base}>
      <header className="inventory-book-folio__header">
        <span>{title}</span>
        <small>Folio {leaf.pageNo}</small>
      </header>
      {renderFormattedPageContent(leaf.text, leaf.pageNo === 1, searchQuery)}
      <footer className="inventory-book-folio__footer">
        <span>— {leaf.pageNo} —</span>
      </footer>
    </article>
  );
}

function resolveAutoDesign(book: ReadingBookState, theme: Palette): BookReaderResolvedDesign {
  const family = (palettes.find((p) => p.value === theme) ?? palettes[0]).family;
  if (book.style === 'map') return 'scholar';
  if (book.style === 'note') return family === 'neon' ? 'datapad' : 'scroll';
  if (family === 'neon') return 'datapad';
  if (family === 'celestial') return 'grimoire';
  if (theme === 'dwemer') return 'dwemer';
  return 'codex';
}

export function BookReader({
  book,
  playerName,
  theme = 'dragonstone',
  design = 'auto',
  themeSync = true,
  standalone = false,
  canTake = false,
  isStealing = false,
  taken = false,
  busyTaking = false,
  onTakeBook,
  bookValue = 0,
  bookWeight = 0,
  teachesSkill = false,
  skillName = '',
  teachesSpell = false,
  spellName = '',
  notice = null,
  activateKeyLabel = 'E',
  activateKeyCode = 'KeyE',
  activateKey = 'e',
  onChangeDesign,
  onToggleThemeSync,
  onOpenNative,
  onClose,
}: BookReaderProps): React.JSX.Element {
  const [localDesign, setLocalDesign] = useState<BookReaderDesignChoice>(design);
  const [localThemeSync, setLocalThemeSync] = useState<boolean>(themeSync);
  const [fontScale, setFontScale] = useState<BookReaderFontScale>('md');
  const [viewMode, setViewMode] = useState<'spread' | 'scroll'>(() =>
    book.style === 'book' && (book.pages.length >= 1 || book.images.length > 0) ? 'spread' : 'scroll',
  );
  const [spreadIndex, setSpreadIndex] = useState<number>(() => {
    const marks = readBookmarks();
    return marks[book.id] ?? 0;
  });
  const [turnDir, setTurnDir] = useState<'next' | 'prev' | ''>('');
  const [searchOpen, setSearchOpen] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [zoomScale, setZoomScale] = useState<number>(1);
  const [pan, setPan] = useState<{ x: number; y: number }>({ x: 0, y: 0 });
  const panDragRef = useRef<{ id: number; startX: number; startY: number; origX: number; origY: number } | null>(null);
  const panRafRef = useRef<number>(0);
  const pendingPanRef = useRef<{ x: number; y: number } | null>(null);
  useEffect(() => () => {
    if (panRafRef.current) cancelAnimationFrame(panRafRef.current);
  }, []);
  const [bookmarks, setBookmarks] = useState<Record<string, number>>(() => readBookmarks());
  const spreadMaxRef = useRef(0);
  const mountedAtRef = useRef<number>(typeof performance !== 'undefined' ? performance.now() : 0);

  useEffect(() => { setLocalDesign(design); }, [design]);
  useEffect(() => { setLocalThemeSync(themeSync); }, [themeSync]);
  useEffect(() => {
    mountedAtRef.current = typeof performance !== 'undefined' ? performance.now() : 0;
    const marks = readBookmarks();
    setBookmarks(marks);
    setSpreadIndex(marks[book.id] ?? 0);
    setTurnDir('');
    setZoomScale(1);
    setPan({ x: 0, y: 0 });
    setSearchQuery('');
    setViewMode(book.style === 'book' && (book.pages.length >= 1 || book.images.length > 0) ? 'spread' : 'scroll');
  }, [book.id, book.style, book.pages.length, book.images.length]);

  const resolvedDesign: BookReaderResolvedDesign =
    localDesign === 'auto' ? resolveAutoDesign(book, theme) : localDesign;

  const isMap = book.style === 'map';
  const hasRealText = book.pages.some((p) => !/^(?:Illustration Plate\s*\d+|No inscribed text\.)/i.test(p.trim()));
  const textPageCount = hasRealText ? book.pages.length : 0;
  const shownImages = !isMap && book.images.length > 1 ? book.images.slice(1) : book.images;
  const canUseCodex =
    viewMode === 'spread' &&
    resolvedDesign !== 'scroll' &&
    book.style === 'book' &&
    (book.pages.length >= 1 || book.images.length > 0);
  const leaves = useMemo(() => buildBookLeaves(book, hasRealText), [book, hasRealText]);
  const totalSpreads = Math.max(1, Math.ceil(leaves.length / 2));
  const clampedSpread = Math.max(0, Math.min(totalSpreads - 1, spreadIndex));
  spreadMaxRef.current = totalSpreads - 1;
  const leftLeaf = leaves[clampedSpread * 2];
  const rightLeaf = leaves[clampedSpread * 2 + 1];
  const ownerDisplay = playerName && playerName.trim() ? playerName.trim() : 'Dragonborn';
  const leftFolio = useMemo(
    () => renderBookLeaf(leftLeaf, 'left', book.title, ownerDisplay, searchQuery),
    [leftLeaf, book.title, ownerDisplay, searchQuery],
  );
  const rightFolio = useMemo(
    () => renderBookLeaf(rightLeaf, 'right', book.title, ownerDisplay, searchQuery),
    [rightLeaf, book.title, ownerDisplay, searchQuery],
  );
  const scrolledPages = useMemo(
    () =>
      book.pages.map((pageText, pIdx) => (
        <div key={pIdx} className="inventory-book-parchment__section">
          {book.pages.length > 1 && (
            <div className="inventory-book-parchment__folio-tag">Folio {pIdx + 1} of {book.pages.length}</div>
          )}
          {renderFormattedPageContent(pageText, pIdx === 0, searchQuery)}
        </div>
      )),
    [book.pages, searchQuery],
  );

  const matchingPages = useMemo(() => {
    const q = searchQuery.trim().toLowerCase();
    if (!q) return [];
    const hits: Array<{ pageIndex: number; spreadIdx: number; label: string }> = [];
    leaves.forEach((leaf, leafIdx) => {
      if (leaf.kind === 'text' && leaf.text.toLowerCase().includes(q)) {
        hits.push({ pageIndex: leaf.pageNo, spreadIdx: Math.floor(leafIdx / 2), label: `Folio ${leaf.pageNo}` });
      }
    });
    return hits;
  }, [leaves, searchQuery]);

  const bookmarkedSpread = bookmarks[book.id];
  const isCurrentBookmarked = bookmarkedSpread !== undefined && bookmarkedSpread === clampedSpread;

  useEffect(() => {
    if (typeof window === 'undefined') return;
    const onKey = (ev: KeyboardEvent): void => {
      const target = ev.target as HTMLElement | null;
      const inInput = target && (target.tagName === 'INPUT' || target.tagName === 'TEXTAREA');
      if (ev.key === 'Escape') {
        ev.preventDefault();
        ev.stopPropagation();
        onClose();
        return;
      }
      if (!standalone && (ev.ctrlKey || ev.metaKey) && ev.key.toLowerCase() === 'f') {
        ev.preventDefault();
        ev.stopPropagation();
        setSearchOpen(true);
        return;
      }
      if (inInput) return;
      const elapsedSinceOpen =
        typeof performance !== 'undefined' ? performance.now() - mountedAtRef.current : 1000;
      const matchesActivateKey =
        !ev.repeat &&
        elapsedSinceOpen >= 250 &&
        ((Boolean(activateKeyCode) && ev.code === activateKeyCode) ||
          (Boolean(activateKey) && ev.key.toLowerCase() === activateKey.toLowerCase()));
      if (matchesActivateKey && canTake && !taken && !busyTaking && onTakeBook) {
        ev.preventDefault();
        ev.stopPropagation();
        onTakeBook();
        return;
      }
      if (ev.key === 'ArrowRight' || ev.code === 'ArrowRight' || ev.key === 'PageDown') {
        ev.preventDefault();
        ev.stopPropagation();
        setTurnDir('next');
        setSpreadIndex((i) => Math.min(spreadMaxRef.current, i + 1));
      } else if (ev.key === 'ArrowLeft' || ev.code === 'ArrowLeft' || ev.key === 'PageUp') {
        ev.preventDefault();
        ev.stopPropagation();
        setTurnDir('prev');
        setSpreadIndex((i) => Math.max(0, i - 1));
      }
    };
    window.addEventListener?.('keydown', onKey, true);
    return () => window.removeEventListener?.('keydown', onKey, true);
  }, [onClose, standalone, activateKeyCode, activateKey, canTake, taken, busyTaking, onTakeBook]);

  const turn = (dir: 'prev' | 'next'): void => {
    setTurnDir(dir);
    setSpreadIndex((i) => (dir === 'next' ? Math.min(totalSpreads - 1, i + 1) : Math.max(0, i - 1)));
  };

  const toggleBookmark = (): void => {
    const next = { ...bookmarks };
    if (isCurrentBookmarked) {
      delete next[book.id];
    } else {
      next[book.id] = clampedSpread;
    }
    setBookmarks(next);
    writeBookmarks(next);
  };

  const handleDesignSelect = (next: BookReaderDesignChoice): void => {
    setLocalDesign(next);
    if (next === 'scroll') setViewMode('scroll');
    else if (next === 'codex' && book.style === 'book' && (book.pages.length > 1 || book.images.length > 0)) setViewMode('spread');
    onChangeDesign?.(next);
  };

  const handleThemeSyncToggle = (): void => {
    const next = !localThemeSync;
    setLocalThemeSync(next);
    onToggleThemeSync?.(next);
  };

  const cycleFontScale = (): void => {
    const order: BookReaderFontScale[] = ['sm', 'md', 'lg', 'xl'];
    const next = order[(order.indexOf(fontScale) + 1) % order.length] ?? 'md';
    setFontScale(next);
  };

  const cycleZoom = (): void => {
    const next = zoomScale < 1.2 ? 1.65 : zoomScale < 2.0 ? 2.3 : 1;
    setZoomScale(next);
    if (next === 1) setPan({ x: 0, y: 0 });
  };

  const eyebrowText = isMap
    ? "CARTOGRAPHER'S ARCHIVE · SURVEY PLATE"
    : book.style === 'note'
      ? 'IMPERIAL DISPATCH · MANUSCRIPT'
      : 'SKYULTRA CHRONICLE · ARCHIVE READER';

  return (
    <div
      className={`inventory-book-modal inventory-book-modal--${book.style}`}
      role="dialog"
      aria-modal="true"
      aria-label={book.title}
      data-book-design={resolvedDesign}
      data-book-theme-sync={localThemeSync ? 'true' : 'false'}
      data-book-font={fontScale}
      onClick={onClose}
    >
      <div
        className={`inventory-book-reader inventory-book-reader--${book.style} is-design-${resolvedDesign}${canUseCodex ? ' is-codex' : ''} surface`}
        onClick={(e) => e.stopPropagation()}
      >
        <span className="inventory-book-reader__corner is-tl" aria-hidden="true" />
        <span className="inventory-book-reader__corner is-tr" aria-hidden="true" />
        <span className="inventory-book-reader__corner is-bl" aria-hidden="true" />
        <span className="inventory-book-reader__corner is-br" aria-hidden="true" />

        <header className="inventory-book-reader__header">
          <div className="inventory-book-reader__title-group">
            <span className="eyebrow"><Icon name="book" size={12} /> {eyebrowText}</span>
            <h2>{book.title}</h2>
            <div className="inventory-book-reader__meta">
              <span>{isMap ? 'Field Survey Plate' : book.style === 'note' ? 'Manuscript' : 'Bound Tome'}</span>
              {textPageCount > 0 && <span>{textPageCount} {textPageCount === 1 ? 'folio' : 'folios'}</span>}
              {!isMap && shownImages.length > 0 && <span>{shownImages.length} {shownImages.length === 1 ? 'engraving' : 'engravings'}</span>}
              {typeof bookValue === 'number' && bookValue > 0 && <span>{bookValue.toLocaleString()}g</span>}
              {typeof bookWeight === 'number' && bookWeight > 0 && <span>{bookWeight} wt</span>}
              {teachesSkill && skillName && <span>Skill Book · {skillName}</span>}
              {teachesSpell && spellName && <span>Spell Tome · {spellName}</span>}
              {!standalone && (
                <span className="book-design-badge">{resolvedDesign.toUpperCase()} · {localThemeSync ? 'THEME' : 'PARCHMENT'}</span>
              )}
            </div>
            {notice ? <div className="book-reader-world-banner">{notice}</div> : null}
          </div>

          <div className="inventory-book-reader__actions">
            {standalone && canTake && !taken && onTakeBook && (
              <button
                type="button"
                className={'book-action-btn ' + (isStealing ? 'is-steal' : 'is-take')}
                disabled={busyTaking}
                onClick={onTakeBook}
                title={isStealing ? `Steal this book [${activateKeyLabel}]` : `Take this book into your inventory [${activateKeyLabel}]`}
              >
                <Icon name={isStealing ? 'stolen' : 'inventory'} size={12} />
                <span>{busyTaking ? 'Taking...' : isStealing ? 'Steal Book' : 'Take Book'}</span>
                <kbd>{activateKeyLabel}</kbd>
              </button>
            )}
            {!standalone && isMap && shownImages.length > 0 && (
              <button type="button" className={'book-action-btn' + (zoomScale > 1 ? ' is-active' : '')} onClick={cycleZoom}>
                <Icon name="search" size={12} />
                <span>{zoomScale > 1 ? `Zoom ${zoomScale.toFixed(1)}x` : 'Magnify'}</span>
              </button>
            )}
            {!standalone && book.style === 'book' && (book.pages.length >= 1 || book.images.length > 0) && (
              <button
                type="button"
                className="book-action-btn"
                onClick={() => {
                  setViewMode((m) => (m === 'spread' ? 'scroll' : 'spread'));
                  setSpreadIndex(0);
                  setTurnDir('');
                }}
              >
                <Icon name="book" size={12} />
                <span>{canUseCodex ? 'Scroll View' : 'Open Codex'}</span>
              </button>
            )}
            {!standalone && (
              <>
                <button type="button" className="book-action-btn" onClick={cycleFontScale} title="Cycle reader typography scale">
                  <span>Text {fontScale.toUpperCase()}</span>
                </button>
                <button
                  type="button"
                  className={'book-action-btn' + (localThemeSync ? ' is-active' : '')}
                  onClick={handleThemeSyncToggle}
                  title="Toggle active SkyUltra theme colors vs classic warm parchment"
                >
                  <span>{localThemeSync ? 'Theme Sync' : 'Parchment'}</span>
                </button>
                <button
                  type="button"
                  className={'book-action-btn' + (isCurrentBookmarked ? ' is-active' : '')}
                  onClick={toggleBookmark}
                  title={isCurrentBookmarked ? 'Remove bookmark for this folio' : 'Bookmark this folio'}
                >
                  <Icon name="pin" size={12} />
                  <span>{isCurrentBookmarked ? 'Bookmarked' : 'Bookmark'}</span>
                </button>
                <button
                  type="button"
                  className={'book-action-btn' + (searchOpen ? ' is-active' : '')}
                  onClick={() => setSearchOpen((v) => !v)}
                  title="Search text inside book (Ctrl+F)"
                >
                  <Icon name="search" size={12} />
                  <span>Find</span>
                </button>
                {onOpenNative && (
                  <button
                    type="button"
                    className="book-action-btn"
                    onClick={() => {
                      onClose();
                      onOpenNative(book);
                    }}
                  >
                    <span>Open Native Book</span>
                  </button>
                )}
              </>
            )}
            <button type="button" className="book-action-btn is-close" onClick={onClose} aria-label="Close reader">
              <span>Close</span><kbd>ESC</kbd>
            </button>
          </div>
        </header>

        {!standalone && (
          <div className="inventory-book-reader__toolbar" aria-label="Reader design selector">
          <div className="inventory-book-design-pills" role="group" aria-label="Book reader designs">
            {bookReaderDesignOptions.map((opt) => (
              <button
                key={opt.value}
                type="button"
                className={'book-design-pill' + (localDesign === opt.value ? ' is-active' : '')}
                title={opt.note}
                onClick={() => handleDesignSelect(opt.value)}
              >
                {opt.label}
              </button>
            ))}
          </div>
          {searchOpen && (
            <div className="book-find-bar">
              <Icon name="search" size={13} />
              <input
                type="search"
                value={searchQuery}
                placeholder="Search words in folio..."
                aria-label="Search inside book"
                onChange={(e) => setSearchQuery(e.currentTarget.value)}
              />
              {searchQuery.trim() && (
                <div className="book-find-hits">
                  <span>{matchingPages.length} {matchingPages.length === 1 ? 'folio' : 'folios'}</span>
                  {canUseCodex && matchingPages.slice(0, 6).map((hit) => (
                    <button
                      key={hit.pageIndex}
                      type="button"
                      className="book-find-jump"
                      onClick={() => {
                        setTurnDir(hit.spreadIdx > clampedSpread ? 'next' : 'prev');
                        setSpreadIndex(hit.spreadIdx);
                      }}
                    >
                      {hit.label}
                    </button>
                  ))}
                </div>
              )}
            </div>
          )}
          </div>
        )}

        <div className="inventory-book-reader__body" tabIndex={0}>
          {canUseCodex ? (
            <div className="inventory-book-reader__codex-wrap">
              <div className="inventory-book-reader__binding">
                <button type="button" className="inventory-book-reader__turn is-prev" disabled={clampedSpread === 0} onClick={() => turn('prev')} aria-label="Previous pages">‹</button>
                <div key={clampedSpread} className={'inventory-book-reader__codex' + (turnDir ? ` is-turn-${turnDir}` : '')}>
                  {leftFolio}
                  <div className="inventory-book-reader__spine" aria-hidden="true" />
                  {rightFolio}
                </div>
                <button type="button" className="inventory-book-reader__turn is-next" disabled={clampedSpread >= totalSpreads - 1} onClick={() => turn('next')} aria-label="Next pages">›</button>
              </div>
              <div className="inventory-book-reader__pager">
                <span className="inventory-book-reader__pager-status">
                  {leafLabel(leftLeaf)}{rightLeaf ? ` · ${leafLabel(rightLeaf)}` : ''}
                  {bookmarkedSpread !== undefined && bookmarkedSpread !== clampedSpread && (
                    <button
                      type="button"
                      className="book-find-jump"
                      style={{ marginLeft: 10 }}
                      onClick={() => {
                        setTurnDir(bookmarkedSpread > clampedSpread ? 'next' : 'prev');
                        setSpreadIndex(bookmarkedSpread);
                      }}
                    >
                      Jump to Bookmark
                    </button>
                  )}
                </span>
                <div className="inventory-book-reader__ribbon" aria-hidden="true">
                  {Array.from({ length: totalSpreads }, (_, i) => (
                    <button
                      key={i}
                      type="button"
                      tabIndex={-1}
                      className={
                        (i === clampedSpread ? 'is-active' : '') +
                        (i === bookmarkedSpread ? ' is-bookmarked' : '')
                      }
                      onClick={() => {
                        setTurnDir(i > clampedSpread ? 'next' : 'prev');
                        setSpreadIndex(i);
                      }}
                    />
                  ))}
                </div>
                <span className="inventory-book-reader__pager-hint">
                  {standalone
                    ? `← → turn pages${canTake && !taken ? ` · ${activateKeyLabel} ${isStealing ? 'steal' : 'take'}` : ''} · Esc close`
                    : '← → turn pages · Ctrl+F find'}
                </span>
              </div>
            </div>
          ) : (
            <>
              {shownImages.length > 0 && (
                <div className={'inventory-book-reader__gallery' + (zoomScale > 1 ? ' is-zoomed' : '') + (isMap ? ' is-map' : '')}>
                  {shownImages.map((src, idx) => (
                    <figure
                      key={idx}
                      className="inventory-book-reader__figure"
                      onClick={() => {
                        if (zoomScale <= 1) cycleZoom();
                      }}
                    >
                      <div
                        className="inventory-book-reader__art-frame"
                        onPointerDown={(ev) => {
                          if (zoomScale <= 1 || ev.button !== 0) return;
                          panDragRef.current = {
                            id: ev.pointerId,
                            startX: ev.clientX,
                            startY: ev.clientY,
                            origX: pan.x,
                            origY: pan.y,
                          };
                          ev.currentTarget.setPointerCapture?.(ev.pointerId);
                        }}
                        onPointerMove={(ev) => {
                          const drag = panDragRef.current;
                          if (!drag || drag.id !== ev.pointerId) return;
                          pendingPanRef.current = {
                            x: drag.origX + (ev.clientX - drag.startX),
                            y: drag.origY + (ev.clientY - drag.startY),
                          };
                          if (!panRafRef.current) {
                            panRafRef.current = requestAnimationFrame(() => {
                              panRafRef.current = 0;
                              if (pendingPanRef.current) {
                                setPan(pendingPanRef.current);
                              }
                            });
                          }
                        }}
                        onPointerUp={(ev) => {
                          if (panDragRef.current?.id === ev.pointerId) {
                            panDragRef.current = null;
                            if (panRafRef.current) {
                              cancelAnimationFrame(panRafRef.current);
                              panRafRef.current = 0;
                            }
                            if (pendingPanRef.current) {
                              setPan(pendingPanRef.current);
                              pendingPanRef.current = null;
                            }
                            ev.currentTarget.releasePointerCapture?.(ev.pointerId);
                          }
                        }}
                      >
                        <img
                          src={src}
                          alt={`${book.title} plate ${idx + 1}`}
                          style={zoomScale > 1 ? { transform: `translate(${pan.x}px, ${pan.y}px) scale(${zoomScale})`, transformOrigin: 'center center' } : undefined}
                        />
                      </div>
                      <figcaption>
                        <span>{isMap ? `${book.title} · Cartographic Plate` : `Illuminated Plate ${idx + 1}`}</span>
                        <small>{zoomScale > 1 ? 'Drag to pan · click Magnify to reset' : 'Click plate to zoom'}</small>
                      </figcaption>
                    </figure>
                  ))}
                </div>
              )}
              {(hasRealText || shownImages.length === 0) && (
                <article className={`inventory-book-parchment inventory-book-parchment--${book.style}`}>
                  <div className="inventory-book-parchment__crest" aria-hidden="true">
                    <span />
                  </div>
                  {scrolledPages}
                  <div className="inventory-book-parchment__seal" aria-hidden="true">
                    <span>SKYULTRA ARCHIVES · {ownerDisplay.toUpperCase()}</span>
                  </div>
                </article>
              )}
            </>
          )}
        </div>
      </div>
    </div>
  );
}

export default BookReader;
