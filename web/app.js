import createVeil from './dist/veil.mjs';

const $ = (id) => document.getElementById(id);

const els = {
    status: $('status'),
    main: $('main'),
    log: $('log'),
    verbose: $('verbose'),
    logClear: $('log-clear'),
    preview: $('preview'),
    previewImg: $('preview-img'),
    previewMeta: $('preview-meta'),
    hide: {
        carrier: $('hide-carrier'),
        payload: $('hide-payload'),
        codec: $('hide-codec'),
        codecHint: $('hide-codec-hint'),
        pass: $('hide-pass'),
        go: $('hide-go'),
        result: $('hide-result'),
    },
    reveal: {
        carrier: $('reveal-carrier'),
        pass: $('reveal-pass'),
        go: $('reveal-go'),
        result: $('reveal-result'),
    },
};

/* ---------------------------------------------------------------- logging */

function write(line, kind) {
    const row = document.createElement('span');
    row.textContent = line + '\n';
    if (kind) row.className = kind;
    els.log.append(row);
    els.log.scrollTop = els.log.scrollHeight;
}

/* The core tags its output: [+] info, [!] warning, [-] error. */
function classify(line) {
    if (line.startsWith('[-]')) return 'err';
    if (line.startsWith('[!]')) return 'warn';
    if (line.startsWith('[+]')) return 'info';
    return null;
}

els.logClear.addEventListener('click', () => { els.log.textContent = ''; });

/* ------------------------------------------------------------ module init */

let Module;

try {
    Module = await createVeil({
        print: (line) => write(line, classify(line)),
        printErr: (line) => write(line, classify(line) ?? 'err'),
    });
} catch (err) {
    els.status.textContent = 'module failed to load';
    els.status.className = 'status status--bad';
    write(String(err), 'err');
    throw err;
}

if (Module.ccall('veil_init', 'number', [], []) !== 0) {
    els.status.textContent = 'libsodium init failed';
    els.status.className = 'status status--bad';
    throw new Error('veil_init failed');
}

els.status.textContent = 'ready';
els.status.className = 'status status--ready';
els.main.hidden = false;
write('module ready', 'info');

els.verbose.addEventListener('change', () => {
    Module.ccall('veil_set_verbose', null, ['number'], [els.verbose.checked ? 1 : 0]);
    write('verbose ' + (els.verbose.checked ? 'on' : 'off'));
});

/*
 * Every entry point returns a malloc'd JSON string that we own. Read it, hand
 * the pointer straight back, and let a null pointer (allocation failure inside
 * cJSON) surface as an error rather than as an empty parse.
 */
function call(fn, types, args) {
    const ptr = Module.ccall(fn, 'number', types, args);
    if (!ptr) return { ok: false, error: fn + ' returned no result' };

    try {
        return JSON.parse(Module.UTF8ToString(ptr));
    } finally {
        Module.ccall('veil_free', null, ['number'], [ptr]);
    }
}

/* ------------------------------------------------------------------- MEMFS */

let counter = 0;

/* Unique names keep repeated runs from reading a previous run's leftovers. */
const scratch = (name) => `/w${counter++}-${name}`;

async function put(path, file) {
    Module.FS.writeFile(path, new Uint8Array(await file.arrayBuffer()));
    return path;
}

function drop(...paths) {
    for (const path of paths) {
        try {
            Module.FS.unlink(path);
        } catch {
            /* never written, or already gone */
        }
    }
}

/* ------------------------------------------------------------------ output */

const units = ['B', 'KB', 'MB'];

function human(bytes) {
    let size = bytes;
    let unit = 0;
    while (size >= 1024 && unit < units.length - 1) {
        size /= 1024;
        unit++;
    }
    return (unit === 0 ? size : size.toFixed(1)) + ' ' + units[unit];
}

function fail(box, message) {
    box.hidden = false;
    box.className = 'result result--bad';
    box.textContent = message;
}

