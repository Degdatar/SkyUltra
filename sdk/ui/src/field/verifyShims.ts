import type { SkyUltraInstalledShims } from '@skyultra/sdk';

export interface ShimVerification {
  framework: 'PrismaUI' | 'MagelightUI';
  passed: boolean;
  message: string;
}

/** Check the installed API and the host's same-view interop echo, not gameplay. */
export async function verifyActiveShims(
  target: Partial<SkyUltraInstalledShims>, signal?: AbortSignal, timeoutMs = 3000,
): Promise<ShimVerification[]> {
  const probe = 'studio-' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2);
  const check = (framework: ShimVerification['framework']): Promise<ShimVerification> => {
    const api = framework === 'PrismaUI' ? target.prismaUI : target.magelight;
    const listen = framework === 'PrismaUI' ? target.prismaUI?.RegisterJSListener : target.magelight?.on;
    const send = framework === 'PrismaUI' ? target.prismaUI?.InteropCall : target.magelight?.send;
    if (!api || typeof listen !== 'function' || typeof send !== 'function') {
      return Promise.resolve({ framework, passed: false, message: 'Not installed or missing interop methods.' });
    }
    return new Promise(resolve => {
      let finished = false;
      let off: (() => void) | undefined;
      const finish = (passed: boolean, message: string): void => {
        if (finished) return;
        finished = true;
        clearTimeout(timer);
        off?.();
        signal?.removeEventListener('abort', abort);
        resolve({ framework, passed, message });
      };
      const abort = (): void => finish(false, 'Verification cancelled.');
      const timer = setTimeout(() => finish(false, 'No host echo received. Check ui.interopCall permission and bridge errors.'), timeoutMs);
      signal?.addEventListener('abort', abort, { once: true });
      if (signal?.aborted) { abort(); return; }
      const method = 'skyultra.studio.verify.' + probe + '.' + framework;
      try {
        off = listen.call(api, method, value => {
          try {
            const payload = typeof value === 'string' ? JSON.parse(value) : value;
            if (payload?.probe === probe) finish(true, 'API present; host request and listener echo passed.');
          } catch { /* Ignore unrelated or malformed events. */ }
        });
        void Promise.resolve(send.call(api, method, { probe })).catch(error => {
          finish(false, error instanceof Error ? error.message : String(error));
        });
      } catch (error) {
        finish(false, error instanceof Error ? error.message : String(error));
      }
    });
  };
  return Promise.all([check('PrismaUI'), check('MagelightUI')]);
}