/*
 * Object URLs are revoked when the panel is next used: the anchor has to stay
 * live until the user actually clicks it.
 */
const urls = new WeakMap();

function reset(box) {
    const stale = urls.get(box);
    if (stale) URL.revokeObjectURL(stale);
    urls.delete(box);
    box.hidden = true;
    box.textContent = '';
}

/*
 * One card per result: a head line, optional facts, an inline preview when the
 * payload is readable text, and the download. Everything is a child of the same
 * card so nothing renders as a box inside a box.
 */
function succeed(box, { title, meta, facts = [], bytes, filename, mime }) {
    const url = URL.createObjectURL(new Blob([bytes], { type: mime }));
    urls.set(box, url);

    const head = document.createElement('div');
    head.className = 'card-head';

    const name = document.createElement('span');
    name.className = 'card-title';
    name.textContent = title;

    const size = document.createElement('span');
    size.className = 'card-meta';
    size.textContent = meta;

    head.append(name, size);

    const parts = [head];

    if (facts.length) {
        const list = document.createElement('dl');
        list.className = 'facts';

        for (const [key, value] of facts) {
            const row = document.createElement('div');
            const dt = document.createElement('dt');
            dt.textContent = key;
            const dd = document.createElement('dd');
            dd.textContent = value;
            row.append(dt, dd);
            list.append(row);
        }

        parts.push(list);
    }

    const text = asText(bytes);
    if (text !== null) {
        const pre = document.createElement('pre');
        pre.className = 'payload';
        pre.textContent = text;
        parts.push(pre);
    }

    const foot = document.createElement('div');
    foot.className = 'card-foot';

    const link = document.createElement('a');
    link.className = 'download';
    link.href = url;
    link.download = filename;
    link.textContent = 'Download ' + filename;
    foot.append(link);

    parts.push(foot);

    box.hidden = false;
    box.className = 'result result--ok';
    box.replaceChildren(...parts);
}

/*
 * A hidden file is very often just text, so show it inline when it decodes
 * cleanly rather than making the user download it to find out what it was.
 */
function asText(bytes) {
    try {
        return new TextDecoder('utf-8', { fatal: true }).decode(bytes);
    } catch {
        return null;
    }
}

/* ----------------------------------------------------------------- preview */

let previewUrl = null;

function showPreview(file) {
    if (previewUrl) URL.revokeObjectURL(previewUrl);

    if (!file) {
        previewUrl = null;
        els.preview.hidden = true;
        return;
    }

    previewUrl = URL.createObjectURL(file);
    els.previewImg.src = previewUrl;
    els.preview.hidden = false;

    els.previewImg.onload = () => {
        const { naturalWidth: w, naturalHeight: h } = els.previewImg;
        els.previewMeta.textContent = `${file.name} — ${w}×${h}, ${human(file.size)}`;
    };
}

/* -------------------------------------------------------------------- tabs */

const panels = [
    { tab: $('tab-hide'), panel: $('panel-hide'), input: els.hide.carrier },
    { tab: $('tab-reveal'), panel: $('panel-reveal'), input: els.reveal.carrier },
];

for (const entry of panels) {
    entry.tab.addEventListener('click', () => {
        for (const other of panels) {
            const on = other === entry;
            other.tab.classList.toggle('tab--on', on);
            other.tab.setAttribute('aria-selected', String(on));
            other.panel.hidden = !on;
        }
        showPreview(entry.input.files[0] ?? null);
    });
}

/* -------------------------------------------------------------------- hide */

const isJpeg = (file) => file && /^image\/jpe?g$/.test(file.type);

/*
 * The core forces DCT on a JPEG carrier and rejects DCT on a PNG, so mirror
 * that in the form instead of letting the user pick a losing combination.
 */
function syncCodec() {
    const file = els.hide.carrier.files[0];
    const jpeg = isJpeg(file);

    els.hide.codec.disabled = jpeg;
    if (jpeg) els.hide.codec.value = 'dct';
    else if (els.hide.codec.value === 'dct') els.hide.codec.value = 'lsbm';

    els.hide.codecHint.textContent = jpeg
        ? 'JPEG carrier: DCT is the only option.'
        : 'LSB codecs need a PNG carrier; JPEG always uses DCT.';
}

function syncHide() {
    els.hide.go.disabled = !(els.hide.carrier.files[0] && els.hide.payload.files[0]);
}

els.hide.carrier.addEventListener('change', () => {
    showPreview(els.hide.carrier.files[0] ?? null);
    syncCodec();
    syncHide();
});

els.hide.payload.addEventListener('change', syncHide);

els.hide.go.addEventListener('click', async () => {
    const carrier = els.hide.carrier.files[0];
    const payload = els.hide.payload.files[0];
    reset(els.hide.result);

    const jpeg = isJpeg(carrier);
    const ext = jpeg ? '.jpg' : '.png';

    const target = scratch('carrier' + ext);
    const data = scratch('payload.bin');
    const output = scratch('out' + ext);

    els.hide.go.disabled = true;
    els.hide.go.textContent = 'Hiding…';

    try {
        await put(target, carrier);
        await put(data, payload);

        const result = call(
            'veil_encode',
            ['string', 'string', 'string', 'string', 'string'],
            [target, data, output, els.hide.codec.value, els.hide.pass.value],
        );

        if (!result.ok) {
            fail(els.hide.result, result.error);
            return;
        }

        const bytes = Module.FS.readFile(output);
        const name = carrier.name.replace(/(\.[^.]+)?$/, '') + '-veiled' + ext;

        succeed(els.hide.result, {
            title: 'Payload hidden',
            meta: human(bytes.length),
            facts: [
                ['payload', human(result.bytes)],
                ['codec', els.hide.codec.value],
                ['encrypted', result.encrypted ? 'yes' : 'no'],
            ],
            bytes,
            filename: name,
            mime: jpeg ? 'image/jpeg' : 'image/png',
        });
    } catch (err) {
        fail(els.hide.result, String(err));
        write(String(err), 'err');
    } finally {
        drop(target, data, output);
        els.hide.go.disabled = false;
        els.hide.go.textContent = 'Hide payload';
        syncHide();
    }
});

/* ------------------------------------------------------------------ reveal */

els.reveal.carrier.addEventListener('change', () => {
    showPreview(els.reveal.carrier.files[0] ?? null);
    els.reveal.go.disabled = !els.reveal.carrier.files[0];
});

els.reveal.go.addEventListener('click', async () => {
    const carrier = els.reveal.carrier.files[0];
    reset(els.reveal.result);

    const ext = isJpeg(carrier) ? '.jpg' : '.png';
    const target = scratch('encoded' + ext);
    const output = scratch('payload.bin');

    els.reveal.go.disabled = true;
    els.reveal.go.textContent = 'Revealing…';

    try {
        await put(target, carrier);

        const result = call(
            'veil_decode',
            ['string', 'string', 'string'],
            [target, output, els.reveal.pass.value],
        );

        if (!result.ok) {
            fail(els.reveal.result, result.error);
            return;
        }

        const bytes = Module.FS.readFile(output);

        const text = asText(bytes);

        succeed(els.reveal.result, {
            title: 'Payload revealed',
            meta: human(result.bytes) + (text === null ? ' \u00b7 binary' : ' \u00b7 text'),
            bytes,
            filename: text === null ? 'revealed.bin' : 'revealed.txt',
            mime: text === null ? 'application/octet-stream' : 'text/plain',
        });
    } catch (err) {
        fail(els.reveal.result, String(err));
        write(String(err), 'err');
    } finally {
        drop(target, output);
        els.reveal.go.disabled = false;
        els.reveal.go.textContent = 'Reveal payload';
    }
});
